#ifndef GENETIC_H
#define GENETIC_H

#include <vector>
#include <random>

#include "akari_io.h"

struct Individual {
    // an individual's gene vector is a (n*m) length string of 0s & 1s representing grid locations for light placement
    // keep track of original location by using the original rows/cols
    std::vector<u_int8_t> genes;
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
};

struct Population {
    std::vector<Individual> individuals;
};

// true if the cell is a wall: numbered ('0'-'4') or unnumbered ('X')
inline bool is_wall(char cell) {
    return (cell >= '0' && cell <= '4') || cell == 'X';
}

// initializer: each gene is a bulb with probability bulb_probability, then the individual is repaired so it starts fully lit
Individual make_individual(const GeneMap& map, std::mt19937& rng, double bulb_probability = 0.5);

// light every unlit cell by putting a bulb on it, visiting cells in random order.
// afterwards the individual has 0 unlit cells. the new bulbs never see another bulb,
// but they can still break a numbered wall's count.
void repair_individual(Individual& individual, const GeneMap& map, std::mt19937& rng);

// mutate an individual's genes
void mutate_individual(Individual& individual, std::mt19937& rng);

// make two children from two parents
std::pair<Individual, Individual> make_children(const std::pair<Individual, Individual>& parents, std::mt19937& rng);

// the two quantities the fitness is built from
struct Evaluation {
    int violations; // cells the verifier would flag: bulbs that see another bulb + numbered walls with the wrong count
    int unlit;      // open cells with no bulb in their row or column segment (any > 0 makes the output invalid)
};

// count violations and unlit cells for an individual
Evaluation evaluate(const Individual& individual, const GeneMap& map);

// fitness to minimize: violations + penalty * unlit, where the penalty is larger than any
// possible violation count, so lighting one more cell always beats removing every violation
int64_t compute_fitness(const Individual& individual, const GeneMap& map);

// produce the valid map of gene cells for placing lights
GeneMap make_gene_map(const Problem& problem);

#endif // GENETIC_H