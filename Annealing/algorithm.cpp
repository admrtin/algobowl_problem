#include "algorithm.h"

#include <iostream>
#include <fstream>
#include <string>

constexpr static std::string SKIP_CHARACTERS = "X01234";
constexpr static std::string NUMBERED_WALL = "01234";

const static int NUM_DIRECTIONS = 4;

constexpr static int di[4] = {-1, 1, 0, 0};
constexpr static int dj[4] = {0, 0, -1, 1};

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

void Algorithm::output(const char* output_file) {
    // try to open the file for write
    std::ofstream out(output_file, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open output file!\n";
        return;
    }
    
    violations = getViolations(1);
    out << violations << std::endl;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            out << solution[i][j];
        }
        out << std::endl;
    }
}

// get the number of violations in the current board
// if the cell is not lit and final = 1, return inf (1e9)
// otherwise count it as NON_LIT_VIOLATION
// for numbered cell count the number of violations as |needed - current|
int Algorithm::getViolations(bool final) {
    // declare violation number
    int violations = 0;
    // 0 is unlit, 1 is lit
    std::vector<std::vector<bool>> lit(r, std::vector<bool>(c));
    // if the cell is bad, so it violates row/column bulbs or wrong number of adjacent bulbs
    std::vector<std::vector<bool>> bad(r, std::vector<bool>(c));

    // Check rows
    for (int i = 0; i < r; i++) {
        // Last lightbull column
        int lst = -1;
        int l = 0; // Start of the block
        // j == c acts as a wall closing the last block
        for (int j = 0; j <= c; j++) {
            // If solution[i][j] is either '0'-'4' or 'X'
            if (j == c || SKIP_CHARACTERS.find(solution[i][j]) != std::string::npos) {
                if (lst != -1) {
                    for (int k = l; k < j; k++) {
                        lit[i][k] = 1;
                    }
                }
                // Reset pointer
                lst = -1;
                l = j + 1;
            } else if (solution[i][j] == 'L') {
                if (lst != -1) {
                    // It is a violation, so set the respective array
                    bad[i][lst] = bad[i][j] = 1;
                }
                lst = j;
            }
        }
    }

    // Check columns
    for (int j = 0; j < c; j++) {
        // Last lightbull row
        int lst = -1;
        int l = 0; // Start of the block
        // i == r acts as a wall closing the last block
        for (int i = 0; i <= r; i++) {
            // If solution[i][j] is either '0'-'4' or 'X'
            if (i == r || SKIP_CHARACTERS.find(solution[i][j]) != std::string::npos) {
                if (lst != -1) {
                    for (int k = l; k < i; k++) {
                        lit[k][j] = 1;
                    }
                }
                // Reset pointer
                lst = -1;
                l = i + 1;
            } else if (solution[i][j] == 'L') {
                if (lst != -1) {
                    // It is a violation, so set the respective array
                    bad[lst][j] = bad[i][j] = 1;
                }
                lst = i;
            }
        }
    }

    // calculate the total violation quantity
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            // if the cell is 0-4
            if (NUMBERED_WALL.find(solution[i][j]) != std::string::npos) {
                int needed = solution[i][j] - '0';
                int current = 0;
                for (auto [x, y] : getAdjacent(i, j)) {
                    current += solution[x][y] == 'L';
                }
                violations += abs(needed - current);
            } else if (solution[i][j] == '.') {
                // make sure that it is lit, or add the penalty
                violations += !lit[i][j] * NON_LIT_VIOLATION;
                // if the it is the final count, there should not be unlit cells,
                // so return inf (1e9)
                if (final) return 1e9;
            } else if (solution[i][j] == 'L') {
                // check if the bulb is adjacent to any
                violations += bad[i][j];
            }
        }
    }
    return violations;
}

//============================= PART1 =============================//
// Fill the board with bulbs 'L' that lit all cells.
void Algorithm::greedyFill() {
    // create a dummy array to keep track of lighted cells
    // 0 is unlit, 1 is lit
    std::vector<std::vector<bool>> lit(r, std::vector<bool>(c));
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] != '.') continue;
            // if adjacent wall is lit and it is not a wall, then the cell is also lit
            if (i) lit[i][j] = lit[i][j] | lit[i - 1][j];
            if (j) lit[i][j] = lit[i][j] | lit[i][j - 1];
            // skip if the cell is lit
            if (lit[i][j]) continue;
            board[i][j] = 'L';
            lit[i][j] = 1;
        }
    }
}

// Find cells with light bulbs that are uniquely determined
// It asssumes solution with 0 violations exists (WHICH MAY NOT BE TRUE)
void Algorithm::deterministicFill() {
    // copy the solution. If we know cell has no light bulb -> '-'
    std::vector<std::string> solutionDummy = solution;
    // fills all lit cells with '-'
    auto fillLitCells = [&](this auto& self, int i, int j) {
        
    };
    // check 0, 4 numbered walls first, as they determine the answer
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (solutionDummy[i][j] == '0') {
                // place all adjacent as '-'
                for (auto [x, y] : getAdjacent(i, j)) {
                    char& ch = solutionDummy[x][y];
                    if (ch != '.') continue;
                    ch = '-';       // uses reference, so I do not have to write solutionDummy
                }
            } else if (solutionDummy[i][j] == '4') {
                // place all adjacent as 'L'
                for (auto [x, y] : getAdjacent(i, j)) {
                    char& ch = solutionDummy[x][y];
                    if (ch != '.') continue;
                    ch = 'L';       // uses reference, so I do not have to write solutionDummy
                }
            }
        }
    }
}

//========================= HELPER FUNCTIONS ========================//
std::vector<std::pair<int, int>> Algorithm::getAdjacent(int i, int j) {
    std::vector<std::pair<int, int>> adjacentCells;
    for (int t = 0; t < NUM_DIRECTIONS; t++) {
        int x = i + di[t];
        int y = j + di[t];
        // skip if falls outside
        if (x < 0 || y < 0 || x >= r || y >= c) continue;
        adjacentCells.push_back({x, y});
    }
    return adjacentCells;
}