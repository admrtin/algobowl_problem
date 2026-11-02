#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <stack>

class Algorithm {
private:
    int r;                                          // the number of rows
    int c;                                          // the number of columns
    std::vector<std::string> board;                 // the board
    std::vector<std::string> solution;              // the solution board, similar notation to board, but lights are denoted with 'L'
    int violations = 0;                             // the number of violations
public:
    // reads input file and fills up board and solution board
    explicit Algorithm(const char* input_file);
    // do greedy parsing to make all lights placed so all cells are lit
    void initialize();
    // return the file
    void output(const char* output_file);
};

#endif ALGORITHM_H