#include "algorithm.h"

#include <iostream>
#include <fstream>

Algorithm::Algorithm(const char* input_file) {
    // try to open the file for read
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
}

void Algorithm::initialize() {
    // create a dummy array to keep track of lighted cells
    // 0 is unlit, 1 is lit
    std::vector<std::vector<bool>> vis(r, std::vector<bool>(c));
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] != '.') continue;
            // if adjacent wall is lit and it is not a wall, then the cell is also lit
            if (i) vis[i][j] = vis[i][j] | vis[i - 1][j];
            if (j) vis[i][j] = vis[i][j] | vis[i][j - 1];
            // skip if the cell is lit
            if (vis[i][j]) continue;
            board[i][j] = 'L';
            vis[i][j] = 1;
        }
    }
}

void Algorithm::output(const char* output_file) {
    // try to open the file for write
    std::ofstream out(output_file, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open output file!\n";
        return;
    }
    
    out << violations << std::endl;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            out << solution[i][j];
        }
        out << std::endl;
    }
}