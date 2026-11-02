#include "algorithm.h"

#include <iostream>
#include <fstream>

Algorithm::Algorithm(const char* input_file) {
    // try to open the file
    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file!\n";
        return;
    }
    // read rows and columns
    in >> r >> c;
    // Read all rows
    board.resize(r);
    for (int i = 0; i < r; i++) {
        // Read board line
        in >> board[i];
    }
    // Solution starts as a copy of the board
    solution = board;
    // Copy all empty cells (.) to empty_cell 
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            
        }
    }
}

void Algorithm::deterministic() {

}