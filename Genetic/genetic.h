#ifndef GENETIC_H
#define GENETIC_H

#include <vector>
#include <random>

#include "akari_io.h"

struct Individual {
    // an individual's gene vector is a (n*m) length string of 0s & 1s representing grid locations for light placement
    // keep track of original location by using the original rows/cols
    std::vector<u_int8_t> genes;
    int fitness=0;
};

struct GeneMap {
    std::vector<std::pair<int,int>> cells; // each index of cells represents it's original row/col.
};

struct Population {
    std::vector<Individual> individuals;
};

// initializer
Individual make_individual(const GeneMap& map, std::mt19937& rng);

// mutate an individual's genes
void mutate_individual(Individual& individual, std::mt19937& rng);

// make two children from two parents
std::pair<Individual, Individual> make_children(std::pair<Individual, Individual> parents);

// fitness function to determine new population
int compute_fitness(Individual individual, const Problem& problem, const GeneMap& map);

// produce the valid map of gene cells for placing lights
GeneMap make_gene_map(const Problem& problem);

#endif // GENETIC_H