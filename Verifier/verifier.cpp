#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>

using namespace std;

const static string BOARD_CHARACTERS = "01234X.";
const static string SOLUTION_CHARACTERS = "01234X.L";
const static string SKIP_CHARACTERS = "X01234";

const static int di[4] = {-1, 1, 0, 0};
const static int dj[4] = {0, 0, -1, 1};

/*
================== USAGE =====================
Compile file: g++ -o verifier verifier.cpp

Check the board:    ./verifier input_file
Check the solution: ./verifier input_file output_file
*/

int main(int argc, char* argv[]) {
    //================================= OPEN INPUT FILE ====================================//
    if (argc != 3 && argc != 2) {
        std::cerr << "ERROR: the format is './verifier input_file [output_file]'\n";
        return 1;
    }
    ifstream board_file(argv[1]);
    if (!board_file.is_open()) {
        std::cerr << "Error: Could not open input file!\n";
        return 1;
    }

    //================================== READ INPUT FILE ====================================//
    int r = -1, c = -1;                     // Number of rows and columns
    board_file >> r >> c;
    if (!board_file || r <= 0 || c <= 0 || (long long)r * c > 100000) {
        std::cerr << "Error: number of rows or columns were passed incorrectly (need 1 <= r * c <= 10^5)\n";
        return 1;
    }

    vector<string> board(r);
    // Read all rows
    for (int i = 0; i < r; i++) {
        // Read board line
        board_file >> board[i];
        // Find its length
        int board_len = board[i].size();
        if (board_len != c) {
            std::cerr << "Error: input file row " << i + 1 << " has ";
            std::cerr << board_len << " character(s), while it should have " << c << endl;
            return 1;
        }
        // Check boards content
        for (int j = 0; j < c; j++) {
            if (BOARD_CHARACTERS.find(board[i][j]) == std::string::npos) {
                std::cerr << "Error: input file cell (" << i + 1 << ", " << j + 1 << ") has value " << board[i][j] << ", but availible characters are '0', '1', '2', '3', '4', 'X', '.'" << endl;
                return 1;
            }
        }
    }
    // Nothing else may follow the grid in the input file
    string extra;
    if (board_file >> extra) {
        std::cerr << "Error: input file has extra content after row " << r << endl;
        return 1;
    }

    // Only the board was requested, so we are done
    if (argc == 2) {
        std::cout << "INPUT is good\n";
        return 0;
    }

    //================================= OPEN OUTPUT FILE ===================================//
    ifstream solution_file(argv[2]);
    if (!solution_file.is_open()) {
        std::cerr << "Error: Could not open output file!\n";
        return 1;
    }

    //================================= READ OUTPUT FILE ===================================//
    int violation = 0;
    if (!(solution_file >> violation) || violation < 0) {
        std::cerr << "Error: first line of the output must be a non-negative number of violations\n";
        return 1;
    }
    const int reported = violation;

    vector<string> solution(r);
    // Read all rows
    for (int i = 0; i < r; i++) {
        // Read solution line
        solution_file >> solution[i];
        // Find its length
        int solution_len = solution[i].size();
        if (solution_len != c) {
            std::cerr << "Error: output file row " << i + 1 << " has ";
            std::cerr << solution_len << " character(s), while it should have " << c << endl;
            return 1;
        }
        // Check solutions content
        for (int j = 0; j < c; j++) {
            if (SOLUTION_CHARACTERS.find(solution[i][j]) == std::string::npos) {
                std::cerr << "Error: output file cell (" << i + 1 << ", " << j + 1 << ") has value " << solution[i][j] << ", but availible characters are '0', '1', '2', '3', '4', 'X', '.', 'L'" << endl;
                return 1;
            }
            if (board[i][j] == solution[i][j]) continue;
            if (board[i][j] != '.' || solution[i][j] != 'L') {
                std::cerr << "Error: board cell (" << i + 1 << ", " << j + 1 << ") has value " << board[i][j]; 
                std::cerr << ", corresponding solution cell is " << solution[i][j] << ". Two cells can differ if and only if board[i][j] = '.' and solution[i][j] = 'L'\n";  
                return 1;
            }
        }
    }

    // Nothing else may follow the grid in the output file
    extra = "";
    if (solution_file >> extra) {
        std::cerr << "Error: output file has extra content after row " << r << endl;
        return 1;
    }

    //================================= CHECK THE SOLUTION =================================//
    vector<vector<bool>> violations(r, vector<bool>(c, false));
    vector<vector<bool>> illuminated(r, vector<bool>(c, false));
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
                        illuminated[i][k] = 1;
                    }
                }
                // Reset pointer
                lst = -1;
                l = j + 1;
            } else if (solution[i][j] == 'L') {
                if (lst != -1) {
                    // It is a violation, so set the respective array
                    violations[i][lst] = violations[i][j] = 1;
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
                        illuminated[k][j] = 1;
                    }
                }
                // Reset pointer
                lst = -1;
                l = i + 1;
            } else if (solution[i][j] == 'L') {
                if (lst != -1) {
                    // It is a violation, so set the respective array
                    violations[lst][j] = violations[i][j] = 1;
                }
                lst = i;
            }
        }
    }

    // Check numbered gray cells
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (solution[i][j] < '0' || solution[i][j] > '4') continue;
            int bulbs = 0;
            for (int d = 0; d < 4; d++) {
                int ni = i + di[d], nj = j + dj[d];
                if (ni >= 0 && ni < r && nj >= 0 && nj < c && solution[ni][nj] == 'L') bulbs++;
            }
            if (bulbs != solution[i][j] - '0') {
                violations[i][j] = 1;
            }
        }
    }

    // Check reported number of violations
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            violation -= violations[i][j];
        }
    }
    if (violation != 0) {
        std::cerr << "ERROR: wrong number of violations reported: " << reported << " reported, " << reported - violation << " actual\n";
        return 1;
    }

    // Check that all empty cells are illuminated
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] != '.') continue;
            if (!illuminated[i][j]) {
                std::cerr << "ERROR: cell (" << i + 1 << ", " << j + 1 << ") is not illuminated\n";
                return 1;
            }
        }
    }

    std::cout << "OUTPUT is good\n";
}