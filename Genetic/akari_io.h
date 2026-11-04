#ifndef AKARI_IO_H
#define AKARI_IO_H

#include <vector>
#include <string>

struct Problem {
    int num_rows;                  // the number of rows
    int num_cols;                  // the number of columns
    std::vector<std::string> grid; // cells are '.', 'X', '0'-'4', and 'L' for lights in a solution
};

struct Solution {
    Problem problem;
    int violations;
    std::vector<std::string> grid;
};

// reads an input file into board, returns false (with a message on stderr) if the file is malformed
bool read_input(const char* input_file, Problem& board);
// writes the violation count followed by the solution grid, returns false if the file can't be opened
bool write_output(const char* output_file, const Solution& solution);

#endif
