#ifndef GENETIC_H
#define GENETIC_H

#include <bitset>
#include <vector>

template <std::size_t N>
struct Individual {
    // an individual is a (n*m) length string of 0s & 1s representing grid locations for light placement
    // keep track of original location by using the original rows/cols
    std::bitset<N> gene;
};

template <std::size_t N>
struct Population {
    // a population contains individuals.
    std::vector<Individual<N>>  individuals;
};

#endif GENETIC_H