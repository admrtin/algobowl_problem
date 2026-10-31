import argparse

from src.input_gen.generate_input_file import generate_input_file

parser = argparse.ArgumentParser()
parser.add_argument("--rows", type=int, default=316)
parser.add_argument("--cols", type=int, default=316)
parser.add_argument("--seed", type=int, default=1234)
parser.add_argument("--output_path", type=str, default = "out/")
parser.add_argument("--plot_solution_grid", action="store_true")
parser.add_argument("--wall_density", type=float, default=0.3)
parser.add_argument("--numbered_wall_chance", type=float, default=0.7)
args = parser.parse_args()


if __name__ == "__main__":
    generate_input_file(args.rows, args.cols, args.seed, args.output_path, args.plot_solution_grid, args.wall_density, args.numbered_wall_chance)