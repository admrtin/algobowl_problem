#include "algorithm.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <string>

namespace {

const std::string SKIP_CHARACTERS = "X01234";

constexpr int NUM_DIRECTIONS = 4;
constexpr int di[NUM_DIRECTIONS] = {-1, 1, 0, 0};
constexpr int dj[NUM_DIRECTIONS] = {0, 0, -1, 1};

bool isNumberedWall(char ch) {
    return ch >= '0' && ch <= '4';
}
} // namespace


// part 2 constants 
constexpr int ANNEALING_THREADS = 24;                       // independent parallel searches
constexpr double INITIAL_TEMPERATURE = 2.0;                 // initial search temperature
constexpr double FINAL_TEMPERATURE = 0.10;                  // final search temperature

// part 3 contants
constexpr int MAX_WINDOW_SIZE = 29;                     // max square window nxn                     
constexpr int MAX_VISITED_NODES = 200'000;                // max nodes in one dfs tree of nodes

constexpr int MAX_LNS_ROUNDS = 100;                       // the number of times to run LNS

// constructor takes the input_file to read
Algorithm::Algorithm(const char* input_file) {
    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file!\n";
        return;
    }

    in >> r >> c;
    board.resize(r);
    for (int i = 0; i < r; i++) in >> board[i];

    solution = board;
    getSegmentData();
}

// Use a saved solution as a starting point, if it matches the board and is fully lit.
// Keep the current solution if the saved one is worse.
bool Algorithm::loadSolution(const char* filename) {
    std::ifstream in(filename);
    int reported;
    if (!(in >> reported)) return false;

    std::vector<std::string> candidate(r);
    for (int i = 0; i < r; i++) {
        if (!(in >> candidate[i]) || int(candidate[i].size()) != c) return false;
        for (int j = 0; j < c; j++) {
            if (board[i][j] != '.' && candidate[i][j] != board[i][j]) return false;
            if (board[i][j] == '.' && candidate[i][j] != '.' && candidate[i][j] != 'L') return false;
        }
    }

    std::vector<std::string> old_solution = solution;
    solution = candidate;
    getSegmentData();

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.' && !isLit(i, j)) {
                solution = old_solution;
                getSegmentData();
                return false;
            }
        }
    }

    int new_violations = getViolations();
    solution = old_solution;
    getSegmentData();

    // Only replace a valid current solution if the saved solution is strictly better.
    bool current_valid = true;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.' && !isLit(i, j)) current_valid = false;
        }
    }
    if (current_valid && getViolations() <= new_violations) return false;

    solution = candidate;
    getSegmentData();
    std::cout << "Loaded saved solution: " << new_violations << " violations\n";
    return true;
}

void Algorithm::setDeadlines(std::chrono::steady_clock::time_point annealing_end,
                             std::chrono::steady_clock::time_point overall_end) {
    annealing_deadline = annealing_end;
    deadline = overall_end;
}

// output the solution to the given cell
void Algorithm::output(const char* output_file) {
    std::ofstream out(output_file, std::ios::trunc);

    // Complete any remaining gaps before writing the exact verifier score.
    completeIllumination();

    out << getViolations() << '\n';
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) out << solution[i][j];
        out << '\n';
    }
}

// Count exactly the cells violating rules
int Algorithm::getViolations() {
    // reset all segments
    for (auto& segment : horizontals) segment.bulbs = 0;
    for (auto& segment : verticals) segment.bulbs = 0;

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (solution[i][j] != 'L' || horizontal_id[i][j] == -1) continue;
            horizontals[horizontal_id[i][j]].bulbs++;
            verticals[vertical_id[i][j]].bulbs++;
        }
    }

    int total = 0;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (isNumberedWall(board[i][j])) {
                int bulbs = 0;
                for (auto [x, y] : getAdjacent(i, j)) {
                    bulbs += solution[x][y] == 'L';
                }
                num_wall[i][j] = bulbs;
                // add wall violation
                total += bulbs != board[i][j] - '0';
            } else if (solution[i][j] == 'L') {
                // A bulb is counted once even if it conflicts in both directions.
                total += horizontals[horizontal_id[i][j]].bulbs > 1 ||
                         verticals[vertical_id[i][j]].bulbs > 1;
            }
        }
    }

    return total;
}

// Fills arrays with correct segment and numbered-wall data
void Algorithm::getSegmentData() {
    // reset all data
    horizontals.clear();
    verticals.clear();
    horizontal_id.assign(r, std::vector<int>(c, -1));
    vertical_id.assign(r, std::vector<int>(c, -1));
    num_wall.assign(r, std::vector<int>(c, 0));

    // Get horizontals
    for (int i = 0; i < r; i++) {
        int l = 0; // Start of the segment

        // j == c acts as a wall closing the last segment
        for (int j = 0; j <= c; j++) {
            if (j == c || SKIP_CHARACTERS.find(board[i][j]) != std::string::npos) {
                if (l < j) {
                    // get current index
                    int id = int(horizontals.size());
                    // send it
                    horizontals.emplace_back();

                    for (int k = l; k < j; k++) {
                        horizontal_id[i][k] = id;
                        horizontals[id].add(i, k, solution[i][k] == 'L');
                    }
                }
                l = j + 1;
            }
        }
    }

    // Get verticals
    for (int j = 0; j < c; j++) {
        int l = 0; // Start of the segment

        // i == r acts as a wall closing the last segment
        for (int i = 0; i <= r; i++) {
            if (i == r || SKIP_CHARACTERS.find(board[i][j]) != std::string::npos) {
                if (l < i) {
                    // get current index
                    int id = int(verticals.size());
                    // send it
                    verticals.emplace_back();

                    for (int k = l; k < i; k++) {
                        vertical_id[k][j] = id;
                        verticals[id].add(k, j, solution[k][j] == 'L');
                    }
                }
                l = i + 1;
            }
        }
    }

    // Count neighboring bulbs for numbered walls
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (!isNumberedWall(board[i][j])) continue;
            for (auto [x, y] : getAdjacent(i, j)) {
                num_wall[i][j] += solution[x][y] == 'L';
            }
        }
    }

    // add correct number of violations
    score = 0;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (isNumberedWall(board[i][j])) score += wallScore(i, j);
            else if (board[i][j] == '.') score += cellScore(i, j);
        }
    }
}

// A smooth wall penalty gives credit for moving a clue closer to its target.
int Algorithm::wallScore(int i, int j) {
    int difference = std::abs(num_wall[i][j] - (board[i][j] - '0'));
    return difference == 0 ? 0 : 3 + difference;
}

// Search penalty from a single floor cell.
int Algorithm::cellScore(int i, int j) {
    if (solution[i][j] == 'L') {
        return 8 * (horizontals[horizontal_id[i][j]].bulbs > 1 ||
                    verticals[vertical_id[i][j]].bulbs > 1);
    }
    return 5 * !isLit(i, j);
}

// Recalculate the part of the search score that can change when (i, j) changes.
int Algorithm::affectedScore(int i, int j) {
    int affected = 0;

    for (auto [x, y] : horizontals[horizontal_id[i][j]].lst) {
        affected += cellScore(x, y);
    }
    for (auto [x, y] : verticals[vertical_id[i][j]].lst) {
        if (x == i && y == j) continue;
        affected += cellScore(x, y);
    }

    for (auto [x, y] : getAdjacent(i, j)) {
        if (isNumberedWall(board[x][y])) {
            affected += wallScore(x, y);
        }
    }

    return affected;
}

// Update the cell state and all segment / numbered-wall counters.
void Algorithm::changeCell(int i, int j, char ch) {
    bool old_bulb = solution[i][j] == 'L';
    bool new_bulb = ch == 'L';
    solution[i][j] = ch;
    if (old_bulb == new_bulb) return;

    int bulb_change = int(new_bulb) - int(old_bulb);
    horizontals[horizontal_id[i][j]].bulbs += bulb_change;
    verticals[vertical_id[i][j]].bulbs += bulb_change;

    for (auto [x, y] : getAdjacent(i, j)) {
        if (isNumberedWall(board[x][y])) num_wall[x][y] += bulb_change;
    }
}

// Change a floor cell and update segment, numbered-wall, and search-score data.
int Algorithm::setCell(int i, int j, char ch) {
    if (!isIn(i, j) || horizontal_id[i][j] == -1) return score;
    if (ch != '.' && ch != 'L') return score;
    if (solution[i][j] == ch) return score;

    int before = affectedScore(i, j);
    changeCell(i, j, ch);
    int after = affectedScore(i, j);

    score += after - before;
    return score;
}

// Light every remaining dark cell, since the verifier rejects any dark cell.
// A dark cell has no bulb in either of its two segments, so this cannot create a conflict.
void Algorithm::completeIllumination() {
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (horizontal_id[i][j] != -1 && !isLit(i, j)) setCell(i, j, 'L');
        }
    }
}

//============================= PART2 =============================//
// Simulated annealing. We keep segments for ease keeping of violations
// every floor cell may receive a bulb.

void Algorithm::annealingFill() {

    std::vector<int> open_cells, numbered_cells;
    std::vector<unsigned char> near_wall(r * c, 0);

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.') open_cells.push_back(i * c + j);
            if (!isNumberedWall(board[i][j])) continue;

            numbered_cells.push_back(i * c + j);
            for (auto [x, y] : getAdjacent(i, j)) {
                if (board[x][y] == '.') near_wall[x * c + y] = 1;
            }
        }
    }

    if (open_cells.empty()) return;

    const auto stop_time = std::min(annealing_deadline, deadline - std::chrono::seconds(3));
    if (std::chrono::steady_clock::now() >= stop_time) return;

    int threads_count = ANNEALING_THREADS;
    std::vector<std::pair<int, std::vector<std::string>>> results(threads_count);
    std::vector<std::thread> threads;
    threads.reserve(threads_count);

    // Give each worker an independent random seed.
    std::random_device rd;
    uint64_t now = uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::vector<std::array<uint32_t, 6>> seeds(threads_count);
    for (int t = 0; t < threads_count; t++) {
        seeds[t] = {rd(), rd(), rd(), uint32_t(now), uint32_t(now >> 32), uint32_t(t)};
    }

    for (int t = 0; t < threads_count; t++) {
        threads.emplace_back([&, t]() {
            // Each worker owns its own board, segments, score, and RNG.
            Algorithm worker = *this;

            std::seed_seq seed(seeds[t].begin(), seeds[t].end());
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> random_probability(0.0, 1.0);

            auto randomInt = [&](int limit) {
                return std::uniform_int_distribution<int>(0, limit - 1)(rng);
            };

            // Use different starting boards so the workers explore different solutions.
            if (t > 0) {
                if (t % 3 == 0) {
                    // Start from scratch for some workers.
                    worker.solution = worker.board;
                    worker.getSegmentData();
                } else if (t % 3 == 2) {
                    // Perturb the existing solution for some workers.
                    int changes = std::max(1, int(open_cells.size()) / 50);
                    for (int k = 0; k < changes; k++) {
                        int cell_id = open_cells[randomInt(int(open_cells.size()))];
                        int i = cell_id / c, j = cell_id % c;
                        worker.setCell(i, j, worker.solution[i][j] == 'L' ? '.' : 'L');
                    }
                }

                // Randomized greedy fill makes the 24 initial solutions different.
                std::vector<int> order = open_cells;
                std::shuffle(order.begin(), order.end(), rng);
                for (int cell_id : order) {
                    int i = cell_id / c, j = cell_id % c;
                    if (!worker.isLit(i, j)) worker.setCell(i, j, 'L');
                }
            }

            std::vector<int> bulb_cells;
            std::vector<int> bulb_position(r * c, -1);
            for (int cell_id : open_cells) {
                if (worker.solution[cell_id / c][cell_id % c] != 'L') continue;
                bulb_position[cell_id] = int(bulb_cells.size());
                bulb_cells.push_back(cell_id);
            }

            auto addBulb = [&](int cell_id) {
                if (bulb_position[cell_id] != -1) return;
                bulb_position[cell_id] = int(bulb_cells.size());
                bulb_cells.push_back(cell_id);
            };

            auto removeBulb = [&](int cell_id) {
                int pos = bulb_position[cell_id];
                if (pos == -1) return;

                int last_cell = bulb_cells.back();
                bulb_cells[pos] = last_cell;
                bulb_position[last_cell] = pos;
                bulb_cells.pop_back();
                bulb_position[cell_id] = -1;
            };

            // Each independent worker reheats periodically, up to the shared deadline.
            constexpr int RESTART_STEPS = 150'000;
            double cooling_rate = std::pow(FINAL_TEMPERATURE / INITIAL_TEMPERATURE,
                                           1.0 / RESTART_STEPS);
            double temperature = INITIAL_TEMPERATURE;


            std::vector<std::string> best_solution = worker.solution;
            int best_score = worker.score;

            for (int step = 0; best_score > 0; step++) {
                if ((step & 511) == 0 && std::chrono::steady_clock::now() >= stop_time) break;
                if (step > 0 && step % RESTART_STEPS == 0) {
                    // Reheat from the best state instead of freezing at the temperature floor.
                    worker.solution = best_solution;
                    worker.getSegmentData();
                    bulb_cells.clear();
                    std::fill(bulb_position.begin(), bulb_position.end(), -1);
                    for (int id : open_cells) {
                        if (worker.solution[id / c][id % c] != 'L') continue;
                        bulb_position[id] = int(bulb_cells.size());
                        bulb_cells.push_back(id);
                    }
                    temperature = INITIAL_TEMPERATURE;
                }
                int old_score = worker.score;
                std::array<int, 2> changed_cells;
                std::array<char, 2> old_states;
                int changed = 0;

                auto change = [&](int cell_id, char ch) {
                    int i = cell_id / c, j = cell_id % c;
                    if (worker.solution[i][j] == ch) return;
                    changed_cells[changed] = cell_id;
                    old_states[changed] = worker.solution[i][j];
                    changed++;
                    worker.setCell(i, j, ch);
                };

                int move_type = bulb_cells.empty() ? 0 : randomInt(6);

                if (move_type == 0) {
                    // Add a bulb to a random empty cell.
                    for (int k = 0; k < 8 && !changed; k++) {
                        int cell_id = open_cells[randomInt(int(open_cells.size()))];
                        if (worker.solution[cell_id / c][cell_id % c] == '.') change(cell_id, 'L');
                    }
                } else if (move_type == 1) {
                    // Remove a random bulb.
                    change(bulb_cells[randomInt(int(bulb_cells.size()))], '.');
                } else if (move_type == 2) {
                    // Move a bulb to a neighboring empty cell.
                    int cell_id = bulb_cells[randomInt(int(bulb_cells.size()))];
                    int i = cell_id / c, j = cell_id % c;
                    int direction = randomInt(NUM_DIRECTIONS);

                    for (int k = 0; k < NUM_DIRECTIONS; k++) {
                        int d = (direction + k) % NUM_DIRECTIONS;
                        int ni = i + di[d], nj = j + dj[d];
                        if (!worker.isIn(ni, nj) || worker.solution[ni][nj] != '.') continue;
                        change(cell_id, '.');
                        change(ni * c + nj, 'L');
                        break;
                    }
                } else if (move_type == 3) {
                    // Move a bulb within its segment to a cell next to a numbered wall.
                    int cell_id = bulb_cells[randomInt(int(bulb_cells.size()))];
                    int i = cell_id / c, j = cell_id % c;
                    int destination = -1, found = 0;

                    auto consider = [&](const SegmentData& segment) {
                        for (auto [x, y] : segment.lst) {
                            int id = x * c + y;
                            if (!near_wall[id] || worker.solution[x][y] != '.') continue;
                            if (randomInt(++found) == 0) destination = id;
                        }
                    };

                    consider(worker.horizontals[worker.horizontal_id[i][j]]);
                    consider(worker.verticals[worker.vertical_id[i][j]]);
                    if (destination != -1) {
                        change(cell_id, '.');
                        change(destination, 'L');
                    }
                } else if (move_type == 4 && !numbered_cells.empty()) {
                    // Repair a random incorrect numbered wall.
                    for (int k = 0; k < 8 && !changed; k++) {
                        int cell_id = numbered_cells[randomInt(int(numbered_cells.size()))];
                        int i = cell_id / c, j = cell_id % c;
                        int required = worker.board[i][j] - '0';
                        int current = worker.num_wall[i][j];
                        if (current == required) continue;

                        int destination = -1, found = 0;
                        char target = current < required ? '.' : 'L';
                        for (auto [x, y] : worker.getAdjacent(i, j)) {
                            if (worker.solution[x][y] != target) continue;
                            if (randomInt(++found) == 0) destination = x * c + y;
                        }
                        if (destination != -1) change(destination, target == '.' ? 'L' : '.');
                    }
                } else if (move_type == 5) {
                    // Long-range move: relocate a bulb anywhere on the board.
                    int source = bulb_cells[randomInt(int(bulb_cells.size()))];
                    for (int k = 0; k < 8 && !changed; k++) {
                        int destination = open_cells[randomInt(int(open_cells.size()))];
                        if (worker.solution[destination / c][destination % c] != '.') continue;
                        change(source, '.');
                        change(destination, 'L');
                    }
                }

                // Always try an actual change, even if a selected move is impossible.
                if (!changed) {
                    int cell_id = open_cells[randomInt(int(open_cells.size()))];
                    char ch = worker.solution[cell_id / c][cell_id % c] == 'L' ? '.' : 'L';
                    change(cell_id, ch);
                }

                int delta = old_score - worker.score;
                if (delta >= 0 || random_probability(rng) < std::exp(double(delta) / temperature)) {
                    for (int k = 0; k < changed; k++) {
                        int cell_id = changed_cells[k];
                        if (worker.solution[cell_id / c][cell_id % c] == 'L') addBulb(cell_id);
                        else removeBulb(cell_id);
                    }
                    if (worker.score < best_score) {
                        best_score = worker.score;
                        best_solution = worker.solution;
                    }
                } else {
                    // Rejected changes must be undone in reverse order.
                    for (int k = changed - 1; k >= 0; k--) {
                        int cell_id = changed_cells[k];
                        worker.setCell(cell_id / c, cell_id % c, old_states[k]);
                    }
                }

                temperature *= cooling_rate;
            }

            worker.solution = best_solution;
            worker.getSegmentData();
            worker.completeIllumination();
            int final_violations = worker.getViolations();
            results[t] = {final_violations, worker.solution};
        });
    }

    for (auto& thread : threads) thread.join();

    // Compare actual verifier scores after completing each solution.
    int best_worker = 0;
    for (int t = 1; t < threads_count; t++) {
        if (results[t].first < results[best_worker].first) best_worker = t;
    }

    // Never discard an already-valid starting solution when a worker does worse.
    int old_violations = getViolations();
    bool old_valid = true;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.' && !isLit(i, j)) old_valid = false;
        }
    }
    if (!old_valid || results[best_worker].first < old_violations) {
        solution = std::move(results[best_worker].second);
        getSegmentData();
    }
    std::cout << "Annealing best: " << getViolations() << " violations\n";
}

//============================= PART3 =============================//
// Search a complete bulb arrangement inside each window while keeping every
// bulb outside the window fixed. Only completely illuminated, conflict-free
// arrangements with fewer incorrect numbered walls are accepted.
void Algorithm::fixCell() {
    using Clock = std::chrono::steady_clock;
    std::mt19937 rng(std::random_device{}());
    int current_violations = getViolations();

    std::vector<int> bad_walls;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (isNumberedWall(board[i][j]) && num_wall[i][j] != board[i][j] - '0') {
                bad_walls.push_back(i * c + j);
            }
        }
    }
    std::shuffle(bad_walls.begin(), bad_walls.end(), rng);

    // Reuse these arrays for every window instead of allocating a full board each time.
    std::vector<int> wall_stamp(r * c, 0), cell_stamp(r * c, 0);
    std::vector<int> wall_remaining(r * c, 0);
    std::vector<int> remaining_h(horizontals.size()), remaining_v(verticals.size());
    int stamp = 0;

    for (int center_id : bad_walls) {
        if (Clock::now() >= deadline - std::chrono::seconds(3)) return;

        int i = center_id / c, j = center_id % c;
        if (num_wall[i][j] == board[i][j] - '0') continue;

        for (int n = 3; n <= MAX_WINDOW_SIZE; n += 2) {
            if (Clock::now() >= deadline - std::chrono::seconds(3)) return;
            stamp++;
            int half = n / 2;
            int top = std::max(0, i - half), bottom = std::min(r, i + half + 1);
            int left = std::max(0, j - half), right = std::min(c, j + half + 1);

            std::vector<int> window_cells, affected_walls, affected_cells;
            for (int x = top; x < bottom; x++) {
                for (int y = left; y < right; y++) {
                    if (board[x][y] != '.') continue;
                    window_cells.push_back(x * c + y);
                    for (auto [wx, wy] : getAdjacent(x, y)) {
                        if (!isNumberedWall(board[wx][wy])) continue;
                        int wall_id = wx * c + wy;
                        if (wall_stamp[wall_id] != stamp) {
                            wall_stamp[wall_id] = stamp;
                            affected_walls.push_back(wall_id);
                            wall_remaining[wall_id] = 0;
                        }
                        wall_remaining[wall_id]++;
                    }
                }
            }
            if (window_cells.empty()) continue;

            // Collect every floor cell whose illumination might change.
            for (int cell_id : window_cells) {
                int x = cell_id / c, y = cell_id % c;
                for (auto [a, b] : horizontals[horizontal_id[x][y]].lst) {
                    int id = a * c + b;
                    if (cell_stamp[id] != stamp) {
                        cell_stamp[id] = stamp;
                        affected_cells.push_back(id);
                    }
                }
                for (auto [a, b] : verticals[vertical_id[x][y]].lst) {
                    int id = a * c + b;
                    if (cell_stamp[id] != stamp) {
                        cell_stamp[id] = stamp;
                        affected_cells.push_back(id);
                    }
                }
            }

            // The search only changes walls adjacent to a window cell.
            int old_local_violations = 0;
            for (int wall_id : affected_walls) {
                int x = wall_id / c, y = wall_id % c;
                old_local_violations += num_wall[x][y] != board[x][y] - '0';
            }
            if (old_local_violations == 0) continue;

            // More constrained cells first; prefer the original bulb positions.
            std::shuffle(window_cells.begin(), window_cells.end(), rng);
            std::stable_sort(window_cells.begin(), window_cells.end(), [&](int a, int b) {
                auto priority = [&](int id) {
                    int x = id / c, y = id % c;
                    int value = solution[x][y] == 'L' ? 10 : 0;
                    for (auto [wx, wy] : getAdjacent(x, y)) {
                        if (isNumberedWall(board[wx][wy])) value += 5;
                    }
                    return value;
                };
                return priority(a) > priority(b);
            });

            std::vector<char> original_state;
            for (int cell_id : window_cells) {
                original_state.push_back(solution[cell_id / c][cell_id % c]);
            }

            // Remove the old bulbs; the DFS will rebuild every cell in the square.
            for (int cell_id : window_cells) {
                setCell(cell_id / c, cell_id % c, '.');
            }

            std::fill(remaining_h.begin(), remaining_h.end(), 0);
            std::fill(remaining_v.begin(), remaining_v.end(), 0);
            for (int cell_id : window_cells) {
                int x = cell_id / c, y = cell_id % c;
                remaining_h[horizontal_id[x][y]]++;
                remaining_v[vertical_id[x][y]]++;
            }

            int best_local = old_local_violations;
            std::vector<char> best_state;
            std::vector<char> selected(window_cells.size(), '.');
            int visited_nodes = 0;
            bool exhausted = false;

            auto dfs = [&](auto&& self, int k) -> void {
                if (best_local == 0 || exhausted) return;
                visited_nodes++;
                if (visited_nodes >= MAX_VISITED_NODES ||
                    ((visited_nodes & 255) == 0 && Clock::now() >= deadline - std::chrono::seconds(3))) {
                    exhausted = true;
                    return;
                }

                // Lower bound: wrong walls that no remaining window bulbs can fix.
                int forced_bad = 0;
                for (int wall_id : affected_walls) {
                    int x = wall_id / c, y = wall_id % c;
                    int required = board[x][y] - '0';
                    int current = num_wall[x][y];
                    if (current > required || current + wall_remaining[wall_id] < required) forced_bad++;
                }
                if (forced_bad >= best_local) return;

                if (k == int(window_cells.size())) {
                    // Check all potentially affected illumination, including outside the square.
                    for (int id : affected_cells) {
                        if (!isLit(id / c, id % c)) return;
                    }
                    best_local = forced_bad;
                    best_state = selected;
                    return;
                }

                int cell_id = window_cells[k];
                int x = cell_id / c, y = cell_id % c;
                int h = horizontal_id[x][y], v = vertical_id[x][y];
                remaining_h[h]--;
                remaining_v[v]--;
                for (auto [wx, wy] : getAdjacent(x, y)) {
                    if (isNumberedWall(board[wx][wy])) wall_remaining[wx * c + wy]--;
                }

                auto branch = [&](bool place) {
                    if (place && (horizontals[h].bulbs > 0 || verticals[v].bulbs > 0)) return;
                    if (place) setCell(x, y, 'L');
                    selected[k] = place ? 'L' : '.';

                    // A dark cell with no future bulb in either segment is impossible.
                    bool possible = true;
                    if (remaining_h[h] == 0 || remaining_v[v] == 0) {
                        auto check = [&](const SegmentData& segment) {
                            for (auto [a, b] : segment.lst) {
                                if (!isLit(a, b) && remaining_h[horizontal_id[a][b]] == 0 &&
                                    remaining_v[vertical_id[a][b]] == 0) return false;
                            }
                            return true;
                        };
                        if (remaining_h[h] == 0) possible = check(horizontals[h]);
                        if (possible && remaining_v[v] == 0) possible = check(verticals[v]);
                    }
                    if (possible) self(self, k + 1);
                    if (place) setCell(x, y, '.');
                };

                // Try the original state first, then its opposite.
                if (original_state[k] == 'L') {
                    branch(true);
                    branch(false);
                } else {
                    branch(false);
                    branch(true);
                }

                for (auto [wx, wy] : getAdjacent(x, y)) {
                    if (isNumberedWall(board[wx][wy])) wall_remaining[wx * c + wy]++;
                }
                remaining_h[h]++;
                remaining_v[v]++;
            };

            dfs(dfs, 0);

            // DFS has restored the empty square. Apply only a strict improvement.
            const auto& chosen = best_state.empty() ? original_state : best_state;
            for (int k = 0; k < int(window_cells.size()); k++) {
                int id = window_cells[k];
                setCell(id / c, id % c, chosen[k]);
            }

            if (!best_state.empty()) {
                current_violations -= old_local_violations - best_local;
                std::cout << "LNS: " << current_violations << " violations, window " << n << "x" << n << '\n';
                break;
            }
            // If the exact search ran out of nodes, a larger window cannot be explored reliably.
            if (exhausted) break;
        }
    }
}

void Algorithm::windowFill() {
    using Clock = std::chrono::steady_clock;
    if (Clock::now() >= deadline - std::chrono::seconds(3)) return;

    // LNS needs a conflict-free, fully illuminated initial configuration.
    auto previous = solution;
    int previous_violations = getViolations();
    bool previous_valid = true;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.' && !isLit(i, j)) previous_valid = false;
        }
    }
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (solution[i][j] != 'L') continue;
            int h = horizontal_id[i][j], v = vertical_id[i][j];
            if (horizontals[h].bulbs > 1 || verticals[v].bulbs > 1) setCell(i, j, '.');
        }
    }
    completeIllumination();
    getSegmentData();

    int stagnant_rounds = 0;
    for (int round = 0; round < MAX_LNS_ROUNDS; round++) {
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
            deadline - Clock::now()
        ).count();

        if (remaining <= 3) {
            std::cout << "LNS stopped: time limit reached\n";
            break;
        }

        std::cout << "LNS round " << round + 1
                << " started, remaining time: "
                << remaining << " seconds\n";

        int before = getViolations();
        if (before == 0) break;

        fixCell();

        int after = getViolations();

        std::cout << "LNS round " << round + 1
                << ": " << after << " violations\n";

        if (after >= before) {
            if (++stagnant_rounds >= 2) {
                std::cout << "LNS stopped: no improvement in 2 rounds\n";
                break;
            }
        } else {
            stagnant_rounds = 0;
        }
    }
}

//========================= HELPER FUNCTIONS ========================//
// get all adjacent cells that do not fall out
std::vector<std::pair<int, int>> Algorithm::getAdjacent(int i, int j) {
    std::vector<std::pair<int, int>> adjacentCells;

    for (int t = 0; t < NUM_DIRECTIONS; t++) {
        int x = i + di[t];
        int y = j + dj[t];

        // skip if falls outside
        if (!isIn(x, y)) continue;
        adjacentCells.push_back({x, y});
    }

    return adjacentCells;
}