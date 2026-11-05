#include "algorithm.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    int group, option;
    std::cout << "Enter input group: ";
    if (!(std::cin >> group)) return 1;

    const std::string names[4] = {"Deterministic", "Greedy", "Annealing", "Window (LNS)"};
    std::cout << "\nChoose algorithms to run:\n";
    for (int mask = 0; mask < 16; mask++) {
        std::cout << mask << ". ";
        bool first = true;
        for (int k = 0; k < 4; k++) {
            if (!(mask & (1 << k))) continue;
            if (!first) std::cout << " + ";
            std::cout << names[k];
            first = false;
        }
        if (first) std::cout << "None";
        std::cout << '\n';
    }

    std::cout << "\nEnter option (0-15): ";
    if (!(std::cin >> option) || option < 0 || option > 15) return 1;

    std::string input_file = "../TeamsInputs/input_group" + std::to_string(group) + ".txt";
    std::string out_file = "output_group" + std::to_string(group) + ".txt";
    std::string best_file = "../TeamsOutputs/output_group" + std::to_string(group) + ".txt";

    // best_file = 'X';

    constexpr int ANNEALING_TIME = 20;
    constexpr int LNS_TIME = 40;
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    auto stop = start + std::chrono::seconds(ANNEALING_TIME - 5); // 5 seconds reserved for output and verifier
    auto anneal_stop = (option & 4) && (option & 8) ? start + std::chrono::seconds(LNS_TIME) : stop;

    Algorithm algorithm(input_file.c_str());
    algorithm.setDeadlines(anneal_stop, stop);

    if (option & 1) algorithm.deterministicFill();
    if (option & 2) algorithm.greedyFill();
    algorithm.loadSolution(best_file.c_str());

    if (option & 4) {
        std::cout << "Annealing (24 threads)...\n" << std::flush;
        algorithm.annealingFill();
    }
    if (option & 8) {
        std::cout << "Window repair (LNS)...\n" << std::flush;
        algorithm.windowFill();
    }
    algorithm.output(out_file.c_str());

    int found_violations = -1, best_violations = -1;
    std::ifstream found_input(out_file), best_input(best_file);
    found_input >> found_violations;
    best_input >> best_violations;
    std::cout << "\nFOUND_NUMBER_OF_VIOLATIONS = " << found_violations << '\n';
    if (best_input) std::cout << "BEST_FOUND_NUMBER_OF_VIOLATION = " << best_violations << '\n';

    std::string command = "../Verifier/verifier \"" + input_file + "\" \"" + out_file + "\"";
    return std::system(command.c_str()) == 0 ? 0 : 1;
}
