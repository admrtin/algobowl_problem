#include "Annealing_algo.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <utility>

using namespace std;

int Evaluation::energy() const {
    return conflictingBulbs + clueError + 5 * unlit;
}

void Solver::read() {
    cin >> rows >> cols;

    board.resize(rows);
    cellId.assign(rows, vector<int>(cols, -1));

    cells.clear();
    horizontalCount = 0;
    verticalCount = 0;

    // Give each blank cell a unique ID.
    for (int r = 0; r < rows; ++r) {
        cin >> board[r];

        for (int c = 0; c < cols; ++c) {
            if (board[r][c] == '.') {
                int id = static_cast<int>(cells.size());

                cellId[r][c] = id;
                cells.push_back({r, c, -1, -1});
            }
        }
    }

    // Assign horizontal segments.
    // A gray or numbered cell separates segments.
    for (int r = 0; r < rows; ++r) {
        int segment = -1;

        for (int c = 0; c < cols; ++c) {
            int id = cellId[r][c];

            if (id == -1) {
                segment = -1;
            } else {
                if (segment == -1) {
                    segment = horizontalCount++;
                }

                cells[id].horizontal = segment;
            }
        }
    }

    // Assign vertical segments.
    for (int c = 0; c < cols; ++c) {
        int segment = -1;

        for (int r = 0; r < rows; ++r) {
            int id = cellId[r][c];

            if (id == -1) {
                segment = -1;
            } else {
                if (segment == -1) {
                    segment = verticalCount++;
                }

                cells[id].vertical = segment;
            }
        }
    }
}

Evaluation Solver::evaluate(const vector<bool>& bulbs) const {
    vector<int> horizontal(horizontalCount, 0);
    vector<int> vertical(verticalCount, 0);

    int count = static_cast<int>(cells.size());

    // Count bulbs in each segment.
    for (int i = 0; i < count; ++i) {
        if (bulbs[i]) {
            ++horizontal[cells[i].horizontal];
            ++vertical[cells[i].vertical];
        }
    }

    Evaluation result{};

    for (int i = 0; i < count; ++i) {
        const Cell& cell = cells[i];

        int h = horizontal[cell.horizontal];
        int v = vertical[cell.vertical];

        // No bulb in either segment means this cell is unlit.
        if (h == 0 && v == 0) {
            ++result.unlit;
        }

        // Count each bulb that sees another bulb once.
        if (bulbs[i] && (h > 1 || v > 1)) {
            ++result.violations;
            ++result.conflictingBulbs;
        }
    }

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    // Check numbered gray cells.
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            char value = board[r][c];

            if (value < '0' || value > '4') {
                continue;
            }

            int adjacent = 0;

            for (int d = 0; d < 4; ++d) {
                int nr = r + dr[d];
                int nc = c + dc[d];

                if (nr < 0 || nr >= rows ||
                    nc < 0 || nc >= cols) {
                    continue;
                }

                int id = cellId[nr][nc];

                if (id != -1 && bulbs[id]) {
                    ++adjacent;
                }
            }

            result.clueError += std::abs(adjacent - (value - '0'));

            // One violation for each unsatisfied numbered cell.
            if (adjacent != value - '0') {
                ++result.violations;
            }
        }
    }

    return result;
}

void Solver::solve(double seconds) {
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();

    int count = static_cast<int>(cells.size());

    horizontalCells.assign(horizontalCount, std::vector<int>{});
    verticalCells.assign(verticalCount, std::vector<int>{});

    horizontalBulbs.assign(horizontalCount, 0);
    verticalBulbs.assign(verticalCount, 0);

    bulbs.assign(count, false);

    for (int id = 0; id < count; ++id) {
        horizontalCells[cells[id].horizontal].push_back(id);
        verticalCells[cells[id].vertical].push_back(id);
    }

    std::vector<int> order(count);
    std::iota(order.begin(), order.end(), 0);

    // Cache clue neighbours for moves aimed at unsatisfied clues.
    struct Clue {
        int required;
        std::vector<int> neighbours;
    };
    std::vector<Clue> clues;
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board[r][c] < '0' || board[r][c] > '4') {
                continue;
            }
            Clue clue{board[r][c] - '0', {}};
            for (int d = 0; d < 4; ++d) {
                int nr = r + dr[d], nc = c + dc[d];
                if (nr >= 0 && nr < rows && nc >= 0 && nc < cols &&
                    cellId[nr][nc] != -1) {
                    clue.neighbours.push_back(cellId[nr][nc]);
                }
            }
            clues.push_back(std::move(clue));
        }
    }

    std::vector<bool> best;
    int bestViolations = 0;
    bool haveBest = false;
    long long attempts = 0;
    int runs = 0;
    auto elapsedSeconds = [&]() {
        return std::chrono::duration<double>(Clock::now() - start).count();
    };
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    auto chooseIndex = [&](int size) {
        return std::uniform_int_distribution<int>(0, size - 1)(rng);
    };
    auto saveBest = [&]() {
        // Only fully illuminated boards may be written to the output.
        if (currentScore.unlit == 0 &&
            (!haveBest || currentScore.violations < bestViolations)) {
            best = bulbs;
            bestViolations = currentScore.violations;
            haveBest = true;
        }
    };

    // Five independent cooling runs share the overall time budget.
    // Always initialize once, including when the budget is zero.
    const int maxRuns = 5;
    for (int run = 0; run < maxRuns; ++run) {
        if (run > 0 && (bestViolations == 0 || elapsedSeconds() >= seconds)) {
            break;
        }
        ++runs;
        std::fill(bulbs.begin(), bulbs.end(), false);
        std::fill(horizontalBulbs.begin(), horizontalBulbs.end(), 0);
        std::fill(verticalBulbs.begin(), verticalBulbs.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);

        // A maximal nonconflicting placement lights every blank cell.
        for (int id : order) {
            int h = cells[id].horizontal;
            int v = cells[id].vertical;
            if (horizontalBulbs[h] == 0 && verticalBulbs[v] == 0) {
                bulbs[id] = true;
                ++horizontalBulbs[h];
                ++verticalBulbs[v];
            }
        }
        currentScore = evaluate(bulbs);
        saveBest();

        double runStart = elapsedSeconds();
        double runEnd = seconds * (run + 1) / maxRuns;
        while (count > 0 && bestViolations > 0) {
            double elapsed = elapsedSeconds();
            if (elapsed >= runEnd) {
                break;
            }
            double progress = (elapsed - runStart) / (runEnd - runStart);
            double temperature = 2.0 * std::pow(0.02 / 2.0, progress);
            int id = chooseIndex(count);
            double moveKind = probability(rng);

            if (moveKind < 0.3 && !clues.empty()) {
                // Sample a few clues rather than scanning the entire board.
                for (int trial = 0; trial < 12; ++trial) {
                    const Clue& clue = clues[chooseIndex(static_cast<int>(clues.size()))];
                    int adjacent = 0;
                    for (int other : clue.neighbours) {
                        adjacent += bulbs[other];
                    }
                    if (adjacent == clue.required) {
                        continue;
                    }
                    int candidates[4];
                    int candidateCount = 0;
                    for (int other : clue.neighbours) {
                        // Add for a deficit; remove for an excess.
                        if (bulbs[other] == (adjacent > clue.required)) {
                            candidates[candidateCount++] = other;
                        }
                    }
                    if (candidateCount > 0) {
                        id = candidates[chooseIndex(candidateCount)];
                        break;
                    }
                }
            } else if (moveKind < 0.5) {
                // Aim some moves at conflicting bulbs or dark cells.
                for (int trial = 0; trial < 12; ++trial) {
                    int other = chooseIndex(count);
                    int h = horizontalBulbs[cells[other].horizontal];
                    int v = verticalBulbs[cells[other].vertical];
                    if ((bulbs[other] && (h > 1 || v > 1)) || (h == 0 && v == 0)) {
                        id = other;
                        break;
                    }
                }
            }

            int partner = -1;
            if (probability(rng) < 0.5) {
                // Relocate within either visible segment, allowing an occupied
                // source or an empty destination to be selected first.
                bool horizontal = probability(rng) < 0.5;
                const auto& segment = horizontal
                    ? horizontalCells[cells[id].horizontal]
                    : verticalCells[cells[id].vertical];
                int offset = chooseIndex(static_cast<int>(segment.size()));
                for (int k = 0; k < static_cast<int>(segment.size()); ++k) {
                    int other = segment[(offset + k) % segment.size()];
                    if (bulbs[other] != bulbs[id]) {
                        partner = other;
                        break;
                    }
                }
            }

            int oldEnergy = currentScore.energy();
            toggle(id);
            if (partner != -1) {
                toggle(partner);
            }
            int delta = currentScore.energy() - oldEnergy;
            bool accept = delta <= 0 ||
                probability(rng) < std::exp(-static_cast<double>(delta) / temperature);
            if (!accept) {
                // Undo the complete move in reverse order.
                if (partner != -1) {
                    toggle(partner);
                }
                toggle(id);
            } else {
                saveBest();
            }
            ++attempts;
        }
    }

    // Verify the saved solution once before output.
    Evaluation checked = evaluate(best);

    std::vector<std::string> output = board;

    for (int id = 0; id < count; ++id) {
        if (best[id]) {
            output[cells[id].row][cells[id].col] = 'L';
        }
    }

    std::cout << checked.violations << '\n';

    for (const std::string& row : output) {
        std::cout << row << '\n';
    }

    // Diagnostics go to stderr, keeping output.txt valid.
    double elapsed =
        std::chrono::duration<double>(
            Clock::now() - start
        ).count();

    std::cerr << "Cooling runs: " << runs << '\n';
    std::cerr << "Attempts: " << attempts << '\n';
    std::cerr << "Elapsed seconds: " << elapsed << '\n';
    std::cerr << "Unlit cells: " << checked.unlit << '\n';
    std::cerr << "Violations: " << checked.violations << '\n';
}

void Solver::toggle(int id) {
    int h = cells[id].horizontal;
    int v = cells[id].vertical;

    // Calculate only the score contributions that this move
    // can change.
    auto affectedScore = [&]() -> Evaluation {
        Evaluation result{};

        auto countCell = [&](int other) {
            int otherH = cells[other].horizontal;
            int otherV = cells[other].vertical;

            int hCount = horizontalBulbs[otherH];
            int vCount = verticalBulbs[otherV];

            if (hCount == 0 && vCount == 0) {
                ++result.unlit;
            }

            if (bulbs[other] &&
                (hCount > 1 || vCount > 1)) {
                ++result.violations;
                ++result.conflictingBulbs;
            }
        };

        for (int other : horizontalCells[h]) {
            countCell(other);
        }

        for (int other : verticalCells[v]) {
            // The changed cell is in both segments.
            // Count it only once.
            if (other != id) {
                countCell(other);
            }
        }

        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};

        int r = cells[id].row;
        int c = cells[id].col;

        // Only clues adjacent to the changed bulb can change.
        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d];
            int nc = c + dc[d];

            if (nr < 0 || nr >= rows ||
                nc < 0 || nc >= cols) {
                continue;
            }

            char clue = board[nr][nc];

            if (clue < '0' || clue > '4') {
                continue;
            }

            int adjacent = 0;

            for (int k = 0; k < 4; ++k) {
                int ar = nr + dr[k];
                int ac = nc + dc[k];

                if (ar < 0 || ar >= rows ||
                    ac < 0 || ac >= cols) {
                    continue;
                }

                int other = cellId[ar][ac];

                if (other != -1 && bulbs[other]) {
                    ++adjacent;
                }
            }

            result.clueError += std::abs(adjacent - (clue - '0'));
            if (adjacent != clue - '0') {
                ++result.violations;
            }
        }

        return result;
    };

    Evaluation before = affectedScore();

    int change = 1;

    if (bulbs[id]) {
        change = -1;
    }

    bulbs[id] = !bulbs[id];

    horizontalBulbs[h] += change;
    verticalBulbs[v] += change;

    Evaluation after = affectedScore();

    currentScore.violations +=
        after.violations - before.violations;

    currentScore.unlit +=
        after.unlit - before.unlit;
    currentScore.conflictingBulbs +=
        after.conflictingBulbs - before.conflictingBulbs;
    currentScore.clueError +=
        after.clueError - before.clueError;
}
