#include "akari_io.h"

#include <iostream>
#include <fstream>

const static std::string CHARACTERS = "01234X.";

bool read_input(const char* input_file, Problem& problem) {
    // try to open the file for read
    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file!\n";
        return false;
    }
    // read rows and columns
    if (!(in >> problem.num_rows >> problem.num_cols) || problem.num_rows <= 0 || problem.num_cols <= 0) {
        std::cerr << "Error: number of rows or columns were passed incorrectly\n";
        return false;
    }
    // Read all rows
    problem.grid.assign(problem.num_rows, "");
    for (int i = 0; i < problem.num_rows; i++) {
        in >> problem.grid[i];
        if ((int)problem.grid[i].size() != problem.num_cols) {
            std::cerr << "Error: input file row " << i + 1 << " has " << problem.grid[i].size()
                      << " character(s), while it should have " << problem.num_cols << "\n";
            return false;
        }
        // Check row content
        for (int j = 0; j < problem.num_cols; j++) {
            if (CHARACTERS.find(problem.grid[i][j]) == std::string::npos) {
                std::cerr << "Error: input file cell (" << i + 1 << ", " << j + 1 << ") has invalid value "
                          << problem.grid[i][j] << "\n";
                return false;
            }
        }
    }
    return true;
}

bool write_output(const char* output_file, const Solution& solution) {
    // try to open the file for write
    std::ofstream out(output_file, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open output file!\n";
        return false;
    }
    out << solution.violations << '\n';
    for (int i = 0; i < solution.problem.num_rows; i++) {
        out << solution.grid[i] << '\n';
    }
    return true;
}
