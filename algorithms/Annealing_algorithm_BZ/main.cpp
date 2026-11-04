#include <iostream>

#include "Annealing_algo.h"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Solver solver;

    solver.read();
    solver.solve(30.0);  // Approximately five seconds of work.

    return 0;
}