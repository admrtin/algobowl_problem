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
LIT_WALL_SHARE = 0.8


def generate_input_file(
    rows: int,
    cols: int,
    seed: int,
    output_path: str = "out/",
    plot_solution_grid: bool = False,
    wall_density: float = 0.3,
    numbered_wall_chance: float = 0.7,
) -> list[list[str]]:
    """Write a generated puzzle to `output_path` and return its character grid.

    A directory or a path ending in "/" receives a file named input.txt.
    `numbered_wall_chance` is the fraction of walls that show their adjacent light
    count, drawn LIT_WALL_SHARE/(1-LIT_WALL_SHARE) from walls with and without an adjacent light.
    """
    walls, lights, constraints = generate_solution(
        rows, cols, seed, wall_density, numbered_wall_chance
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
    rows: int, cols: int, seed: int, wall_density: float, numbered_wall_chance: float
) -> tuple[set[Cell], set[Cell], dict[str, set[Cell]]]:
    """Return (unnumbered walls, lights, numbered walls have key "gray_<n>" with n in 0-4)."""
    random.seed(seed)

    walls = generate_walls(rows, cols, density=wall_density)
    lights = generate_lights(rows, cols, walls)
    constraints = generate_constraints(walls, lights, numbered_wall_chance)

    # Remove any blank walls that became numbered in generate_constraints()
    walls -= set().union(*constraints.values())

    return walls, lights, constraints


def generate_light(rows: int, cols: int) -> Cell:
    return (random.randint(0, rows - 1), random.randint(0, cols - 1))


def generate_lights(rows: int, cols: int, walls: set[Cell]) -> set[Cell]:
    """Greedily place lights in random order until every open cell is lit."""
    open_cells = [
        (r, c) for r in range(rows) for c in range(cols) if (r, c) not in walls
    ]
    random.shuffle(open_cells) # shuffle the set of open cells so that we don't place lights in an obvious ordering
    lights: set[Cell] = set()
    lit_cells: set[Cell] = set()

    for cell in open_cells:
        if cell in lit_cells:
            continue
        
        # An unlit cell is seen by no light, so adding a new light here never creates a violation.
        lights.add(cell)
        lit_cells.add(cell)

        # Cast the new light's beam outward until it hits a wall or the grid edge.
        light_r, light_c = cell
        for dr, dc in DIRECTIONS:  # directions are just orthogonal
            r, c = light_r + dr, light_c + dc
            while 0 <= r < rows and 0 <= c < cols and (r, c) not in walls:
                lit_cells.add((r, c))
                r, c = r + dr, c + dc

    return lights


def generate_constraints(
    walls: set[Cell], lights: set[Cell], number_chance: float
) -> dict[str, set[Cell]]:
    """Number some walls with their adjacent light count (0-4), keyed by "gray_<n>".

    `number_chance` of the walls are numbered, (LIT_WALL_SHARE*100)% with an adjacent light and ((1-LIT_WALL_SHARE)*100)%
    without.
    """
    constraints: dict[str, set[Cell]] = {f"gray_{n}": set() for n in range(5)}

    # Sorted so that the sample depends only on the seed.
    lit_walls = sorted(
        (r, c) for r, c in walls if not orthogonal_neighbors(r, c).isdisjoint(lights)
    )
    dark_walls = sorted(walls.difference(lit_walls))

    numbered_count = round(number_chance * len(walls))
    lit_count = min(round(LIT_WALL_SHARE * numbered_count), len(lit_walls))
    dark_count = min(numbered_count - lit_count, len(dark_walls))
    lit_count = numbered_count - dark_count

    for r, c in random.sample(lit_walls, lit_count) + random.sample(
        dark_walls, dark_count
    ):
        adjacent_lights = len(orthogonal_neighbors(r, c) & lights)
        constraints[f"gray_{adjacent_lights}"].add((r, c))

    return constraints


def generate_walls(rows: int, cols: int, density: float) -> set[Cell]:
    """Make each cell a wall independently with probability `density`."""
    return {
        (r, c) for r in range(rows) for c in range(cols) if random.random() < density
    }


def pick_blocker(cells: list[Cell], lights: set[Cell]) -> Cell:
    # Prefer a cell with no adjacent light so it becomes a gray_0.
    cells_without_adjacent_light = [
        (r, c) for r, c in cells if orthogonal_neighbors(r, c).isdisjoint(lights)
    ]
    return random.choice(cells_without_adjacent_light or cells)


def orthogonal_neighbors(r: int, c: int) -> set[Cell]:
    """Return the four adjacent cells, which may fall outside the grid."""
    return {(r + dr, c + dc) for dr, dc in DIRECTIONS}


if __name__ == "__main__":
    generate_input_file(316, 316, 42)
