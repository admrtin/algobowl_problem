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
    std::stack<std::pair<int, int>> empty_cell;     // keeps all potential empty ceclls where light could be placed at
public:
    // reads input file and fills up board and solution board
    explicit Algorithm(const char* input_file);
    /*
    Finds for each empty cell (.) if it should have a light or not. Denotes light with L, if cell does not have a light, 
    it is removed from empty_cell. Algorithm stops when no empty cell (.) is updates. Worst case scenario O(row^2 col^2).
    */
    void deterministic();
};

#endif ALGORITHM_H