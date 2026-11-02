"""Generate puzzle input files that are guaranteed to have a valid solution.

The solution is built first (walls, then lights, then numbered numbered_walls that
match those lights), and the input file is that solution with the lights removed.
"""

import math
import random
from pathlib import Path

from ..plot.plot_solution_grid import ENCODING
from ..plot.plot_solution_grid import plot_solution_grid as show_solution_plot

Cell = tuple[int, int]  # (row, col)

# orthogonal unit directions from the current cell.
DIRECTIONS = ((1, 0), (-1, 0), (0, 1), (0, -1))

# Share of numbered walls showing each number 0-4. Must sum to 1.
NUMBERED_WALL_DENSITIES = (0.2, 0.53, 0.21, 0.05, 0.01)


def generate_input_file(
    rows: int,
    cols: int,
    seed: int,
    output_path: str = "out/",
    plot_solution_grid: bool = False,
    wall_density: float = 0.3,
    numbered_wall_chance: float = 0.7,
    numbered_wall_densities: tuple[float, ...] = NUMBERED_WALL_DENSITIES,
) -> list[list[str]]:
    """Write a generated puzzle to `output_path` and return its character grid.

    A directory or a path ending in "/" receives a file named input.txt.
    `numbered_wall_chance` is the fraction of walls that show their adjacent light
    count. `numbered_wall_densities[n]` is the share of those that show n.
    """
    walls, lights, numbered_walls = generate_solution(
        rows, cols, seed, wall_density, numbered_wall_chance, numbered_wall_densities
    )

    # lights are left out of the input grid
    grid = [[ENCODING["blank"] for _ in range(cols)] for _ in range(rows)]
    for r, c in walls:
        grid[r][c] = ENCODING["gray"]
    for numbered_wall_key, cells in numbered_walls.items():
        for r, c in cells:
            grid[r][c] = ENCODING[numbered_wall_key]

    # WRITING/PLOTTING
    path = Path(output_path)
    if path.is_dir() or str(output_path).endswith("/"):
        path /= "input.txt"
    path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Wrote input file to {path}")

    # write the input file.
    with path.open("w", encoding="utf-8") as file:
        file.write(f"{rows} {cols}\n")
        for row in grid:
            file.write("".join(row) + "\n")

    # interactively plot the solution with matplotlib.
    if plot_solution_grid:
        show_solution_plot(rows, cols, walls, lights, numbered_walls)

    return grid


def generate_solution(
    rows: int,
    cols: int,
    seed: int,
    wall_density: float,
    numbered_wall_chance: float,
    numbered_wall_densities: tuple[float, ...] = NUMBERED_WALL_DENSITIES,
) -> tuple[set[Cell], set[Cell], dict[str, set[Cell]]]:
    """Return (unnumbered walls, lights, numbered walls have key "gray_<n>" with n in 0-4)."""
    random.seed(seed)

    walls = generate_walls(rows, cols, density=wall_density)
    lights = generate_lights(rows, cols, walls)
    numbered_walls = generate_numbered_walls(walls, lights, numbered_wall_chance, numbered_wall_densities)

    # Remove any blank walls that became numbered in generate_numbered_walls()
    walls -= set().union(*numbered_walls.values()) # need to union to merge all "gray_X" sets into one

    return walls, lights, numbered_walls


def generate_light(rows: int, cols: int) -> Cell:
    return (random.randint(0, rows - 1), random.randint(0, cols - 1))


def generate_lights(rows: int, cols: int, walls: set[Cell]) -> set[Cell]:
    """Greedily place lights in random order until every open cell is lit."""
    open_cells = []

    for r in range(rows):
        for c in range(cols):
            if (r,c) not in walls:
                open_cells.append((r,c))
            
    random.shuffle(open_cells) # shuffle the set of open cells so that we don't place lights in an obvious ordering
    lights: set[Cell] = set()
    lit_cells: set[Cell] = set()

    for cell in open_cells:
        if cell in lit_cells:
            continue
        
        # An unlit cell is seen by no light, so adding a new light here never creates a violation.
        lights.add(cell) # add the light
        lit_cells.add(cell)

        # Cast the new light's beam outward until it hits a wall or the grid edge.
        light_r, light_c = cell
        for dr, dc in DIRECTIONS:  # directions are just orthogonal
            r, c = light_r + dr, light_c + dc
            while 0 <= r < rows and 0 <= c < cols and (r, c) not in walls:
                lit_cells.add((r, c))
                r, c = r + dr, c + dc # try to go to the next row/col and re-test.

    return lights


def generate_numbered_walls(
    walls: set[Cell],
    lights: set[Cell],
    numbered_wall_chance: float,
    numbered_wall_densities: tuple[float, ...] = NUMBERED_WALL_DENSITIES,
) -> dict[str, set[Cell]]:
    """Number some walls with their adjacent light count (0-4), keyed by "gray_<n>".

    `numbered_wall_chance` of the walls are numbered, and `numbered_wall_densities[n]`
    of those show n.
    """
    if len(numbered_wall_densities) != 5 or min(numbered_wall_densities) < 0:
        raise ValueError("numbered_wall_densities needs five non-negative values, one per number 0-4")
    if not math.isclose(sum(numbered_wall_densities), 1.0):
        raise ValueError(f"numbered_wall_densities must sum to 1, got {sum(numbered_wall_densities)}")

    # each item in the dictionary is a set corresponding to "gray_0"-"gray_4" numbered_wall cells
    numbered_walls: dict[str, set[Cell]] = {}

    # walls_by_count[n] holds the walls with exactly n adjacent lights, the only walls that can show n
    walls_by_count: dict[int, list[Cell]] = {}
    for n in range(5):
        numbered_walls[f"gray_{n}"] = set()
        walls_by_count[n] = []

    for r, c in sorted(walls):
        adjacent_lights = len(orthogonal_neighbors(r, c) & lights) # determines the # of adjacent lights at this wall.
        walls_by_count[adjacent_lights].append((r, c))

    # Determine the number of walls to be converted to numbered_walls
    numbered_count = round(numbered_wall_chance * len(walls))

    # Give each number its share of the numbered walls, capped at the walls that can show it
    for n in range(5):
        count = round(numbered_wall_densities[n] * numbered_count)
        count = min(count, len(walls_by_count[n]))
        numbered_walls[f"gray_{n}"] = set(random.sample(walls_by_count[n], count))

    return numbered_walls


def generate_walls(rows: int, cols: int, density: float) -> set[Cell]:
    """Make each cell a wall independently with probability `density`."""
    walls: set[Cell] = set()

    for r in range (rows):
        for c in range(cols):
            if random.random() < density:
                walls.add((r,c))

    return walls


def orthogonal_neighbors(r: int, c: int) -> set[Cell]:
    """Return the four adjacent cells, which may fall outside the grid."""
    neighbors: set[Cell] = set()

    for dr, dc in DIRECTIONS:
        neighbors.add((r + dr, c + dc))
    
    return neighbors


if __name__ == "__main__":
    generate_input_file(316, 316, 42)
