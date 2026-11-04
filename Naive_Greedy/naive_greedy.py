"""Naive greedy solver for the Light Up (Akari) AlgoBowl problem.

Usage:
    python algorithms/naive_greedy.py [input] [-o output]

Writes the violation count followed by the solved grid (bulbs as 'L').
Timing information is printed to stderr so the output stays verifier-compatible.
"""

import argparse
import sys
import time
from pathlib import Path

NUMBERS = "01234"
DIRS = ((-1, 0), (1, 0), (0, -1), (0, 1))


def read_board(path):
    tokens = Path(path).read_text().split()
    r, c = int(tokens[0]), int(tokens[1])
    grid = tokens[2:2 + r]
    if len(grid) != r or any(len(row) != c for row in grid):
        raise ValueError(f"malformed input: expected {r} rows of {c} characters")
    return r, c, grid


def build_segments(r, c, grid):
    """Split blank cells into maximal horizontal/vertical runs bounded by walls."""
    n = r * c
    row_seg = [-1] * n
    col_seg = [-1] * n
    segments = []

    for i in range(r):
        cur = None
        for j in range(c):
            if grid[i][j] == ".":
                if cur is None:
                    cur = []
                    segments.append(cur)
                idx = i * c + j
                cur.append(idx)
                row_seg[idx] = len(segments) - 1
            else:
                cur = None

    for j in range(c):
        cur = None
        for i in range(r):
            if grid[i][j] == ".":
                if cur is None:
                    cur = []
                    segments.append(cur)
                idx = i * c + j
                cur.append(idx)
                col_seg[idx] = len(segments) - 1
            else:
                cur = None

    return row_seg, col_seg, segments


def solve(r, c, grid):
    n = r * c
    row_seg, col_seg, segments = build_segments(r, c, grid)

    need = {}         # numbered cell -> required bulb count
    blank_nbrs = {}   # numbered cell -> adjacent blank cells
    num_nbrs = [()] * n  # blank cell -> adjacent numbered cells
    tmp_num_nbrs = {}

    for i in range(r):
        for j in range(c):
            ch = grid[i][j]
            if ch not in NUMBERS:
                continue
            m = i * c + j
            need[m] = int(ch)
            nbrs = []
            for di, dj in DIRS:
                ni, nj = i + di, j + dj
                if 0 <= ni < r and 0 <= nj < c and grid[ni][nj] == ".":
                    k = ni * c + nj
                    nbrs.append(k)
                    tmp_num_nbrs.setdefault(k, []).append(m)
            blank_nbrs[m] = nbrs
    for k, ms in tmp_num_nbrs.items():
        num_nbrs[k] = tuple(ms)

    bulb = bytearray(n)
    lit = bytearray(n)
    adj = {m: 0 for m in need}
    unlit_in_seg = [len(s) for s in segments]

    def place(idx):
        bulb[idx] = 1
        for s in (row_seg[idx], col_seg[idx]):
            for k in segments[s]:
                if not lit[k]:
                    lit[k] = 1
                    unlit_in_seg[row_seg[k]] -= 1
                    unlit_in_seg[col_seg[k]] -= 1
        for m in num_nbrs[idx]:
            adj[m] += 1

    def penalty(k):
        return sum(1 for m in num_nbrs[k] if adj[m] >= need[m])

    def bonus(k):
        return sum(1 for m in num_nbrs[k] if adj[m] < need[m])

    def coverage(k):
        return unlit_in_seg[row_seg[k]] + unlit_in_seg[col_seg[k]] - 1

    # Phase 1: satisfy numbered cells, most constrained (least slack) first.
    order = sorted(need, key=lambda m: (len(blank_nbrs[m]) - need[m], -need[m]))
    for m in order:
        while adj[m] < need[m]:
            cands = [k for k in blank_nbrs[m] if not lit[k] and penalty(k) == 0]
            if not cands:
                break
            place(max(cands, key=lambda k: (bonus(k), coverage(k))))

    # Phase 2: illuminate every remaining dark cell.
    for idx in range(n):
        if row_seg[idx] == -1 or lit[idx]:
            continue
        cands = [k for k in segments[row_seg[idx]] if not lit[k]]
        cands += [k for k in segments[col_seg[idx]] if not lit[k] and k != idx]
        place(min(cands, key=lambda k: (penalty(k), -bonus(k), -coverage(k))))

    assert all(lit[k] for k in range(n) if row_seg[k] != -1), "unlit blank cell"
    return bulb, segments


def count_violations(r, c, grid, bulb, segments):
    """Count violating cells exactly as the verifier does."""
    viol = bytearray(r * c)
    for seg in segments:
        bulbs_in = [k for k in seg if bulb[k]]
        if len(bulbs_in) > 1:
            for k in bulbs_in:
                viol[k] = 1
    for i in range(r):
        for j in range(c):
            ch = grid[i][j]
            if ch not in NUMBERS:
                continue
            cnt = 0
            for di, dj in DIRS:
                ni, nj = i + di, j + dj
                if 0 <= ni < r and 0 <= nj < c and bulb[ni * c + nj]:
                    cnt += 1
            if cnt != int(ch):
                viol[i * c + j] = 1
    return sum(viol)


def render(r, c, grid, bulb):
    return [
        "".join("L" if bulb[i * c + j] else grid[i][j] for j in range(c))
        for i in range(r)
    ]


def main():
    parser = argparse.ArgumentParser(description="Naive greedy Light Up solver")
    parser.add_argument("input", nargs="?", default="out/input.txt")
    parser.add_argument("-o", "--output", default="out/output.txt",
                        help="output file path, or '-' for stdout")
    args = parser.parse_args()

    t0 = time.perf_counter()
    r, c, grid = read_board(args.input)
    t1 = time.perf_counter()
    bulb, segments = solve(r, c, grid)
    t2 = time.perf_counter()
    violations = count_violations(r, c, grid, bulb, segments)
    text = "\n".join([str(violations)] + render(r, c, grid, bulb)) + "\n"
    if args.output == "-":
        sys.stdout.write(text)
    else:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text)
    t3 = time.perf_counter()

    print(
        f"{r}x{c} board | bulbs: {sum(bulb)} | violations: {violations}\n"
        f"read: {t1 - t0:.3f}s | solve: {t2 - t1:.3f}s | "
        f"score+write: {t3 - t2:.3f}s | total: {t3 - t0:.3f}s",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()