/* Commons macros and functions that are used between
 * programs
 */

#ifndef COMMON_H
#define COMMON_H

#include <graphx.h>
#include <stdint.h>
#include <stdbool.h>

#include "sprites/gfx.h"

// I'm including this because the union declaration needs to know how much
// space to allocate for every Sokoban level.
#include "sokoban_data.h"

/* Be wary that SNAKE_PX_STRIDE needs to be at least 2
 * due to an overflow in struct Pos
 */
#define SNAKE_PX_STRIDE 10
#define SNAKE_GRID_WIDTH (GFX_LCD_WIDTH / SNAKE_PX_STRIDE)
#define SNAKE_GRID_HEIGHT (GFX_LCD_HEIGHT / SNAKE_PX_STRIDE)
#define SNAKE_VERTDATA_LEN (SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT)
#define _2048_GRID_WH 4
#define SUDOKU_GRID_WH 9

// 2^24 - 1 = 16,777,215 which occupies 8 characters (plus \0).
#define UINT24_STRING_SIZE (8 + 1)

/* This constant is defined from experience. */
#define CHAR_HEIGHT 8

bool any(const void *, size_t nmemb, size_t size);
bool all(const void *, size_t nmemb, size_t size);
int sign(int a);
uint8_t get_single_key_pressed(void);
void g_list(const char *s[], int x, int y);
void g_sel(int x, int y);

void snake_mainloop(void);
void sokoban_mainloop(void);
void sudoku_mainloop(void);
void game2048_mainloop(void);

struct Pos {
	uint8_t x, y;
};

enum Colors {
	TRANSPARENT = 0,
	BLACK,
	WHITE,
	RED,
	GREEN,
	BLUE,
	GRAY1,
	GRAY2,

	// It's an implementation requirement that
	// these color definitions are in increasing order.
	G2048_2,
	G2048_4,
	G2048_8,
	G2048_16,
	G2048_32,
	G2048_64,
	G2048_128,
	G2048_256,
	G2048_512,
	G2048_1024,
	G2048_2048,
	G2048_4096,
};

/* To avoid the overhead for a call like malloc (speed and space), data
 * with non-overlapping lifetimes are merged using a union.
 * A lot of space is saved by sharing memory like this besides the obvious,
 * algorithms can be more wasteful and use less instructions to perform a
 * computation.
 */
union Shared {
	struct {
		struct Pos vertdata[SNAKE_VERTDATA_LEN];
	} snake_bss;

	struct {
		uint24_t tiles[_2048_GRID_WH][_2048_GRID_WH];
	} _2048_bss;

	struct {
		uint8_t tiles[SUDOKU_GRID_WH][SUDOKU_GRID_WH];
		uint8_t *tiles_initial;
		uint24_t curx, cury;
		bool candidate_set[SUDOKU_GRID_WH][SUDOKU_GRID_WH][10];
	} sudoku_bss;

	struct {
		// The order of these fields must be maintained (it is easier
		// to copy them)
		// --->
		uint8_t width, height;
		uint8_t playerx, playery;
		// <---

		gfx_sprite_t *player_sprite;

		uint8_t level[SOKOBAN_MAX_LEVEL_SIZE];
		uint8_t levels[SOKOBAN_LEVELS_SIZE];
	} sokoban_bss;
};

extern union Shared share;

#endif // COMMON_H
