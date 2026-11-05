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
const std::string NUMBERED_WALL = "01234";

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

constexpr int MAX_LNS_ROUNDS = 30;                       // the number of times to run LNS

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
    greedyFill();

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
    num_available.assign(r, std::vector<int>(c, 0));
    can_bulb.assign(r, std::vector<unsigned char>(c, 0));

    // Initially, every empty cell is a possible bulb location
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            can_bulb[i][j] = board[i][j] == '.' && solution[i][j] != 'L';
        }
    }

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
                        // define each cell id as i * c + j + 1 <---- 1-indexation
                        int cell_id = i * c + k + 1;
                        horizontals[id].add(i, k, can_bulb[i][k], solution[i][k] == 'L', cell_id);
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
                        // define each cell id as i * c + j + 1 <---- 1-indexation
                        int cell_id = k * c + j + 1;
                        verticals[id].add(k, j, can_bulb[k][j], solution[k][j] == 'L', cell_id);
                    }
                }
                l = i + 1;
            }
        }
    }

    // Count neighboring bulbs and available candidates for numbered walls
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (NUMBERED_WALL.find(board[i][j]) == std::string::npos) continue;
            // If the wall is numbered
            for (auto [x, y] : getAdjacent(i, j)) {
                // count number of cells satisfying numbered wall
                num_wall[i][j] += solution[x][y] == 'L';
                // count the potential number of candidates
                num_available[i][j] += can_bulb[x][y];
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

// Add the wall to the queue
void Algorithm::enqueueNumberedWall(int i, int j) {
    if (!propagation_active || !isIn(i, j)) return;
    if (NUMBERED_WALL.find(board[i][j]) == std::string::npos) return;
    if (wall_queued[i][j]) return;

    wall_queued[i][j] = 1;
    numbered_walls.push(i * c + j);
}

// Add the cell to the queue
void Algorithm::enqueueGoodCell(int i, int j) {
    if (!propagation_active || !isIn(i, j)) return;
    if (horizontal_id[i][j] == -1 || isLit(i, j)) return;
    if (good_queued[i][j]) return;

    good_queued[i][j] = 1;
    good_cells.push(i * c + j);
}

// If a segment has at most one candidate, inspect its cells.
void Algorithm::enqueueGoodCellsFromSegment(bool horizontal, int id) {
    const SegmentData& segment = horizontal ? horizontals[id] : verticals[id];
    if (segment.bulbs > 0 || segment.unknown > 1) return;

    for (auto [i, j] : segment.lst) {
        if (isLit(i, j)) continue;
        if (possibleLightCount(i, j) <= 1) {
            enqueueGoodCell(i, j);
        }
    }
}

// number of candidate bulbs that can illuminate (i, j)
int Algorithm::possibleLightCount(int i, int j) {
    int h = horizontal_id[i][j];
    int v = vertical_id[i][j];
    if (h == -1 || v == -1) return 0;

    // The cell itself belongs to both segments, so count it only once.
    return horizontals[h].unknown + verticals[v].unknown - can_bulb[i][j];
}

// XOR of candidate cell ids in the union of the two segments
int Algorithm::possibleLightXor(int i, int j) {
    int h = horizontal_id[i][j];
    int v = vertical_id[i][j];
    if (h == -1 || v == -1) return 0;

    int result = horizontals[h].xorCandidates ^ verticals[v].xorCandidates;
    if (can_bulb[i][j]) result ^= (i * c + j + 1);
    return result;
}

// Remove a cell from the set of possible bulb locations.
void Algorithm::removeCandidate(int i, int j) {
    if (!can_bulb[i][j]) return;

    int cell_id = i * c + j + 1;
    int h = horizontal_id[i][j];
    int v = vertical_id[i][j];
    int old_h_unknown = horizontals[h].unknown;
    int old_v_unknown = verticals[v].unknown;

    can_bulb[i][j] = 0;
    horizontals[h].removeCandidate(cell_id);
    verticals[v].removeCandidate(cell_id);

    // Update available positions around adjacent numbered walls
    for (auto [x, y] : getAdjacent(i, j)) {
        if (NUMBERED_WALL.find(board[x][y]) == std::string::npos) continue;
        num_available[x][y]--;
        enqueueNumberedWall(x, y);
    }

    // A segment only needs to be scanned when its candidate count reaches 1 or 0.
    if (old_h_unknown > horizontals[h].unknown && horizontals[h].unknown <= 1) {
        enqueueGoodCellsFromSegment(true, h);
    }
    if (old_v_unknown > verticals[v].unknown && verticals[v].unknown <= 1) {
        enqueueGoodCellsFromSegment(false, v);
    }
}

// Restore a cell as a possible bulb location.
void Algorithm::addCandidate(int i, int j) {
    if (can_bulb[i][j]) return;

    int cell_id = i * c + j + 1;
    int h = horizontal_id[i][j];
    int v = vertical_id[i][j];
    int old_h_unknown = horizontals[h].unknown;
    int old_v_unknown = verticals[v].unknown;

    can_bulb[i][j] = 1;
    horizontals[h].addCandidate(cell_id);
    verticals[v].addCandidate(cell_id);

    // Update available positions around adjacent numbered walls
    for (auto [x, y] : getAdjacent(i, j)) {
        if (NUMBERED_WALL.find(board[x][y]) == std::string::npos) continue;
        num_available[x][y]++;
        enqueueNumberedWall(x, y);
    }

    // Adding a candidate can create a new unique possibility if a segment was empty.
    if (old_h_unknown == 0 && horizontals[h].bulbs == 0) {
        enqueueGoodCellsFromSegment(true, h);
    }
    if (old_v_unknown == 0 && verticals[v].bulbs == 0) {
        enqueueGoodCellsFromSegment(false, v);
    }
}

// Update the cell state and all segment / numbered-wall counters.
void Algorithm::changeCell(int i, int j, char ch) {
    char old_ch = solution[i][j];
    if (old_ch == ch && can_bulb[i][j] == (ch == '.')) return;

    bool old_bulb = old_ch == 'L';
    bool new_bulb = ch == 'L';
    bool old_candidate = can_bulb[i][j];
    bool new_candidate = ch == '.';

    int h = horizontal_id[i][j];
    int v = vertical_id[i][j];
    solution[i][j] = ch;

    // Update bulb counts first, so a newly occupied segment is not scanned for good cells.
    if (old_bulb != new_bulb) {
        int bulb_change = int(new_bulb) - int(old_bulb);
        horizontals[h].bulbs += bulb_change;
        verticals[v].bulbs += bulb_change;

        for (auto [x, y] : getAdjacent(i, j)) {
            if (NUMBERED_WALL.find(board[x][y]) == std::string::npos) continue;
            num_wall[x][y] += bulb_change;
            enqueueNumberedWall(x, y);
        }
    }

    // Update candidate counts and XORs.
    if (old_candidate && !new_candidate) {
        removeCandidate(i, j);
    } else if (!old_candidate && new_candidate) {
        addCandidate(i, j);
    }

    // Removing the last bulb makes the segment unlit again.
    if (old_bulb && !new_bulb) {
        if (horizontals[h].bulbs == 0) {
            enqueueGoodCellsFromSegment(true, h);
        }
        if (verticals[v].bulbs == 0) {
            enqueueGoodCellsFromSegment(false, v);
        }
    }
}

// Change a floor cell and update segment, numbered-wall, and search-score data.
int Algorithm::setCell(int i, int j, char ch) {
    if (!isIn(i, j) || horizontal_id[i][j] == -1) return score;
    if (ch != '.' && ch != 'L') return score;
    if (solution[i][j] == ch && can_bulb[i][j] == (ch == '.')) return score;

    int before = affectedScore(i, j);
    changeCell(i, j, ch);
    int after = affectedScore(i, j);

    score += after - before;
    return score;
}

// Place a bulb, then remove every other candidate in its horizontal and vertical segments.
bool Algorithm::placeBulb(int i, int j) {
    if (solution[i][j] == 'L') return true;
    if (!can_bulb[i][j]) return false;
    if (isLit(i, j)) return false;

    setCell(i, j, 'L');

    // No other bulb can be placed in the same horizontal segment.
    int h = horizontal_id[i][j];
    for (auto [x, y] : horizontals[h].lst) {
        if (can_bulb[x][y]) removeCandidate(x, y);
    }

    // No other bulb can be placed in the same vertical segment.
    int v = vertical_id[i][j];
    for (auto [x, y] : verticals[v].lst) {
        if (can_bulb[x][y]) removeCandidate(x, y);
    }

    return true;
}

//============================= PART1 =============================//
// Place a bulb in every cell that is not already illuminated.
// An unlit cell has no bulb in either of its two segments, so this cannot
// introduce a row/column bulb conflict.
void Algorithm::greedyFill() {
    propagation_active = false;

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (horizontal_id[i][j] == -1 || isLit(i, j)) continue;

            // Deterministic deductions may have removed this candidate.
            if (!can_bulb[i][j]) addCandidate(i, j);
            placeBulb(i, j);
        }
    }
}

//------------------------ Deterministic Algorithm ------------------//
// Queue-based propagation:
// 1. Numbered walls force bulbs / eliminate candidates.
// 2. When a segment reaches 1 or 0 candidates, inspect its cells.
// 3. An unlit cell with exactly one possible bulb forces that bulb.
void Algorithm::deterministicFill() {
    initializePropagation();
    processPropagation();

    propagation_active = false;
    getSegmentData();
}

// Initialize queues and check all constraints that may already be forced.
void Algorithm::initializePropagation() {
    good_cells = std::queue<int>();
    numbered_walls = std::queue<int>();
    good_queued.assign(r, std::vector<unsigned char>(c, 0));
    wall_queued.assign(r, std::vector<unsigned char>(c, 0));
    propagation_active = true;

    // Existing bulbs already occupy their segments; remove other candidates there.
    for (int id = 0; id < int(horizontals.size()); id++) {
        if (horizontals[id].bulbs == 0) continue;
        for (auto [i, j] : horizontals[id].lst) {
            if (can_bulb[i][j]) removeCandidate(i, j);
        }
    }
    for (int id = 0; id < int(verticals.size()); id++) {
        if (verticals[id].bulbs == 0) continue;
        for (auto [i, j] : verticals[id].lst) {
            if (can_bulb[i][j]) removeCandidate(i, j);
        }
    }

    // Every numbered wall is checked once at the beginning.
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            enqueueNumberedWall(i, j);
        }
    }

    // Check segments that already have at most one candidate.
    for (int id = 0; id < int(horizontals.size()); id++) {
        enqueueGoodCellsFromSegment(true, id);
    }
    for (int id = 0; id < int(verticals.size()); id++) {
        enqueueGoodCellsFromSegment(false, id);
    }
}

// Process numbered-wall and good-cell queues until no forced move remains.
void Algorithm::processPropagation() {
    while (!numbered_walls.empty() || !good_cells.empty()) {
        // Process numbered walls first; they have at most four adjacent cells.
        if (!numbered_walls.empty()) {
            int wall_id = numbered_walls.front();
            numbered_walls.pop();

            int i = wall_id / c;
            int j = wall_id % c;
            wall_queued[i][j] = 0;

            int needed = board[i][j] - '0';
            int required = needed - num_wall[i][j];
            int available = num_available[i][j];

            // This constraint is currently impossible to satisfy. Leave it unchanged.
            if (required < 0 || required > available) continue;

            if (required == 0) {
                // No more bulbs are needed around this wall.
                for (auto [x, y] : getAdjacent(i, j)) {
                    if (can_bulb[x][y]) removeCandidate(x, y);
                }
            } else if (required == available) {
                // Every remaining candidate around this wall must be a bulb.
                for (auto [x, y] : getAdjacent(i, j)) {
                    if (!can_bulb[x][y]) continue;
                    // If this placement conflicts with an existing bulb, skip it.
                    placeBulb(x, y);
                }
            }
            continue;
        }

        // Process a cell that may have exactly one possible illuminating bulb.
        int cell_id = good_cells.front();
        good_cells.pop();

        int i = cell_id / c;
        int j = cell_id % c;
        good_queued[i][j] = 0;

        if (isLit(i, j)) continue;

        int possible = possibleLightCount(i, j);
        // No candidate can illuminate this cell. Leave it unresolved.
        if (possible == 0) continue;
        if (possible != 1) continue;

        // The XOR of the union contains the id of the only remaining candidate.
        int candidate_id = possibleLightXor(i, j) - 1;
        if (candidate_id < 0 || candidate_id >= r * c) continue;

        int x = candidate_id / c;
        int y = candidate_id % c;
        if (!can_bulb[x][y]) continue;
        // If the forced placement is no longer valid, skip it and keep processing.
        placeBulb(x, y);
    }
}

//============================= PART2 =============================//
// Simulated annealing. We keep segments for ease keeping of violations
// every floor cell may receive a bulb.

void Algorithm::annealingFill() {
    propagation_active = false;

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
            worker.propagation_active = false;

            std::seed_seq seed(seeds[t].begin(), seeds[t].end());
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> random_probability(0.0, 1.0);

            auto randomInt = [&](int limit) {
                return std::uniform_int_distribution<int>(0, limit - 1)(rng);
            };

            // Use different starting boards instead of repeating the same greedy solution.
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
            worker.greedyFill();
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
    propagation_active = false;
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
    greedyFill();
    getSegmentData();

    int stagnant_rounds = 0;
    for (int round = 0; round < MAX_LNS_ROUNDS; round++) {
        if (Clock::now() >= deadline - std::chrono::seconds(3)) break;
        int before = getViolations();
        if (before == 0) break;
        fixCell();
        int after = getViolations();
        std::cout << "LNS round " << round + 1 << ": " << after << " violations\n";
        if (after >= before) {
            // One empty randomized pass is not proof that no improvement exists.
            if (++stagnant_rounds >= 2) break;
        } else {
            stagnant_rounds = 0;
        }
    }

    // If the repair did not beat our original, restore it.
    if (previous_valid && getViolations() > previous_violations) solution = previous;
    getSegmentData();
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