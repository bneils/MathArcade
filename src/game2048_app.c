#include <graphx.h>
#include <tice.h>
#include <stdbool.h>
#include <sys/util.h>
#include <stdio.h>
#include <string.h>

#include "game2048_app.h"
#include "common.h"

#define tiles share._2048_bss.tiles
#define LEFTPAD (LCD_WIDTH - LCD_HEIGHT)
#define CELL_WIDTH (LCD_HEIGHT / _2048_GRID_WH)

extern union Shared share;

static void draw(void);
static bool spawn_new(void);
static void rot90(void);
static int shl_combine(void);
static bool keepleft(uint24_t row[]);

void game2048_mainloop(void) {
	memset(tiles, 0, sizeof(tiles)); 
	spawn_new();
	spawn_new();

	uint24_t score = 0;

	for (;;) {
		uint24_t key;

		draw();
		gfx_SwapDraw();

		while (!(key = os_GetCSC()))
			;

		// The shift algorithm is only implemented once but needs to
		// work in all directions, so to get around it I perform an
		// identity rotation (360 deg) and use the shift routine inbetween.
		int increment_score;
		if (key == sk_Left) {
			increment_score = shl_combine();
		} else if (key == sk_Right) {
			rot90();
			rot90();
			increment_score = shl_combine();
			rot90();
			rot90();
		} else if (key == sk_Up) {
			rot90();
			rot90();
			rot90();
			increment_score = shl_combine();
			rot90();
		} else if (key == sk_Down) {
			rot90();
			increment_score = shl_combine();
			rot90();
			rot90();
			rot90();
		} else if (key == sk_Clear) {
			return;
		} else {
			continue;
		}

		if (increment_score < 0) {
			continue;
		}
		score += increment_score;
		if (!spawn_new()) {
			break;
		}
	}

	draw();
	gfx_SetTextFGColor(BLACK);
	gfx_PrintStringXY("Your ", 10, 92);
	gfx_PrintStringXY("score is", 10, 92 + 8);
	char s[8 + 1];
	sprintf(s,"%d",score); 
	gfx_PrintStringXY(s, 10, 92 + 16);
	gfx_SwapDraw();

	usleep(500000);
	while (!os_GetCSC())
		;
}

static void draw(void) {
	gfx_FillScreen(WHITE);
	
	int py = 5 + (CELL_WIDTH - 8) / 2;
	for (int y = 0; y < _2048_GRID_WH; ++y) {
		for (int x = 0; x < _2048_GRID_WH; ++x) {
			uint24_t n = tiles[y][x];
			if (n == 0)
				continue;
			// 2^24 - 1 = 16,777,215 (8 characters long)
			char str_n[8 + 1];
			sprintf(str_n, "%d", n);

			// __builtin_clz counts the number of leading zeros on an
			// unsigned integer. I subtract 1 during the calculation because
			// I want the shift index required to get the highest order bit.
			int i = 8 * sizeof(unsigned int) - 1 - __builtin_clz(n);
			// The smallest number, 2, would have i=1 (because 1<<1 == 2)
			// To start the index at 0, I'm subtracting 1.
			--i;

			// 2 or 4 have light backgrounds, so the text needs to be black
			// in order to contrast with it.
			gfx_SetTextFGColor((i <= 1) ? BLACK : WHITE);
			gfx_SetColor(G2048_2 + i);

			gfx_FillRectangle(x * CELL_WIDTH + LEFTPAD, y * CELL_WIDTH, CELL_WIDTH, CELL_WIDTH);

			int px = x * CELL_WIDTH + LEFTPAD + 5 + (CELL_WIDTH - gfx_GetStringWidth(str_n)) / 2;
			gfx_PrintStringXY(str_n, px, py);
		}
		py += CELL_WIDTH;
	}

	gfx_SetColor(BLACK);
	for (int i = 0; i <= 4; ++i) {
		int v = i * CELL_WIDTH;
		gfx_HorizLine(LEFTPAD, v, LCD_HEIGHT);
		gfx_VertLine(LEFTPAD + v, 0, LCD_HEIGHT);
	}
}

/* returns true on success */
static bool spawn_new(void) {
	// To avoid an infinite loop, there needs to be a check that it's
	// possible to place something on the board (that theres a 0).
	if (all(tiles, _2048_GRID_WH * _2048_GRID_WH, sizeof(tiles[0][0]))) {
		return false;
	}

	uint24_t *tiles_1d = (uint24_t *) tiles;
	int24_t pos;
	do {
		pos = randInt(0, _2048_GRID_WH * _2048_GRID_WH - 1);
	} while (tiles_1d[pos] != 0);
	
	tiles_1d[pos] = 2;
	return true;
}

/* Shift everything to the left so that order is preserved but that
 * non-zero numbers are on the left and zeros are on the right.
 *
 * Returns true if the row changed.
 */
static bool keepleft(uint24_t row[]) {
	bool changed = false;

	for (int x = 0; x < _2048_GRID_WH - 1; ++x) {
		if (row[x] == 0) {
			for (int i = x + 1; i < _2048_GRID_WH; ++i) {
				if (row[i] != 0) {
					row[x] = row[i];
					row[i] = 0;
					changed = true;
					break;
				}
			}
		}
	}
	return changed;
}

/* Shifts the tile board left and combines tiles. Score increment
 * is the sum of all tiles involved in combinations.
 *
 * -1 is returned as an error if the shift had no effect.
 */
static int shl_combine(void) {
	int score_increment = 0;
	bool changed = false;

	for (int y = 0; y < 4; ++y) {
		uint24_t *row = tiles[y];

		if (keepleft(row)) {
			changed = true;
		}

		for (int x = 0; x < 3 && row[x] != 0; ++x) {
			if (row[x] == row[x + 1]) {
				score_increment += (row[x] *= 2);
				row[x + 1] = 0;
				changed = true;
			}
		}

		if (keepleft(row)) {
			changed = true;
		}
	}

	return (changed) ? score_increment : -1;
}

static void rot90(void) {
	uint24_t temp[_2048_GRID_WH][_2048_GRID_WH];

	/* Here is an "unrolled" variant
	of this algorithm to rotate a 4x4 matrix.
	temp[0][3]=board[0][0];
	temp[1][3]=board[0][1];
	temp[2][3]=board[0][2];
	temp[3][3]=board[0][3];

	temp[0][2]=board[1][0];
	temp[1][2]=board[1][1];
	temp[2][2]=board[1][2];
	temp[3][2]=board[1][3];

	temp[0][1]=board[2][0];
	temp[1][1]=board[2][1];
	temp[2][1]=board[2][2];
	temp[3][1]=board[2][3];

	temp[0][0]=board[3][0];
	temp[1][0]=board[3][1];
	temp[2][0]=board[3][2];
	temp[3][0]=board[3][3];
	*/

	for (int a = 0; a < _2048_GRID_WH; ++a) {
		for (int b = 0; b < _2048_GRID_WH; ++b) {
			temp[b][(_2048_GRID_WH - 1) - a] = tiles[a][b];
		}
	}

	memcpy(tiles, temp, sizeof(temp));
}
