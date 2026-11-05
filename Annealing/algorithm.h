#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <queue>
#include <string>
#include <utility>
#include <vector>

// One segment representing rows or columns of empty continuous cells.
struct SegmentData {
    int unknown = 0;                              // cells that can still contain a bulb
    int bulbs = 0;                                // bulbs currently in the segment
    int xorCandidates = 0;                        // XOR of (cell id + 1) for all candidates
    std::vector<std::pair<int, int>> lst;         // cells in the segment

    // adds a cell (i, j). candidate = if it still can have a light bulb, cell_id = id of the cell
    void add(int i, int j, bool candidate, bool bulb, int cell_id) {
        lst.push_back({i, j});
        if (candidate) {
            unknown++;
            xorCandidates ^= cell_id;
        }
        bulbs += bulb;
    }

    // if it was candidate, remove from the counter, xor it out
    void removeCandidate(int cell_id) {
        unknown--;
        xorCandidates ^= cell_id;
    }

    // add the candidate, add to the counter, xor it in
    void addCandidate(int cell_id) {
        unknown++;
        xorCandidates ^= cell_id;
    }
};

class Algorithm {
private:
    int r = 0;                                              // number of rows
    int c = 0;                                              // number of columns
    std::vector<std::string> board;                         // original board
    std::vector<std::string> solution;                      // bulbs are 'L'; empty cells '.', all else is same

    // ============= Segment data ==================//
    std::vector<std::vector<int>> horizontal_id;            // -1 for walls, id of the segment
    std::vector<std::vector<int>> vertical_id;              // -1 for walls, id of the segment
    std::vector<SegmentData> horizontals;                   // array of horizontal segments
    std::vector<SegmentData> verticals;                     // array of vertical segments
    std::vector<std::vector<int>> num_wall;                 // current bulbs adjacent to each numbered wall
    std::vector<std::vector<int>> num_available;            // remaining candidate cells adjacent to each numbered wall
    std::vector<std::vector<unsigned char>> can_bulb;       // 2D array that tells if (i, j) can still be the bulb

    // =========== Deterministic queues ============//
    std::queue<int> good_cells;                             // cells to consider next in the deterministic solution
    std::queue<int> numbered_walls;                         // walls to consider next in the deterministic solution
    std::vector<std::vector<unsigned char>> good_queued;    // mark cells if it is in the queue, to prevent repeats
    std::vector<std::vector<unsigned char>> wall_queued;    // mark walls if they are in the queue to prevent repeats
    bool propagation_active = false;

    // Search score = verifier violations + currently unilluminated cells.
    // The verifier itself does NOT count unilluminated cells; it rejects them separately.
    int score = 0;

    int cellScore(int i, int j);
    int affectedScore(int i, int j);
    int windowScore(int top, int left, int bottom, int right);
    bool isViolationCell(int i, int j);

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
