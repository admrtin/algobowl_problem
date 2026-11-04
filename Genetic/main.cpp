#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include "akari_io.h"
#include "genetic.h"

// GA parameters
const double DEFAULT_TIME_LIMIT = 60.0; // seconds
const int DEFAULT_POPULATION_SIZE = 10;
const int TOURNAMENT_SIZE = 2;
const double MUTATION_RATE = 0.75;
const double WALL_MUTATION_SHARE = 0.9; // share of mutations that fix a wrong numbered wall instead of flipping a random gene
const int REPORT_EVERY = 100; // print progress every N generations

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0]
                  << " <input_path> <output_path> <seed> [time_limit_seconds=" << DEFAULT_TIME_LIMIT
                  << "] [population_size=" << DEFAULT_POPULATION_SIZE << "]\n";
        return 1;
    }
    char* input_path = argv[1]; char* output_path = argv[2];
    const unsigned int seed = static_cast<unsigned int>(std::stoul(argv[3]));
    const double time_limit = (argc > 4) ? std::stod(argv[4]) : DEFAULT_TIME_LIMIT;
    const int population_size = (argc > 5) ? std::stoi(argv[5]) : DEFAULT_POPULATION_SIZE;
    if (time_limit <= 0 || population_size < 2) {
        std::cerr << "time_limit must be > 0 and population_size must be >= 2\n";
        return 1;
    }
    const auto start_time = std::chrono::steady_clock::now();
    auto elapsed_seconds = [&]() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    };

    // random number generator
    std::mt19937 rng(seed);

    Problem problem;
    if (!read_input(input_path, problem)) return 1;

    std::cout << "read akari input file at " << input_path << "\n";
    std::cout << "num_rows: " << problem.num_rows << "\tnum_cols: " << problem.num_cols;

    // build the gene map for this problem
    GeneMap map = make_gene_map(problem);
    std::cout << "\tnum_genes: " << map.cell_map.size() << "\n";
    int forced = 0, forbidden = 0;
    for (int8_t f : map.fixed) { forced += (f == GeneMap::FORCED); forbidden += (f == GeneMap::FORBIDDEN); }
    std::cout << "walls decide " << forced << " forced + " << forbidden << " forbidden genes, "
              << map.free_genes.size() << " left free\n";

    std::cout << "time_limit: " << time_limit << "s\tpopulation_size: " << population_size << "\n";
    Population population = make_population(map, population_size, rng);

    int generation = 0;
    for (; elapsed_seconds() < time_limit; generation++) {
        const Individual& best = best_individual(population);
        if (best.fitness == 0) {
            std::cout << "perfect solution found at generation " << generation << "\n";
            break;
        }
        if (generation % REPORT_EVERY == 0) {
            Evaluation e = evaluate(best, map);
            std::cout << "generation " << generation
                      << "\tviolations: " << e.violations << "\tunlit: " << e.unlit
                      << "\telapsed: " << elapsed_seconds() << "s"
                      << "\tconflicts: " << e.bulb_conflicts << "\twalls: " << e.wall_violations << "\n";
        }
        population = next_generation(population, map, TOURNAMENT_SIZE, MUTATION_RATE, WALL_MUTATION_SHARE, rng);
    }

    const Individual& best = best_individual(population);
    Evaluation e = evaluate(best, map);
    std::cout << "stopped after " << generation << " generations in " << elapsed_seconds() << "s\n";
    std::cout << "final violations: " << e.violations << "\tunlit: " << e.unlit
              << "\tconflicts: " << e.bulb_conflicts << "\twalls: " << e.wall_violations << "\n";

    Solution solution = individual_to_solution(best, map, problem);
    if (!write_output(output_path, solution)) return 1;
    std::cout << "wrote solution to " << output_path << "\n";
}
