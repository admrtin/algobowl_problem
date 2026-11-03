#include <chrono>
#include <iostream>
#include <string>
#include "akari_io.h"
#include "genetic.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0] << " <input_path> <output_path> <seed>\n";
        return 1;
    }
    char* input_path = argv[1]; char* output_path = argv[2];
    const unsigned int seed = static_cast<unsigned int>(std::stoul(argv[3]));

    // random number generator
    std::mt19937 rng(seed);

    Problem problem;
    Solution solution;

    read_input(input_path, problem);
    
    std::cout << "read akari input file at " << input_path << "\n";
    std::cout << "num_rows: " << problem.num_rows << "\tnum_cols: " << problem.num_cols;

    // build the gene map for this problem
    GeneMap map = make_gene_map(problem);

    Individual test_individual = make_individual(map, rng);
    
    std::cout << "\tnum_genes: " << test_individual.genes.size() << "\n";

    for (size_t i=0; i < test_individual.genes.size(); i++) {
        std::cout << static_cast<int>(test_individual.genes.at(i));
    }
    std::cout << "\n";

}