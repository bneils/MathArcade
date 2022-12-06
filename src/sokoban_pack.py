#!/usr/bin/env python3

"""
Extract Sokoban level information from a text file and encode it into
sokoban_data.c using simple rl encoding
"""
# Credit to http://sneezingtiger.com/sokoban/levels/microcosmosText.html

import os
import time
import sys
import subprocess

base_path = os.path.dirname(__file__)

BOX = "$"
BOX_ON_GOAL = "*"
WALL = "#"
PLAYER = "@"
PLAYER_ON_GOAL = "+"
GOAL = "."
FLOOR = " "

RLE_FLAG = 1

LCD_WIDTH = 320
LCD_HEIGHT = 240
SPRITE_WIDTH = 16

def preprocess():
    with open(os.path.join(base_path, "sokoban_levels.txt")) as f:
        lines = f.readlines()

    levels = []

    for line in lines:
        line = line.rstrip()
        if line.lower().startswith("level"):
            if not levels or levels[-1]:
                levels.append([])
        elif line:
            levels[-1].append(line)
    return levels

# box(1),goal(1),wall(1)/floor(0),rle spec next
# The level format is as follows:
# | width (1B) | height (1B) | size (2B) |
# | player x (1B) | player y (1B) | ... |
# each byte in the sequence is considered nibble-by-nibble.
# the first 3 bits are: is box, is goal, is wall else floor
# the last bit specifies if the nibble is followed by a rle count to determine
# if it should repeat this tile X+2 times where X is the next nibble.
# X+2 is used because when X=0, 2 tiles will be repeated which avoids the non-
# sensical possibilities where the tile is repeated 0 or 1 times (redundant)
# if this bit is 0 it won't be repeated.

levels = preprocess()

mapping = {
    FLOOR:          0b0000,
    BOX:            0b1000,
    BOX_ON_GOAL:    0b1100,
    WALL:           0b0010,
    GOAL:           0b0100,
    PLAYER_ON_GOAL: 0b0100,
    PLAYER:         0b0000,
}

bytes_ = []
cumulative_sizes = []
max_level_size = 0

for level in levels:
    player_x = None
    player_y = None
    max_w = max(map(len, level))
    cells = []
    for y, line in enumerate(level):
        for x, c in enumerate(line.ljust(max_w)):
            if c == PLAYER or c == PLAYER_ON_GOAL:
                player_x = x
                player_y = y
            cells.append(mapping[c])

    assert player_x is not None and player_y is not None
    assert len(bytes_) <= (1 << 16) - 1, "overflow"

    cumulative_sizes.append(len(bytes_))
    level = [max_w, len(level), player_x, player_y] + cells
    if len(cells) > max_level_size:
        max_level_size = len(cells)
    bytes_ += level

with open(os.path.join(base_path, "sokoban_table.c"), "w") as f:
    f.write(
        "#include <stdint.h>\n" + "uint16_t sokoban_level_table[] = {" +
        ",".join(map(str, cumulative_sizes)) + "};"
    )
with open(os.path.join(base_path, f"sokoban_levels.dat"), "wb") as f:
    f.write(bytes(bytes_))
subprocess.run(["convbin",
    "--iformat", "bin",
    "--icompress", "zx7",
    "-i", os.path.join(base_path, "sokoban_levels.dat"),
    "--oformat", "c",
    "-o", os.path.join(base_path, "sokoban_levels.c"),
    "-n", "sokoban_levels"])

os.remove(os.path.join(base_path, "sokoban_levels.dat"))

file_contents = f"""#ifndef SOKOBAN_DATA_H
#define SOKOBAN_DATA_H
#include <stdint.h>
#define SOKOBAN_NUM_LEVELS {len(levels)}
// SOKOBAN_MAX_LEVEL_SIZE does not include header size
#define SOKOBAN_MAX_LEVEL_SIZE {max_level_size}
#define SOKOBAN_LEVELS_SIZE {len(bytes_)}
extern uint16_t sokoban_level_table[];
extern uint8_t sokoban_levels[];
#endif // SOKOBAN_DATA_H"""

with open(os.path.join(base_path, "sokoban_data.h"), "w") as f:
    f.write(file_contents)


