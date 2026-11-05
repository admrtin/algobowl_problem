#include "algorithm.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

namespace {

constexpr std::string SKIP_CHARACTERS = "X01234";
constexpr std::string NUMBERED_WALL = "01234";

constexpr int NUM_DIRECTIONS = 4;
constexpr int di[NUM_DIRECTIONS] = {-1, 1, 0, 0};
constexpr int dj[NUM_DIRECTIONS] = {0, 0, -1, 1};

bool isNumberedWall(char ch) {
    return ch >= '0' && ch <= '4';
}
} // namespace


// part 2 constants 
constexpr double INITIAL_TEMPERATURE = 1000.0;          // T for anneling approach
constexpr double FINAL_TEMPERATURE = 0.1;               // Limit T above
constexpr int ANNEALING_ITERATIONS = 200'000;           // number of itterations

// part 3 contants
constexpr int MAX_WINDOW_SIZE = 11;                     // max square window nxn                     
constexpr int MAX_VISITED_NODES = 50000;                // max nodes in one dfs tree of nodes

constexpr int MAX_LNS_ROUNDS = 5;                       // the number of times to run LNS

// constructor takes the input_file to read
Algorithm::Algorithm(const char* input_file) {
    static bool random_seeded = false;
    if (!random_seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        random_seeded = true;
    }

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
    score = getViolations();
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            // adjust if cell is unlit
            if (board[i][j] == '.' && !isLit(i, j)) score++;
        }
    }
}

// Search penalty from one cell, currently unlit.
int Algorithm::cellScore(int i, int j) {
    if (solution[i][j] == 'L') {
        return horizontals[horizontal_id[i][j]].bulbs > 1 ||
               verticals[vertical_id[i][j]].bulbs > 1;
    }
    return !isLit(i, j);
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
            affected += num_wall[x][y] != board[x][y] - '0';
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
    score = getViolations();
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] == '.' && !isLit(i, j)) score++;
        }
    }
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

    std::vector<int> open_cells;
    std::vector<int> bulb_cells;
    std::vector<int> bulb_position(r * c, -1);

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (board[i][j] != '.') continue;
            int cell_id = i * c + j;
            open_cells.push_back(cell_id);
            if (solution[i][j] == 'L') {
                bulb_position[cell_id] = int(bulb_cells.size());
                bulb_cells.push_back(cell_id);
            }
        }
    }
    if (open_cells.empty()) return;

    auto addBulb = [&](int cell_id) {
        if (bulb_position[cell_id] != -1) return;
        bulb_position[cell_id] = int(bulb_cells.size());
        bulb_cells.push_back(cell_id);
    };

    auto removeBulb = [&](int cell_id) {
        int position = bulb_position[cell_id];
        if (position == -1) return;
        int last_cell = bulb_cells.back();
        bulb_cells[position] = last_cell;
        bulb_position[last_cell] = position;
        bulb_cells.pop_back();
        bulb_position[cell_id] = -1;
    };

    auto syncBulb = [&](int cell_id) {
        int i = cell_id / c;
        int j = cell_id % c;
        if (solution[i][j] == 'L') addBulb(cell_id);
        else removeBulb(cell_id);
    };

    int iterations = ANNEALING_ITERATIONS;
    double cooling_rate = std::pow(FINAL_TEMPERATURE / INITIAL_TEMPERATURE,
                                   1.0 / std::max(1, iterations - 1));

    auto acceptChange = [&](int delta, int t) {
        if (delta >= 0) return true;

        double temperature = INITIAL_TEMPERATURE * std::pow(cooling_rate, t);
        temperature = std::max(temperature, 1e-9);

        double probability = std::exp(static_cast<double>(delta) / temperature);
        double random_value = static_cast<double>(std::rand()) / (static_cast<double>(RAND_MAX) + 1.0);
        return random_value < probability;
    };

    std::vector<std::string> best_solution = solution;
    int best_score = score;

    for (int t = 0; t < iterations; t++) {
        int old_score = score;
        std::vector<int> changed_cells;
        std::vector<char> old_states;

        auto change = [&](int i, int j, char ch) {
            if (solution[i][j] == ch) return;
            changed_cells.push_back(i * c + j);
            old_states.push_back(solution[i][j]);
            setCell(i, j, ch);
        };

        int move_type = bulb_cells.empty() ? 0 : std::rand() % 4;

        if (move_type == 0) {
            // Add a bulb at a random empty cell.
            int cell_id = open_cells[std::rand() % open_cells.size()];
            change(cell_id / c, cell_id % c, 'L');
        } else {
            int source_id = bulb_cells[std::rand() % bulb_cells.size()];
            int i = source_id / c;
            int j = source_id % c;

            if (move_type == 1) {
                // Remove a bulb.
                change(i, j, '.');
            } else if (move_type == 2) {
                // Move a bulb to an orthogonally adjacent empty cell.
                int direction = std::rand() % NUM_DIRECTIONS;
                int ni = i + di[direction];
                int nj = j + dj[direction];
                if (isIn(ni, nj) && board[ni][nj] == '.' && solution[ni][nj] != 'L') {
                    change(i, j, '.');
                    change(ni, nj, 'L');
                }
            } else {
                // Move a bulb within its row/column segment next to a numbered wall.
                std::vector<int> destinations;
                std::vector<unsigned char> seen(r * c, 0);

                auto addDestinations = [&](const SegmentData& segment) {
                    for (auto [x, y] : segment.lst) {
                        int destination_id = x * c + y;
                        if (destination_id == source_id || seen[destination_id]) continue;
                        if (board[x][y] != '.' || solution[x][y] == 'L') continue;

                        bool next_to_numbered_wall = false;
                        for (auto [wx, wy] : getAdjacent(x, y)) {
                            if (isNumberedWall(board[wx][wy])) {
                                next_to_numbered_wall = true;
                                break;
                            }
                        }
                        if (next_to_numbered_wall) {
                            seen[destination_id] = 1;
                            destinations.push_back(destination_id);
                        }
                    }
                };

                addDestinations(horizontals[horizontal_id[i][j]]);
                addDestinations(verticals[vertical_id[i][j]]);

                if (!destinations.empty()) {
                    int destination_id = destinations[std::rand() % destinations.size()];
                    change(i, j, '.');
                    change(destination_id / c, destination_id % c, 'L');
                }
            }
        }

        if (changed_cells.empty()) continue;

        int delta = old_score - score;
        if (acceptChange(delta, t)) {
            for (int cell_id : changed_cells) syncBulb(cell_id);
            if (score < best_score) {
                best_score = score;
                best_solution = solution;
            }
        } else {
            for (int k = int(changed_cells.size()) - 1; k >= 0; k--) {
                int cell_id = changed_cells[k];
                setCell(cell_id / c, cell_id % c, old_states[k]);
            }
        }
    }

    solution = best_solution;
    getSegmentData();
}

//============================= PART3 =============================//
// Count score contributed by cells inside [top, bottom) x [left, right).

int Algorithm::windowScore(int top, int left, int bottom, int right) {
    int total = 0;

    for (int i = top; i < bottom; i++) {
        for (int j = left; j < right; j++) {
            if (board[i][j] == 'X') continue;

            if (NUMBERED_WALL.find(board[i][j]) != std::string::npos) {
                total += num_wall[i][j] != board[i][j] - '0';
            } else {
                total += cellScore(i, j);
            }
        }
    }

    return total;
}

// Check whether a cell currently contributes at least one violation.
bool Algorithm::isViolationCell(int i, int j) {
    if (!isIn(i, j) || board[i][j] == 'X') return false;

    if (NUMBERED_WALL.find(board[i][j]) != std::string::npos) {
        return board[i][j] - '0' != num_wall[i][j];
    }

    return cellScore(i, j) > 0;
}

// Try to repair each currently violating cell using increasingly large windows.
void Algorithm::fixCell() {
    int visited_nodes = 0;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            if (!isViolationCell(i, j)) continue;

            for (int n = 3; n <= MAX_WINDOW_SIZE && visited_nodes < MAX_VISITED_NODES; n += 2) {
                int half = n / 2;
                int top = std::max(0, i - half);
                int left = std::max(0, j - half);
                int bottom = std::min(r, i + half + 1);
                int right = std::min(c, j + half + 1);

                int initial_window_score = windowScore(top, left, bottom, right);
                if (initial_window_score == 0) break;

                std::vector<int> window_cells;
                for (int x = top; x < bottom; x++) {
                    for (int y = left; y < right; y++) {
                        if (board[x][y] == '.') window_cells.push_back(x * c + y);
                    }
                }
                if (window_cells.empty()) continue;

                int global_before = score;
                int best_window_score = initial_window_score;
                int best_global_score = global_before;
                std::vector<char> original_state;
                std::vector<char> best_state;
                for (int cell_id : window_cells) {
                    original_state.push_back(solution[cell_id / c][cell_id % c]);
                }

                // DFS explores sequences of toggles. A branch is extended only when
                // the current square is already better than it was at the start.
                auto dfs = [&](auto&& self, int start, int depth) -> void {
                    if (visited_nodes >= MAX_VISITED_NODES) return;
                    visited_nodes++;

                    int current_window_score = windowScore(top, left, bottom, right);
                    if (current_window_score < best_window_score &&
                        score < global_before) {
                        best_window_score = current_window_score;
                        best_global_score = score;
                        best_state.clear();
                        for (int cell_id : window_cells) {
                            best_state.push_back(solution[cell_id / c][cell_id % c]);
                        }
                    } else if (current_window_score == best_window_score &&
                               current_window_score < initial_window_score &&
                               score < best_global_score) {
                        best_global_score = score;
                        best_state.clear();
                        for (int cell_id : window_cells) {
                            best_state.push_back(solution[cell_id / c][cell_id % c]);
                        }
                    }

                    // Do not explore a branch that has not improved the original square.
                    if (depth > 0 && current_window_score >= initial_window_score) return;

                    for (int k = start; k < int(window_cells.size()); k++) {
                        if (visited_nodes >= MAX_VISITED_NODES) return;

                        int cell_id = window_cells[k];
                        int x = cell_id / c;
                        int y = cell_id % c;
                        char old_state = solution[x][y];
                        char new_state = old_state == 'L' ? '.' : 'L';

                        setCell(x, y, new_state);
                        self(self, k + 1, depth + 1);
                        setCell(x, y, old_state);
                    }
                };
                dfs(dfs, 0, 0);

                // All DFS branches have been rolled back. Apply the best improving state.
                if (!best_state.empty()) {
                    for (int k = 0; k < int(window_cells.size()); k++) {
                        int cell_id = window_cells[k];
                        setCell(cell_id / c, cell_id % c, best_state[k]);
                    }

                    // Do not keep a locally better square if it worsens the global score.
                    if (score >= global_before) {
                        for (int k = 0; k < int(window_cells.size()); k++) {
                            int cell_id = window_cells[k];
                            setCell(cell_id / c, cell_id % c, original_state[k]);
                        }
                    }
                }

                // If the center cell is fixed, a larger window is unnecessary.
                if (!isViolationCell(i, j)) break;
            }

            if (visited_nodes >= MAX_VISITED_NODES) return;
        }
    }
}

// Run several LNS passes, stopping when a pass cannot improve the global score.
void Algorithm::windowFill() {
    propagation_active = false;

    for (int round = 0; round < MAX_LNS_ROUNDS; round++) {
        int before = score;
        fixCell();
        if (score >= before) break;
    }

    // Rebuild candidate information after the bulb configuration changes.
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