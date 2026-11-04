#ifndef ANNEALING_ALGO_H
#define ANNEALING_ALGO_H

#include <random>
#include <string>
#include <vector>

struct Cell {
    int row;
    int col;
    int horizontal;
    int vertical;
};

struct Evaluation {
    int violations = 0;     // Official output score: conflicting bulbs + wrong clues.
    int unlit = 0;
    int conflictingBulbs = 0;
    int clueError = 0;      // Sum of absolute differences from required clue counts.

    int energy() const;
};

class Solver {
private:
    int rows;
    int cols;
    
    std::vector<bool> bulbs;
    std::vector<std::string> board;
    std::vector<std::vector<int>> cellId;
    std::vector<Cell> cells;

    int horizontalCount = 0;
    int verticalCount = 0;

    std::mt19937 rng{std::random_device{}()};

    // Cells belong to each segment.
    std::vector<std::vector<int>> horizontalCells;
    std::vector<std::vector<int>> verticalCells;

    // Current number of bulbs in each segment
    std::vector<int> horizontalBulbs;
    std::vector<int> verticalBulbs;

    Evaluation currentScore{};

    //Add or remove one bulb and update the score
    void toggle(int id);


public:
    void read();
    Evaluation evaluate(const std::vector<bool>& bulbs) const;
    void solve(double seconds);
};

#endif