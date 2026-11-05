#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <chrono>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// One uninterrupted row or column of empty cells.
struct SegmentData {
    int unknown = 0;                              // cells that can still contain a bulb
    int bulbs = 0;                                // bulbs currently in the segment
    int xorCandidates = 0;                        // XOR of (cell id + 1) for all candidates
    std::vector<std::pair<int, int>> lst;         // cells in the segment

    void add(int i, int j, bool candidate, bool bulb, int cell_id) {
        lst.push_back({i, j});
        if (candidate) {
            unknown++;
            xorCandidates ^= cell_id;
        }
        bulbs += bulb;
    }

    void removeCandidate(int cell_id) {
        unknown--;
        xorCandidates ^= cell_id;
    }

    void addCandidate(int cell_id) {
        unknown++;
        xorCandidates ^= cell_id;
    }
};

class Algorithm {
private:
    int r = 0;                                      // number of rows
    int c = 0;                                      // number of columns
    std::vector<std::string> board;                 // original puzzle
    std::vector<std::string> solution;              // bulbs are 'L'; empty cells remain '.'

    // ============= Segment data ==================
    std::vector<std::vector<int>> horizontal_id;    // -1 for walls
    std::vector<std::vector<int>> vertical_id;      // -1 for walls
    std::vector<SegmentData> horizontals;
    std::vector<SegmentData> verticals;
    std::vector<std::vector<int>> num_wall;         // current bulbs adjacent to each numbered wall
    std::vector<std::vector<int>> num_available;    // remaining candidate cells adjacent to each numbered wall
    std::vector<std::vector<unsigned char>> can_bulb;

    // ============= Deterministic queues =================
    std::queue<int> good_cells;
    std::queue<int> numbered_walls;
    std::vector<std::vector<unsigned char>> good_queued;
    std::vector<std::vector<unsigned char>> wall_queued;
    bool propagation_active = false;

    // Search score = verifier violations + currently unilluminated cells.
    // The verifier itself does NOT count unilluminated cells; it rejects them separately.
    int score = 0;
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    std::chrono::steady_clock::time_point annealing_deadline = deadline;

    int wallScore(int i, int j);
    int cellScore(int i, int j);
    int affectedScore(int i, int j);

    void removeCandidate(int i, int j);
    void addCandidate(int i, int j);
    void changeCell(int i, int j, char ch);

    int possibleLightCount(int i, int j);
    int possibleLightXor(int i, int j);

    void enqueueGoodCell(int i, int j);
    void enqueueNumberedWall(int i, int j);
    void enqueueGoodCellsFromSegment(bool horizontal, int id);
    void initializePropagation();
    void processPropagation();
    bool placeBulb(int i, int j);

public:
    explicit Algorithm(const char* input_file);
    void output(const char* output_file);
    bool loadSolution(const char* filename);
    void setDeadlines(std::chrono::steady_clock::time_point annealing_end,
                      std::chrono::steady_clock::time_point overall_end);

    // Exact number of violations as defined by verifier.cpp.
    // Unilluminated cells are not included here; output() ensures there are none.
    int getViolations();

    void getSegmentData();
    int setCell(int i, int j, char ch);

    bool isLit(int i, int j) {
        if (!isIn(i, j) || horizontal_id[i][j] == -1) return false;
        return horizontals[horizontal_id[i][j]].bulbs > 0 ||
               verticals[vertical_id[i][j]].bulbs > 0;
    }

    bool isIn(int i, int j) {
        return 0 <= i && i < r && 0 <= j && j < c;
    }

    // ============= PART 1 =============
    void greedyFill();
    void deterministicFill();

    // ============= PART 2 =============
    void annealingFill();

    // ============= PART 3 =============
    void windowFill();
    void fixCell();

    std::vector<std::pair<int, int>> getAdjacent(int i, int j);
};

#endif // ALGORITHM_H
