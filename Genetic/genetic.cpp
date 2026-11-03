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
        individual.genes.push_back(distribution(rng)); // assigns 0 or 1 randomly at this cell location.
    }

    repair_individual(individual, map, rng); // start fully lit

    return individual;
}

void repair_individual(Individual& individual, const GeneMap& map, std::mt19937& rng) {
    const size_t num_genes = individual.genes.size();

    // count the bulbs in every row and column segment.
    std::vector<int> bulbs_in_segment(map.num_segments, 0);
    for (size_t gene = 0; gene < num_genes; gene++) {
        if (individual.genes[gene]) {
            bulbs_in_segment[map.row_segment[gene]]++;
            bulbs_in_segment[map.col_segment[gene]]++;
        }
    }

    // visit cells in random order so repeated repairs don't all favor the top-left of the grid.
    std::vector<int> order(num_genes);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    for (int gene : order) {
        int& row_bulbs = bulbs_in_segment[map.row_segment[gene]];
        int& col_bulbs = bulbs_in_segment[map.col_segment[gene]];

        if (row_bulbs == 0 && col_bulbs == 0) {
            // unlit. both its segments are empty, so a bulb here can't see any other bulb.
            // it also lights its whole row and column segment, which may fix other unlit cells.
            individual.genes[gene] = 1;
            row_bulbs++;
            col_bulbs++;
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

    return map;
}

void mutate_individual(Individual& individual, std::mt19937& rng) {
    std::uniform_int_distribution<size_t> distribution(0, individual.genes.size() - 1);
    individual.genes.at(distribution(rng)) ^= 1; // bit flip if the int is 0/1
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

    Evaluation result{0, 0};

    for (size_t gene = 0; gene < num_genes; gene++) {
        int row_bulbs = bulbs_in_segment[map.row_segment[gene]];
        int col_bulbs = bulbs_in_segment[map.col_segment[gene]];

        if (individual.genes[gene]) {
            // a bulb sees another bulb if its row or column segment has more than just itself.
            // counted once per bulb, even if it conflicts in both directions (same as the verifier).
            if (row_bulbs >= 2 || col_bulbs >= 2) result.violations++;
        } else if (row_bulbs == 0 && col_bulbs == 0) {
            // no bulb anywhere in this cell's row or column segment.
            result.unlit++;
        }
    }

    // a numbered wall is a violation when its adjacent bulb count doesn't match its number.
    for (const NumberedWall& wall : map.numbered_walls) {
        int bulbs = 0;
        for (int gene : wall.neighbors) bulbs += individual.genes[gene];
        if (bulbs != wall.required) result.violations++;
    }

    return result;
}

int64_t compute_fitness(const Individual& individual, const GeneMap& map) {
    // the most violations possible is every bulb plus every numbered wall, so this penalty outweighs all of them.
    const int64_t unlit_penalty = static_cast<int64_t>(map.cell_map.size() + map.numbered_walls.size()) + 1;

    Evaluation e = evaluate(individual, map);
    return e.violations + unlit_penalty * e.unlit;
}
