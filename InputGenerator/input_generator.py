import argparse
import math

from src.input_gen.generate_input_file import NUMBERED_WALL_DENSITIES, generate_input_file

parser = argparse.ArgumentParser()
parser.add_argument("--rows", type=int, default=316)
parser.add_argument("--cols", type=int, default=316)
parser.add_argument("--seed", type=int, default=1234)
parser.add_argument("--output_path", type=str, default = "out/")
parser.add_argument("--plot_solution_grid", action="store_true")
parser.add_argument("--plot_input_grid", action="store_true")
parser.add_argument("--wall_density", type=float, default=0.3)
parser.add_argument("--numbered_wall_chance", type=float, default=0.7)
for n, density in enumerate(NUMBERED_WALL_DENSITIES):
    parser.add_argument(f"--numbered_wall_{n}_density", type=float, default=density)
args = parser.parse_args()

numbered_wall_densities = tuple(getattr(args, f"numbered_wall_{n}_density") for n in range(5))
if not math.isclose(sum(numbered_wall_densities), 1.0, abs_tol=1e-6):
    parser.error(f"--numbered_wall_(0-4)_density must sum to 1, got {sum(numbered_wall_densities):g}")


if __name__ == "__main__":
    generate_input_file(args.rows, args.cols, args.seed, args.output_path, args.plot_solution_grid, args.wall_density, args.numbered_wall_chance, numbered_wall_densities, args.plot_input_grid)