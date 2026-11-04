#include "genetic.h"
#include <algorithm>
#include <chrono>
#include <numeric>

Individual make_individual(const GeneMap& map, std::mt19937& rng, double bulb_probability) {
    Individual individual;
    individual.genes.reserve(map.cell_map.size());
    individual.fitness = 0; // initialize fitness to 0

    std::bernoulli_distribution distribution(bulb_probability);
    
    for (size_t i = 0; i < map.cell_map.size(); i++) {
        if (map.fixed[i] != GeneMap::FREE) individual.genes.push_back(map.fixed[i]); // the walls decide this one
        else individual.genes.push_back(distribution(rng)); // assigns 0 or 1 randomly at this cell location.
    }

    repair_individual(individual, map, rng); // start fully lit

    return individual;
}

void repair_individual(Individual& individual, const GeneMap& map, std::mt19937& rng) {
    const size_t num_genes = individual.genes.size();

    // count the bulbs in every row and column segment, and next to every numbered wall.
    std::vector<int> bulbs_in_segment(map.num_segments, 0);
    std::vector<int> bulbs_at_wall(map.numbered_walls.size(), 0);
    for (size_t gene = 0; gene < num_genes; gene++) {
        if (individual.genes[gene]) {
            bulbs_in_segment[map.row_segment[gene]]++;
            bulbs_in_segment[map.col_segment[gene]]++;
            for (int w : map.gene_walls[gene]) bulbs_at_wall[w]++;
        }
    }

    // how much worse a bulb at this gene makes the numbered walls (negative = it helps).
    // FORBIDDEN cells cost more than any wall effect so they're only used when nothing else can light the cell.
    auto placement_cost = [&](int gene) {
        int cost = (map.fixed[gene] == GeneMap::FORBIDDEN) ? 100 : 0;
        for (int w : map.gene_walls[gene]) {
            cost += (bulbs_at_wall[w] < map.numbered_walls[w].required) ? -1 : 1;
        }
        return cost;
    };

    // visit cells in random order so repeated repairs don't all favor the top-left of the grid.
    std::vector<int> order(num_genes);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    for (int gene : order) {
        if (bulbs_in_segment[map.row_segment[gene]] || bulbs_in_segment[map.col_segment[gene]]) continue; // lit

        // unlit. any cell in its row or column segment with both segments empty can light it
        // without seeing another bulb (the cell itself always qualifies). pick the cheapest, ties at random.
        int best = gene;
        int best_cost = placement_cost(gene);
        int ties = 1;
        for (int segment : {map.row_segment[gene], map.col_segment[gene]}) {
            for (int candidate : map.segment_cells[segment]) {
                if (candidate == gene) continue;
                if (bulbs_in_segment[map.row_segment[candidate]] || bulbs_in_segment[map.col_segment[candidate]]) continue;
                int cost = placement_cost(candidate);
                if (cost < best_cost) {
                    best = candidate; best_cost = cost; ties = 1;
                } else if (cost == best_cost && std::uniform_int_distribution<int>(0, ties++)(rng) == 0) {
                    best = candidate; // reservoir sampling keeps the tie-break uniform
                }
            }
        }

        individual.genes[best] = 1;
        bulbs_in_segment[map.row_segment[best]]++;
        bulbs_in_segment[map.col_segment[best]]++;
        for (int w : map.gene_walls[best]) bulbs_at_wall[w]++;
    }
}

// decide the genes the numbered walls leave no choice about, repeating until nothing changes:
//  - a wall that already has `required` FORCED neighbors forbids its other FREE neighbors (this covers '0' walls)
//  - a wall whose FORCED + FREE neighbors only just reach `required` forces all its FREE neighbors
//  - a FORCED bulb forbids every FREE cell in its row and column segments (they would see it)
// these only follow from the rules, so a solvable puzzle keeps its solutions. if a grid is contradictory
// the rules simply stop at the contradiction and the GA handles the rest.
static void propagate_fixed(GeneMap& map) {
    map.fixed.assign(map.cell_map.size(), GeneMap::FREE);

    auto force = [&](int gene) {
        map.fixed[gene] = GeneMap::FORCED;
        for (int segment : {map.row_segment[gene], map.col_segment[gene]}) {
            for (int other : map.segment_cells[segment]) {
                if (map.fixed[other] == GeneMap::FREE) map.fixed[other] = GeneMap::FORBIDDEN;
            }
        }
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (const NumberedWall& wall : map.numbered_walls) {
            int forced = 0, free = 0;
            for (int gene : wall.neighbors) {
                if (map.fixed[gene] == GeneMap::FORCED) forced++;
                else if (map.fixed[gene] == GeneMap::FREE) free++;
            }
            if (free == 0) continue;

            if (forced >= wall.required) {
                for (int gene : wall.neighbors) {
                    if (map.fixed[gene] == GeneMap::FREE) map.fixed[gene] = GeneMap::FORBIDDEN;
                }
                changed = true;
            } else if (forced + free == wall.required) {
                for (int gene : wall.neighbors) {
                    if (map.fixed[gene] == GeneMap::FREE) force(gene);
                }
                changed = true;
            }
        }
    }
}

GeneMap make_gene_map(const Problem& problem) {
    GeneMap map;

    // number the open cells.
    // gene_at[row][col] = gene index of an open cell, or -1 for a wall
    std::vector<std::vector<int>> gene_at(problem.num_rows, std::vector<int>(problem.num_cols, -1)); 

    int next_gene_index = 0;
    for (int row = 0; row < problem.num_rows; row++) {
        for (int col = 0; col < problem.num_cols; col++) {
            if (!is_wall(problem.grid.at(row).at(col))) {
                map.cell_map.emplace_back(row, col);
                gene_at[row][col] = next_gene_index++;
            }
        }
    }

    map.row_segment.assign(map.cell_map.size(), -1); // -1 represents an unassigned segment for this cell.
    map.col_segment.assign(map.cell_map.size(), -1);

    // each open cell can light up a given row segment up until it reaches a wall. 
    for (int row = 0; row < problem.num_rows; row++) {
        bool in_segment = false;
        for (int col = 0; col < problem.num_cols; col++) {
            int gene = gene_at[row][col];
            if (gene == -1) { // wall ends the current segment
                in_segment = false;
                continue; // go to the next gene.
            }
            if (!in_segment) { // first open cell of a new segment
                map.num_segments++;
                in_segment = true;
            }
            map.row_segment[gene] = map.num_segments - 1; // continually map the same segment until a wall is met.
        }
    }

    // each open cell can light up a given column segment up until it reaches a wall.
    for (int col = 0; col < problem.num_cols; col++) {
        bool in_segment = false;
        for (int row = 0; row < problem.num_rows; row++) {
            int gene = gene_at[row][col];
            if (gene == -1) {
                in_segment = false;
                continue; // go to the next gene.
            }
            if (!in_segment) {
                map.num_segments++;
                in_segment = true;
            }
            map.col_segment[gene] = map.num_segments - 1;
        }
    }

    // record each wall's required count and the genes next to it.
    const int d_row[4] = {-1, 1, 0, 0}; // up, down, left, right
    const int d_col[4] = {0, 0, -1, 1};

    for (int row = 0; row < problem.num_rows; row++) {
        for (int col = 0; col < problem.num_cols; col++) {
            char cell = problem.grid.at(row).at(col);
            if (cell < '0' || cell > '4') continue; // only numbered walls

            NumberedWall wall;
            wall.required = cell - '0'; // '0'-'0' = 0, '1'-'0' = 1, ... 
            
            // If the adjacent neighbors are a gene, then we can place a light there, so store it in the wall's neighbors.
            for (int d = 0; d < 4; d++) {
                int n_row = row + d_row[d];
                int n_col = col + d_col[d];
                if (n_row < 0 || n_row >= problem.num_rows || n_col < 0 || n_col >= problem.num_cols) continue; // off the grid
                int gene = gene_at[n_row][n_col];
                if (gene != -1) { // neighbor is an open cell, push it.
                    wall.neighbors.push_back(gene);
                }
            }
            map.numbered_walls.push_back(wall);
        }
    }

    map.segment_cells.assign(map.num_segments, {});
    map.gene_walls.assign(map.cell_map.size(), {});
    for (size_t gene = 0; gene < map.cell_map.size(); gene++) {
        map.segment_cells[map.row_segment[gene]].push_back(gene);
        map.segment_cells[map.col_segment[gene]].push_back(gene);
    }
    for (size_t w = 0; w < map.numbered_walls.size(); w++) {
        for (int gene : map.numbered_walls[w].neighbors) map.gene_walls[gene].push_back(w);
    }

    propagate_fixed(map);
    for (size_t gene = 0; gene < map.cell_map.size(); gene++) {
        if (map.fixed[gene] == GeneMap::FREE) map.free_genes.push_back(gene);
    }

    return map;
}

void mutate_individual(Individual& individual, const GeneMap& map, std::mt19937& rng) {
    if (map.free_genes.empty()) return;
    std::uniform_int_distribution<size_t> distribution(0, map.free_genes.size() - 1);
    individual.genes[map.free_genes[distribution(rng)]] ^= 1; // bit flip if the int is 0/1
}

bool fix_wall_mutation(Individual& individual, const GeneMap& map, std::mt19937& rng) {
    // walls with the wrong count, and how many bulbs each has
    std::vector<std::pair<int,int>> wrong; // (wall index, bulbs next to it)
    for (size_t w = 0; w < map.numbered_walls.size(); w++) {
        int bulbs = 0;
        for (int gene : map.numbered_walls[w].neighbors) bulbs += individual.genes[gene];
        if (bulbs != map.numbered_walls[w].required) wrong.emplace_back(w, bulbs);
    }
    if (wrong.empty()) return false;

    const auto [w, bulbs] = wrong[std::uniform_int_distribution<size_t>(0, wrong.size() - 1)(rng)];
    const NumberedWall& wall = map.numbered_walls[w];
    const bool add = bulbs < wall.required;

    // neighbors we're allowed to change: empty and not FORBIDDEN when adding, bulbs and not FORCED when removing
    std::vector<int> options;
    for (int gene : wall.neighbors) {
        if (add ? (!individual.genes[gene] && map.fixed[gene] != GeneMap::FORBIDDEN)
                : (individual.genes[gene] && map.fixed[gene] != GeneMap::FORCED)) {
            options.push_back(gene);
        }
    }
    if (options.empty()) return false;
    const int gene = options[std::uniform_int_distribution<size_t>(0, options.size() - 1)(rng)];

    if (add) {
        // clear the bulbs the new one would see, so it doesn't just trade a wall violation for conflicts
        for (int segment : {map.row_segment[gene], map.col_segment[gene]}) {
            for (int other : map.segment_cells[segment]) {
                if (map.fixed[other] != GeneMap::FORCED) individual.genes[other] = 0;
            }
        }
        individual.genes[gene] = 1;
    } else {
        individual.genes[gene] = 0;
    }
    return true;
}

std::pair<Individual, Individual> make_children(const std::pair<Individual, Individual>& parents, std::mt19937& rng) {
    const Individual parent_1 = parents.first;
    const Individual parent_2 = parents.second;
    const size_t n = parent_1.genes.size();

    std::uniform_int_distribution<size_t> distribution(0, n - 1);
    size_t crossover_index = distribution(rng);
    size_t split = crossover_index + 1; // [0, split) from one parent, [split, n) from the other

    Individual child_1;
    Individual child_2;
    child_1.fitness=0;
    child_2.fitness=0;
    child_1.genes.reserve(n);
    child_2.genes.reserve(n);

    // child 1: parent_1.genes[0 : crossover_index+1] + parent_2.genes[crossover_index+1 : n]
    child_1.genes.insert(child_1.genes.end(), parent_1.genes.begin(), parent_1.genes.begin() + split);
    child_1.genes.insert(child_1.genes.end(), parent_2.genes.begin() + split, parent_2.genes.end());

    // child 2: parent_2.genes[0 : crossover_index+1] + parent_1.genes[crossover_index+1 : n]
    child_2.genes.insert(child_2.genes.end(), parent_2.genes.begin(), parent_2.genes.begin() + split);
    child_2.genes.insert(child_2.genes.end(), parent_1.genes.begin() + split, parent_1.genes.end());

    return {child_1, child_2};
}


Evaluation evaluate(const Individual& individual, const GeneMap& map) {
    const size_t num_genes = individual.genes.size();

    // count the bulbs in every row and column segment.
    std::vector<int> bulbs_in_segment(map.num_segments, 0);
    for (size_t gene = 0; gene < num_genes; gene++) {
        if (individual.genes[gene]) {
            bulbs_in_segment[map.row_segment[gene]]++;
            bulbs_in_segment[map.col_segment[gene]]++;
        }
    }

    Evaluation result{0, 0, 0, 0};

    for (size_t gene = 0; gene < num_genes; gene++) {
        int row_bulbs = bulbs_in_segment[map.row_segment[gene]];
        int col_bulbs = bulbs_in_segment[map.col_segment[gene]];

        if (individual.genes[gene]) {
            // a bulb sees another bulb if its row or column segment has more than just itself.
            // counted once per bulb, even if it conflicts in both directions (same as the verifier).
            if (row_bulbs >= 2 || col_bulbs >= 2) result.bulb_conflicts++;
        } else if (row_bulbs == 0 && col_bulbs == 0) {
            // no bulb anywhere in this cell's row or column segment.
            result.unlit++;
        }
    }

    // a numbered wall is a violation when its adjacent bulb count doesn't match its number.
    for (const NumberedWall& wall : map.numbered_walls) {
        int bulbs = 0;
        for (int gene : wall.neighbors) bulbs += individual.genes[gene];
        if (bulbs != wall.required) result.wall_violations++;
    }

    result.violations = result.bulb_conflicts + result.wall_violations;
    return result;
}

int64_t compute_fitness(const Individual& individual, const GeneMap& map) {
    // the most violations possible is every bulb plus every numbered wall, so this penalty outweighs all of them.
    const int64_t unlit_penalty = static_cast<int64_t>(map.cell_map.size() + map.numbered_walls.size()) + 1;

    Evaluation e = evaluate(individual, map);
    return e.violations + unlit_penalty * e.unlit;
}

static int random_index(size_t size, std::mt19937& rng) {
    return static_cast<int>(std::uniform_int_distribution<size_t>(0, size - 1)(rng));
}

Population make_population(const GeneMap& map, int population_size, std::mt19937& rng) {
    Population population;
    population.individuals.reserve(population_size);
    for (int i = 0; i < population_size; i++) {
        Individual individual = make_individual(map, rng);
        individual.fitness = compute_fitness(individual, map);
        population.individuals.push_back(std::move(individual));
    }
    return population;
}

const Individual& tournament_select(const Population& population, int tournament_size, std::mt19937& rng) {
    const auto& individuals = population.individuals;
    const Individual* best = &individuals[random_index(individuals.size(), rng)];
    for (int i = 1; i < tournament_size; i++) {
        const Individual* challenger = &individuals[random_index(individuals.size(), rng)];
        if (challenger->fitness < best->fitness) best = challenger;
    }
    return *best;
}

const Individual& best_individual(const Population& population) {
    return *std::min_element(population.individuals.begin(), population.individuals.end(),
                             [](const Individual& a, const Individual& b) { return a.fitness < b.fitness; });
}

Population next_generation(const Population& population, const GeneMap& map,
                           int tournament_size, double mutation_rate, double wall_mutation_share,
                           std::mt19937& rng) {
    const size_t population_size = population.individuals.size();

    Population next;
    next.individuals.reserve(population_size);
    next.individuals.push_back(best_individual(population)); // elitism: the best always survives

    // mutate (sometimes), repair, score, and add a child to the next generation
    auto add_child = [&](Individual& child) {
        if (next.individuals.size() >= population_size) return;
        if (std::bernoulli_distribution(mutation_rate)(rng)) {
            // fall back to a random flip when every wall is already right
            if (!std::bernoulli_distribution(wall_mutation_share)(rng) || !fix_wall_mutation(child, map, rng)) {
                mutate_individual(child, map, rng);
            }
        }
        repair_individual(child, map, rng);
        child.fitness = compute_fitness(child, map);
        next.individuals.push_back(std::move(child));
    };

    while (next.individuals.size() < population_size) {
        const Individual& parent_a = tournament_select(population, tournament_size, rng);
        const Individual& parent_b = tournament_select(population, tournament_size, rng);
        auto [child_a, child_b] = make_children({parent_a, parent_b}, rng);
        add_child(child_a);
        add_child(child_b);
    }
    return next;
}

Solution individual_to_solution(const Individual& individual, const GeneMap& map, const Problem& problem) {
    Solution solution;
    solution.problem = problem;
    solution.grid = problem.grid;
    for (size_t gene = 0; gene < individual.genes.size(); gene++) {
        const auto [row, col] = map.cell_map[gene];
        if (individual.genes[gene]) solution.grid[row][col] = 'L';
    }
    solution.violations = evaluate(individual, map).violations;
    return solution;
}
