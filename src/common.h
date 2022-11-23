#ifndef COMMON_H
#define COMMON_H

#include <graphx.h>
#include <stdint.h>
#include <stdbool.h>

#include "sprites/gfx.h"

#define SNAKE_PX_STRIDE 10
#define SNAKE_GRID_WIDTH (GFX_LCD_WIDTH / SNAKE_PX_STRIDE)
#define SNAKE_GRID_HEIGHT (GFX_LCD_HEIGHT / SNAKE_PX_STRIDE)
#define SNAKE_VERTDATA_LEN (SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT)
#define _2048_GRID_WH 4
#define SUDOKU_GRID_WH 9

/* This constant is defined from experience. */
#define CHAR_HEIGHT 8

bool any(const void *, size_t nmemb, size_t size);
bool all(const void *, size_t nmemb, size_t size);
int sign(int a);
void enforce_lt(int *a, int *b);
uint8_t get_single_key_pressed(void);
void g_list(const char *s[], int x, int y);
void g_sel(int x, int y);

struct Vertex {
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
		struct Vertex vertdata[SNAKE_VERTDATA_LEN];
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
};

#endif // COMMON_H