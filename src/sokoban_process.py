#!/usr/bin/env python3

"""
Extract Sokoban level information from a text file and encode it into
sokoban_data.c using simple rl encoding
"""
# Credit to http://sneezingtiger.com/sokoban/levels/microcosmosText.html

import os

base_path = os.path.dirname(__file__)

BOX = "$"
BOX_ON_GOAL = "*"
WALL = "#"
PLAYER = "@"
PLAYER_ON_GOAL = "+"
GOAL = "."
FLOOR = " "

LCD_WIDTH = 320
LCD_HEIGHT = 240
SPRITE_WIDTH = 16

MAX_WIDTH = LCD_WIDTH / SPRITE_WIDTH    # 320/16 = 20
MAX_HEIGHT = LCD_HEIGHT / SPRITE_WIDTH  # 240/16 = 15

def ntob_hof(nibbles):
    """Converts an array of nibbles to bytes taking the high order bits first"""
    shift = 4
    bytearr = []
    for nibble in nibbles:
        nibble <<= shift
        if shift:
            bytearr.append(nibble)
        else:
            bytearr[-1] |= nibble
        shift ^= 4
    return bytearr

def rle_encode_level(nums):
    """Encodes a sequence of tiles using run-length encoding"""
    nibbles = []
    i = 0
    while i < len(nums):
        j = i
        while j < len(nums) - 1 and nums[i] == nums[j]:
            j += 1
        consec_size = j - i - 1
        nibbles.append(nums[i])
        if consec_size >= 3:
            assert consec_size - 3 <= 15, "overflow"
            nibbles[-1] |= 1
            nibbles.append(consec_size - 3)
            i = j
        else:
            i += 1
    return ntob_hof(nibbles)

with open(os.path.join(base_path, "sokoban_levels.txt")) as f:
    lines = f.read().splitlines()

levels = []
linestack = []

for line in lines:
    line = line.rstrip()
    if line.lower().startswith("level"):
        if linestack:
            levels.append(linestack)
            linestack = []
    elif line:
        linestack.append(line)

if linestack:
    levels.append(linestack)
    linestack = []

# The greatest level dimensions are 20x15 (320/16x240/16)
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

mapping = {
    FLOOR:       0b0000,
    BOX:         0b1000,
    BOX_ON_GOAL: 0b1100,
    WALL:        0b0010,
    GOAL:        0b0100,
    PLAYER_ON_GOAL: 0b0100,
}

total_size = 0
byte_arr = []
cumulative_sizes = []
levels_linear = []

max_level_size = 0

for level in levels:
    player_x = None
    player_y = None
    max_w = max(map(len, level))
    cells = []
    assert max_w <= MAX_WIDTH and len(level) <= MAX_HEIGHT
    for y, line in enumerate(level):
        for x, c in enumerate(line.ljust(max_w)):
            if c == PLAYER or c == PLAYER_ON_GOAL:
                player_x = x
                player_y = y
                cells.append(mapping[FLOOR])
            else:
                cells.append(mapping[c])
    levels_linear += cells
    cells = rle_encode_level(cells)
    total_size += len(cells) + 6
    assert player_x is not None and player_y is not None
    assert len(byte_arr) <= (1 << 16) - 1, "overflow"
    cumulative_sizes.append(len(byte_arr))
    byte_arr += [max_w, len(level), (len(cells) >> 8) & 0xff, len(cells) & 0xff,
                player_x, player_y]
    byte_arr += cells

    level_size = max_w * len(level)
    if level_size > max_level_size:
        max_level_size = level_size

print("Update common.h to make the array's size to be", max_level_size)

file_contents = "#include <stdint.h>\n"
file_contents += f"uint8_t sokoban_levels[{len(byte_arr)}] = "
file_contents += "{" + ", ".join(map(str, byte_arr)) + "};\n"
file_contents += f"uint16_t sokoban_level_table[{len(levels)}] = "
file_contents += "{" + ", ".join(map(str, cumulative_sizes)) + "};\n"

with open(os.path.join(base_path, "sokoban_data.c"), "w") as f:
    f.write(file_contents)

file_contents = f"""#ifndef SOKOBAN_DATA_H
#define SOKOBAN_DATA_H
#include <stdint.h>
#define SOKOBAN_NUM_LEVELS {len(levels)}
#define SOKOBAN_MAX_LEVEL_SIZE {len(levels) * max_w}
extern uint16_t sokoban_level_table[];
extern unsigned char sokoban_levels[];
#endif // SOKOBAN_DATA_H
"""

with open(os.path.join(base_path, "sokoban_data.h"), "w") as f:
    f.write(file_contents)
