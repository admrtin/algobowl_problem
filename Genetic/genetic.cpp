#include "genetic.h"
#include <chrono>

Individual make_individual(const GeneMap& map, std::mt19937& rng) {
    Individual individual;
    individual.genes.reserve(map.cells.size());
    individual.fitness = 0; // initialize fitness to 0

    std::bernoulli_distribution bernoulli(0.5);

    for (size_t i = 0; i < map.cells.size(); i++) {
        individual.genes.push_back(bernoulli(rng)); // assigns 0 or 1 randomly at this cell location.
    }

    return individual;
}

GeneMap make_gene_map(const Problem& problem) {
    GeneMap map;

    for (int row = 0; row < problem.num_rows; row++) {
        for (int col = 0; col < problem.num_cols; col++) {
            char cell = problem.grid.at(row).at(col);
            bool is_wall = (cell >= '0' && cell <= '4') || cell == 'X';

            if (!is_wall) {
                map.cells.emplace_back(row, col);
            }
        }
    }

    return map;
}