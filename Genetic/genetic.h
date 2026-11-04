#ifndef GENETIC_H
#define GENETIC_H

#include <cstdint>
#include <random>
#include <vector>

#include "akari_io.h"

struct Individual {
    // an individual's gene vector is a (n*m) length string of 0s & 1s representing grid locations for light placement
    // keep track of original location by using the original rows/cols
    std::vector<uint8_t> genes;
    int64_t fitness=0; // lower is better, see compute_fitness. 64-bit because the unlit penalty can pass INT_MAX on big grids.
};

struct NumberedWall {
    int required;               // number of adjacent bulbs the wall asks for, 0-4
    std::vector<int> neighbors; // genes of the open cells orthogonally adjacent to the wall (at most 4)
};

struct GeneMap {
    std::vector<std::pair<int,int>> cell_map; // each index of cells represents it's original row/col.

    // a segment is a run of open cells in one row or column, bounded by walls or the grid edge.
    // a bulb lights its whole row segment and column segment.
    // row and column segments share one id space: row ids come first, column ids continue after.
    std::vector<int> row_segment; // row_segment[gene] = id of the gene's row segment
    std::vector<int> col_segment; // col_segment[gene] = id of the gene's column segment
    int num_segments = 0;

    std::vector<NumberedWall> numbered_walls; // every '0'-'4' wall in the grid

    std::vector<std::vector<int>> segment_cells; // segment_cells[segment id] = genes in that segment
    std::vector<std::vector<int>> gene_walls;    // gene_walls[gene] = indexes into numbered_walls of the walls next to it

    // values the numbered walls force, found by propagation in make_gene_map:
    // FREE = undecided, FORBIDDEN = never a bulb, FORCED = always a bulb
    static constexpr int8_t FREE = -1, FORBIDDEN = 0, FORCED = 1;
    std::vector<int8_t> fixed;   // fixed[gene] = FREE, FORBIDDEN or FORCED
    std::vector<int> free_genes; // genes with fixed == FREE, the only ones random mutation flips
};

struct Population {
    std::vector<Individual> individuals;
};

// true if the cell is a wall: numbered ('0'-'4') or unnumbered ('X')
inline bool is_wall(char cell) {
    return (cell >= '0' && cell <= '4') || cell == 'X';
}

Individual make_individual(const GeneMap& map, std::mt19937& rng, double bulb_probability = 0.005);

// light every unlit cell, visiting cells in random order. each unlit cell gets a new bulb somewhere in
// its row or column segment, on a cell that can't see another bulb, chosen to help under-filled
// numbered walls and avoid overfilling full ones (FORBIDDEN cells only as a last resort).
// afterwards the individual has 0 unlit cells and the new bulbs never see another bulb.
void repair_individual(Individual& individual, const GeneMap& map, std::mt19937& rng);

// mutate an individual's genes: flip one random FREE gene
void mutate_individual(Individual& individual, const GeneMap& map, std::mt19937& rng);

// pick a random numbered wall with the wrong count and push it toward its number:
// too few bulbs -> add one next to it (and remove the bulbs it would see), too many -> remove one.
// may leave cells unlit, so repair afterwards. returns false if every wall is already satisfied.
bool fix_wall_mutation(Individual& individual, const GeneMap& map, std::mt19937& rng);

// make two children from two parents
std::pair<Individual, Individual> make_children(const std::pair<Individual, Individual>& parents, std::mt19937& rng);

// the two quantities the fitness is built from
struct Evaluation {
    int violations;      // cells the verifier would flag: bulb_conflicts + wall_violations
    int unlit;           // open cells with no bulb in their row or column segment (any > 0 makes the output invalid)
    int bulb_conflicts;  // bulbs that see another bulb
    int wall_violations; // numbered walls with the wrong count
};

// count violations and unlit cells for an individual
Evaluation evaluate(const Individual& individual, const GeneMap& map);

// fitness to minimize: violations + penalty * unlit, where the penalty is larger than any
// possible violation count, so lighting one more cell always beats removing every violation
int64_t compute_fitness(const Individual& individual, const GeneMap& map);

// produce the valid map of gene cells for placing lights
GeneMap make_gene_map(const Problem& problem);

// make population_size random individuals, repaired and scored
Population make_population(const GeneMap& map, int population_size, std::mt19937& rng);

// pick tournament_size random individuals and return the one with the lowest fitness
const Individual& tournament_select(const Population& population, int tournament_size, std::mt19937& rng);

// best (lowest fitness) individual in the population
const Individual& best_individual(const Population& population);

// build the next generation: keep the best individual (elitism), then fill the rest with
// children of tournament-selected parents. each child is mutated with probability mutation_rate
// (a wall-fix mutation with probability wall_mutation_share, otherwise a random flip)
// and repaired so it stays fully lit.
Population next_generation(const Population& population, const GeneMap& map,
                           int tournament_size, double mutation_rate, double wall_mutation_share,
                           std::mt19937& rng);

// turn an individual into a Solution (grid with 'L' for bulbs, plus its violation count)
Solution individual_to_solution(const Individual& individual, const GeneMap& map, const Problem& problem);

#endif // GENETIC_H