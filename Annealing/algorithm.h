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
    constexpr static int NON_LIT_VIOLATION = 1000;  // how many violations for non lit cell
public:
    // reads input file and fills up board and solution board
    explicit Algorithm(const char* input_file);
    // return the solution board to the file
    void output(const char* output_file);
    
    // get the number of violations
    int getViolations(bool final = 0);

    //============= PART 1 =============//
    // greedily fill the cells
    void greedyFill();
    // deterministicly fill the cells
    void deterministicFill();
    
    //============= PART 2 =============//
    // annealing algorithm
    void annealingFill(); 
    // lit the given cell and get the new number of violations
    int litCell(int i, int j);
    // unlit the given cell and get the new number of violations
    int unlitCell(int i, int j);
    // move the given cell to adjacent cell and get the number of violations
    int moveCellAdjacent(int i, int j, int ni, int nj);
    // move the given cell to cell ajacent row/column-vice to numberered cell and get the number of violations
    int moveCellLine(int i, int j, int ni, int nj);
    // checker if we apply the change at time t
    bool applyChange(int delta, int t);

    //============= PART 3 =============//
    // slide window algorithm
    void windowFill();
    // try different windows to fix the node with violation
    void fixCell();

    //============= HELPER FUNCTIONS ==========//
    std::vector<std::pair<int, int>> getAdjacent(int i, int j);
};

#endif ALGORITHM_H