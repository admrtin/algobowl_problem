
#include "algorithm.h"

#include <iostream>
#include <string>

int main() {
    int group, option;

    std::cout << "Enter input group: ";
    std::cin >> group;

    // Display all possible combinations
    std::cout << "\nChoose algorithms to run:\n";

    const std::string names[4] = {
        "Deterministic", "Greedy", "Annealing", "Window (LNS)"
    };

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
    std::cin >> option;

    if (!std::cin || option < 0 || option > 15) {
        std::cout << "Invalid option!\n";
        return 1;
    }

    std::string input_file = "../TeamsInputs/input_group" + std::to_string(group) + ".txt";
    std::string out_file = "output_group" + std::to_string(group) + ".txt";

    Algorithm algorithm(input_file.c_str());

    // Run selected algorithms in order
    if (option & 1) algorithm.deterministicFill();
    if (option & 2) algorithm.greedyFill();
    if (option & 4) algorithm.annealingFill();
    if (option & 8) algorithm.windowFill();

    algorithm.output(out_file.c_str());

    // Run verifier automatically
    std::cout << "\nVerifying solution...\n";

    std::string command = "../Verifier/verifier \"" + input_file + "\" \"" + out_file + "\"";
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cout << "Verification failed!\n";
        return 1;
    }

    return 0;
}
