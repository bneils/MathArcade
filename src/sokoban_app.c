#include <graphx.h>
#include <sys/util.h>
#include <tice.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

#define level         share.sokoban_bss.level
#define playerx       share.sokoban_bss.playerx
#define playery       share.sokoban_bss.playery
#define width         share.sokoban_bss.width
#define height        share.sokoban_bss.height
#define encoded_size  share.sokoban_bss.size
#define player_sprite share.sokoban_bss.player_sprite

/* Sokoban levels taken from
 * http://www.sneezingtiger.com/sokoban/levels/microbanText.html
 *
 * The level format is as follows:
 * width (1B)
 * height (1B)
 * size (2B)
 * player x (1B)
 * player y (1B)
 * level data (size B)...
 *
 * Each byte in the level data contains two LevelTokens
 * If has_rle_specifier is 1, the next token is a 4-bit unsigned integer - 2.
 * The current token will be repeated that number of times.
 *
 * Each token has the bitpattern:
 *  BOX_TILE, GOAL_TILE, WALL/FLOOR TILE, IS_RLE
 */
#define BOX_BIT  0b1000
#define GOAL_BIT 0b0100
#define WALL_BIT 0b0010
#define RLE_BIT  0b0001

#define HEADER_SIZE (1 + 1 + 2 + 1 + 1)
#define CELL_PX_WIDTH 16

#define LEVELIDX(x, y) ((x) + (y) * width)

static bool load_level(int levelid);
static void draw_level(void);
static bool play(void);
static bool check_level_complete(void);

static void draw_level(void)
{
	int offx = (LCD_WIDTH / CELL_PX_WIDTH - width)
		* CELL_PX_WIDTH / 2;
	int offy = (LCD_HEIGHT / CELL_PX_WIDTH - height)
		* CELL_PX_WIDTH / 2;

	gfx_FillScreen(WHITE);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int px = offx + x * CELL_PX_WIDTH;
			int py = offy + y * CELL_PX_WIDTH;
			uint8_t tile = level[LEVELIDX(x, y)];

			if (tile & WALL_BIT) {
				gfx_Sprite(sprite_sokoban_wall, px, py);
				continue;
			}

			if (tile & GOAL_BIT)
				gfx_Sprite(sprite_sokoban_goal, px, py);
			if (tile & BOX_BIT)
				gfx_Sprite(sprite_sokoban_box, px, py);
		}
	}
	gfx_TransparentSprite(player_sprite,
		playerx * CELL_PX_WIDTH + offx,
		playery * CELL_PX_WIDTH + offy);
}

void sokoban_mainloop(void)
{
	for (int i = 0; i < SOKOBAN_NUM_LEVELS; ++i) {
		gfx_FillScreen(WHITE);

		char s[16];
		sprintf(s, "LEVEL %d", i);
		gfx_PrintStringXY(s,
			(GFX_LCD_WIDTH - gfx_GetStringWidth(s)) / 2,
			GFX_LCD_HEIGHT / 2);

		gfx_SwapDraw();
		usleep(1000000);
		load_level(i);
		if (!play())
			return;
	}
}

static bool play(void) {
	player_sprite = sprite_sokoban_left;
	for (;;) {
		int key;

		draw_level();
		gfx_SwapDraw();

		while (!(key = os_GetCSC()))
			;
		/* Validate if the player can move there.
		 * a player can move somewhere if:
		 * there is at least 1 floor/dest in the direction they are
		 * going before a ray hits a wall
		 */
		int dx, dy;

		switch (key) {
		case sk_Left:
			player_sprite = sprite_sokoban_left;
			dx = -1;
			dy = 0;
			break;
		case sk_Right:
			player_sprite = sprite_sokoban_right;
			dx = 1;
			dy = 0;
			break;
		case sk_Up:
			player_sprite = sprite_sokoban_up;
			dx = 0;
			dy = -1;
			break;
		case sk_Down:
			player_sprite = sprite_sokoban_down;
			dx = 0;
			dy = 1;
			break;
		case sk_Clear:
			return false;
		default:
			// bad key, don't do anything.
			continue;
		}

		uint8_t *next1_tile, *next2_tile;

		next1_tile = &level[LEVELIDX(playerx + dx, playery + dy)];
		next2_tile = &level[
			LEVELIDX(playerx + dx * 2, playery + dy * 2)
		];

		if (*next1_tile & WALL_BIT)
			continue;
		if (*next1_tile & BOX_BIT) {
			if (*next2_tile & (WALL_BIT | BOX_BIT))
				continue;
			*next2_tile |= BOX_BIT;
			*next1_tile &= ~BOX_BIT;
		}

		playerx += dx;
		playery += dy;

		if (check_level_complete())
			return true;
	}
}

/* Decode a level that is encoded with run-length encoding and initialize its
 * metadata. The size of this procedure should be smaller than the savings
 * gained from the compression.
 *
 * levelid's range is [0, SOKOBAN_NUM_LEVELS - 1]
 *
 * Returns true if the level was loaded successfully.
 */
static bool load_level(int levelid)
{
	uint8_t *src, *dest, tile;
	bool rle;

	if (levelid < 0 || levelid >= SOKOBAN_NUM_LEVELS)
		return false;

	src = &sokoban_levels[sokoban_level_table[levelid]];
	width = src[0];
	height = src[1];
	encoded_size = (src[2] << 8) | src[3];
	playerx = src[4];
	playery = src[5];

	dest = level;
	rle = false;

	for (int i = 0; i < encoded_size; ++i) {
		uint8_t pair = src[HEADER_SIZE + i];
		for (int j = 0; j < 2; ++j) {
			uint8_t nibble = (pair >> 4) & 0xf;

			if (rle)
				for (int reps = nibble + 2; reps > 0; --reps)
					*dest++ = tile;
			else
				*dest++ = tile = nibble & 0xe;
			rle = nibble & RLE_BIT;
			pair <<= 4;
		}
	}
	return true;
}

/* Ensure that every box tile is on a goal tile */
static bool check_level_complete(void) {
	for (int i = 0; i < width * height; ++i) {
		uint8_t attr = level[i] & (GOAL_BIT | BOX_BIT);
		if (attr == GOAL_BIT || attr == BOX_BIT)
			return false;
	}
	return true;
}
