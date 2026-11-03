#include <iostream>
#include <string>
#include "akari_io.h"
#include "genetic.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <input_path> <output_path>\n";
        return 1;
    }
    char* input_path = argv[1]; char* output_path = argv[2];

    Problem problem;
    Solution solution;

    read_input(input_path, problem);
    
    std::cout << "read akari input file at " << input_path << "\n";
    std::cout << "num_rows: " << problem.num_rows << "\tnum_cols: " << problem.num_cols << "\n";

    // generate an initial solution population
    
}