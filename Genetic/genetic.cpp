#include "genetic.h"
#include <chrono>

Individual make_individual(const GeneMap& map, std::mt19937& rng) {
    Individual individual;
    individual.genes.reserve(map.cells.size());
    individual.fitness = 0; // initialize fitness to 0

    std::bernoulli_distribution bernoulli(0.5);

    for (size_t i = 0; i < map.cells.size(); i++) {
        individual.genes.push_back(bernoulli(rng)); // assigns 0 or 1 randomly at this location.
    }

    return individual;
}

