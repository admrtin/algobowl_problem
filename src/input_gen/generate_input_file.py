"""Generate puzzle input files that are guaranteed to have a valid solution.

The solution is built first (walls, then lights, then numbered constraints that
match those lights), and the input file is that solution with the lights removed.
"""

import random
from pathlib import Path

from ..plot.plot_solution_grid import ENCODING
from ..plot.plot_solution_grid import plot_solution_grid as show_solution_plot

Cell = tuple[int, int]  # (row, col)

# orthogonal unit directions from the current cell.
DIRECTIONS = ((1, 0), (-1, 0), (0, 1), (0, -1))

# Share of numbered walls drawn from walls with an adjacent light.
ADJ_TO_LIGHT_WALL_SHARE = 0.8


def generate_input_file(
    rows: int,
    cols: int,
    seed: int,
    output_path: str = "out/",
    plot_solution_grid: bool = False,
    wall_density: float = 0.3,
    numbered_wall_chance: float = 0.7,
    adj_to_light_wall_share: float = ADJ_TO_LIGHT_WALL_SHARE,
) -> list[list[str]]:
    """Write a generated puzzle to `output_path` and return its character grid.

    A directory or a path ending in "/" receives a file named input.txt.
    `numbered_wall_chance` is the fraction of walls that show their adjacent light
    count. `adj_to_light_wall_share` of those are drawn from walls with an
    adjacent light, and the rest from walls without one.
    """
    walls, lights, constraints = generate_solution(
        rows, cols, seed, wall_density, numbered_wall_chance, adj_to_light_wall_share
    )

    # lights are left out of the input grid
    grid = [[ENCODING["blank"] for _ in range(cols)] for _ in range(rows)]
    for r, c in walls:
        grid[r][c] = ENCODING["gray"]
    for constraint_key, cells in constraints.items():
        for r, c in cells:
            grid[r][c] = ENCODING[constraint_key]

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
        show_solution_plot(rows, cols, walls, lights, constraints)

    return grid


def generate_solution(
    rows: int,
    cols: int,
    seed: int,
    wall_density: float,
    numbered_wall_chance: float,
    adj_to_light_wall_share: float = ADJ_TO_LIGHT_WALL_SHARE,
) -> tuple[set[Cell], set[Cell], dict[str, set[Cell]]]:
    """Return (unnumbered walls, lights, numbered walls have key "gray_<n>" with n in 0-4)."""
    random.seed(seed)

    walls = generate_walls(rows, cols, density=wall_density)
    lights = generate_lights(rows, cols, walls)
    constraints = generate_numbered_walls(walls, lights, numbered_wall_chance, adj_to_light_wall_share)

    # Remove any blank walls that became numbered in generate_constraints()
    walls -= set().union(*constraints.values()) # need to union to merge all "gray_X" sets into one

    return walls, lights, constraints


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
    adj_to_light_wall_share: float = ADJ_TO_LIGHT_WALL_SHARE,
) -> dict[str, set[Cell]]:
    """Number some walls with their adjacent light count (0-4), keyed by "gray_<n>".

    `numbered_wall_chance` of the walls are numbered: `adj_to_light_wall_share` of those
    have an adjacent light and the rest have none.
    """
    # each item in the dictionary is a set corresponding to "gray_0"-"gray_4" constraint cells
    numbered_walls: dict[str, set[Cell]] = {f"gray_{n}": set() for n in range(5)}

    walls_adj_to_light: set[Cell] # walls that are orthogonally adjacent to a light.
    walls_not_adj_to_light: set[Cell] # walls that are not orthogonally adjacent to a light
    
    # Will be replaced with constraints from 1-4 (the required number of adjacent lights)
    walls_adj_to_light = sorted(
        (r, c) for r, c in walls if not orthogonal_neighbors(r, c).isdisjoint(lights)
    )
    # Will be replaced with constraint 0 (no light can be placed adjacent to it)
    walls_not_adj_to_light = sorted(walls.difference(walls_adj_to_light))

    # Determine the number of walls to be converted to constraints based on the input probabilities
    # numbered_wall_chance: the probability that blank walls will be converted to numbered walls (constraints)
    # adj_to_light_wall_share: the probability that converted walls are adjacent to lights
    numbered_count = round(numbered_wall_chance * len(walls))
    adj_count = min(round(adj_to_light_wall_share * numbered_count), len(walls_adj_to_light))
    not_adj_count = min(numbered_count - adj_count, len(walls_not_adj_to_light))
    adj_count = numbered_count - not_adj_count

    # randomly sample a wall from either set & add constraints.
    for r, c in random.sample(walls_adj_to_light, adj_count) + random.sample(
        walls_not_adj_to_light, not_adj_count
    ):
        adjacent_lights = len(orthogonal_neighbors(r, c) & lights)
        numbered_walls[f"gray_{adjacent_lights}"].add((r, c))

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
