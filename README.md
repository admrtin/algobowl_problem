# algobowl_problem
CSCI 406 AlgoBowl Group Project

# Setup

Install [uv](https://docs.astral.sh/uv/getting-started/installation/). `uv run`
creates the virtual environment and installs the Python version and dependencies
the first time it runs, so there is no separate install step.

# Input

## Workflow: 

The generator builds a solved puzzle and then removes the lights, so every input
has at least one valid solution:
1. Walls: each cell independently becomes a wall with probability `--wall_density`.
2. Lights: open cells are visited in random order, and a light is placed on each
   cell that is still unlit. No two lights can see each other, and every open cell
   ends up lit.
3. Numbered walls: a `--numbered_wall_chance` fraction of the walls are numbered with
   their count of adjacent lights. `--numbered_wall_<n>_density` sets the share of
   numbered walls that show `n`. If too few walls touch exactly `n` lights, the
   remainder goes to the other numbers.
4. Light removal: Once walls and numbered walls are placed, lights are removed from
   the solution to produce the input. At this point, the input file is written to disk.

## Generate an input file

```bash
uv run input_generator.py --rows 30 --cols 30 --seed 42 --output_path out/
```

Defaults are set so you don't have to specify certain arguments:

| Option                   | Default | Description                                                  |
| ------------------------ | ------- | ------------------------------------------------------------ |
| `--rows`                 | `316`   | Number of grid rows.                                         |
| `--cols`                 | `316`   | Number of grid columns.                                      |
| `--seed`                 | `1234`  | Random seed. The same options and seed give the same file.   |
| `--output_path`          | `out/`  | Output file, or a directory to receive `input.txt`.          |
| `--wall_density`         | `0.3`   | Probability from 0 to 1 that a cell is a wall.               |
| `--numbered_wall_chance` | `0.7`   | Fraction of walls, from 0 to 1, that are numbered.           |
| `--numbered_wall_0_density` | `0.2` | Share of numbered walls that require `0` adjacent lights. The five densities must sum to 1. |
| `--numbered_wall_1_density` | `0.53` | Share of numbered walls that require `1` adjacent light. |
| `--numbered_wall_2_density` | `0.21` | Share of numbered walls that require `2` adjacent lights. |
| `--numbered_wall_3_density` | `0.05` | Share of numbered walls that require `3` adjacent lights. |
| `--numbered_wall_4_density` | `0.01` | Share of numbered walls that require `4` adjacent lights. |
| `--plot_solution_grid`   | off     | Display the generated solution to the input file in an interactive plot.       |


# Solve an input file

```bash
uv run algorithms/naive_greedy.py out/input.txt -o out/output.txt
```

These paths are also the defaults. Use `-o -` to print the solution to stdout.
Timing information is printed to stderr.
