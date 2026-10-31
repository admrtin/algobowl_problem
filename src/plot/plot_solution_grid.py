import matplotlib.pyplot as plt
import numpy as np
from matplotlib.collections import LineCollection
from matplotlib.colors import to_rgb

ENCODING: dict = {
    "blank": ".",
    "gray": "X",
    "gray_0": "0",
    "gray_1": "1",
    "gray_2": "2",
    "gray_3": "3",
    "gray_4": "4",
    "light": "L",
}

COLORS: dict = {
    "blank": "white",
    "lit_blank": "#ffffaa",
    "gray": "gray",
    "gray_0": "gray",
    "gray_1": "gray",
    "gray_2": "gray",
    "gray_3": "gray",
    "gray_4": "gray",
    "light": "yellow",
}

# Cell width in screen pixels below which an overlay is hidden.
MIN_GLYPH_CELL_PIXELS: float = 8
MIN_GRID_CELL_PIXELS: float = 4

# Glyph size relative to its cell.
GLYPH_CELL_FRACTION: float = 0.7


def plot_solution_grid(
    rows: int,
    cols: int,
    walls: set[tuple[int, int]],
    lights: set[tuple[int, int]],
    constraints: dict[str, set[tuple[int, int]]]
    ,
):
    # constraints[n] holds the (row, col) cells of gray_n; walls holds unnumbered gray cells.
    image = np.empty((rows, cols, 3))
    image[:, :] = to_rgb(COLORS["blank"])
    for r, c in walls or ():
        image[r, c] = to_rgb(COLORS["gray"])
    for key, cells in constraints.items():
        for r, c in cells:
            image[r, c] = to_rgb(COLORS[key])
    blockers = set(walls or ())
    for cells in constraints.values():
        blockers.update(cells)
    for r0, c0 in lights:
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            r, c = r0 + dr, c0 + dc
            while 0 <= r < rows and 0 <= c < cols and (r, c) not in blockers:
                image[r, c] = to_rgb(COLORS["lit_blank"])
                r, c = r + dr, c + dc
    for r, c in lights:
        image[r, c] = to_rgb(COLORS["light"])

    fig, ax = plt.subplots()
    ax.imshow(image, interpolation="nearest")

    # One marker layer per glyph draws far faster than one ax.text per cell.
    glyph_layers = [_add_glyph_layer(ax, lights, ENCODING["light"], "black")]
    for key, cells in constraints.items():
        glyph_layers.append(_add_glyph_layer(ax, cells, ENCODING[key], "white"))

    grid_lines = LineCollection(
        [[(x, -0.5), (x, rows - 0.5)] for x in np.arange(-0.5, cols, 1)]
        + [[(-0.5, y), (cols - 0.5, y)] for y in np.arange(-0.5, rows, 1)],
        colors="black",
        linewidths=0.5,
    )
    ax.add_collection(grid_lines, autolim=False)
    ax.tick_params(which="both", length=0, labelbottom=False, labelleft=False)

    def fit_overlays_to_zoom(_=None):
        # Glyphs and grid lines only show once cells are large enough to hold them.
        ax.apply_aspect()
        origin_x, _origin_y = ax.transData.transform((0, 0))
        next_x, _next_y = ax.transData.transform((1, 0))
        cell_pixels = abs(next_x - origin_x)
        grid_lines.set_visible(cell_pixels >= MIN_GRID_CELL_PIXELS)
        for layer in glyph_layers:
            layer.set_visible(cell_pixels >= MIN_GLYPH_CELL_PIXELS)
            layer.set_markersize(GLYPH_CELL_FRACTION * cell_pixels * 72 / fig.dpi)

    fit_overlays_to_zoom()
    ax.callbacks.connect("xlim_changed", fit_overlays_to_zoom)
    ax.callbacks.connect("ylim_changed", fit_overlays_to_zoom)
    fig.canvas.mpl_connect("resize_event", fit_overlays_to_zoom)

    plt.show()
    return fig, ax


def _add_glyph_layer(ax, cells: set[tuple[int, int]], glyph: str, color: str):
    rows_and_cols = np.array(sorted(cells)).reshape(-1, 2)
    line, = ax.plot(
        rows_and_cols[:, 1],
        rows_and_cols[:, 0],
        linestyle="none",
        marker=rf"$\mathrm{{{glyph}}}$",
        color=color,
        markeredgewidth=0,
    )
    return line
