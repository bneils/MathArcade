#include <graphx.h>
#include <tice.h>
#include <sys/util.h>

#include <stdbool.h>
#include <string.h>

#include "common.h"

#define curx share.sudoku_bss.curx
#define cury share.sudoku_bss.cury
#define tiles share.sudoku_bss.tiles
#define tiles_initial share.sudoku_bss.tiles_initial
#define candidate_set share.sudoku_bss.candidate_set

extern union Shared share;

static void draw(void);
static bool validate_num_insert_at_cur(int n);
static void load_random_board(void);
static void update_candidate_set(void);
static void change_candidate_set_at_cur(int n, bool presence);

static inline void try_dec_coord(uint24_t *);
static inline void try_inc_coord(uint24_t *);

/* The amount of space on the left/right sides of the screen when
 * trying to fit the largest possible square inside the screen.
 */
#define SQUARE_LRMARGIN ((LCD_WIDTH - LCD_HEIGHT) / 2)
#define CELL_WIDTH (LCD_HEIGHT / 9)

#define NUM_SUDOKU_BOARDS 32

#define CELL_NUM_PADDING ((CELL_WIDTH - 8) / 2)

// The amount of pixels on the left/right
#define BOX_THICKNESS 2
#define CHAR_HEIGHT 8

// How many pixels need to be subtracted from the lines due to integer divison.
#define MAGIC_INTEGER_ERR 7

static void draw(void)
{
	gfx_FillScreen(WHITE);

	gfx_SetColor(GRAY1);
	gfx_FillRectangle(SQUARE_LRMARGIN*2 + curx * CELL_WIDTH + 1,
		cury * CELL_WIDTH + 1, CELL_WIDTH - 1, CELL_WIDTH - 1);
	gfx_SetColor(BLACK);
	gfx_SetTextFGColor(BLACK);

	// Handy tip bar on the left of numbers they can place at the cursor.
	for (int i = 1; i <= 9; ++i) {
		if (tiles[cury][curx] == 0 && candidate_set[cury][curx][i]) {
			gfx_SetTextXY(5, 5 + i * CHAR_HEIGHT);
			gfx_PrintChar('0' + i);
		}
	}

	for (int i = 0; i <= 9; ++i) {
		/* The offset uses the smaller dimension, which in this
		 * case is the height of the screen.
		 */
		int offset = LCD_HEIGHT / 9 * i;
		gfx_HorizLine(SQUARE_LRMARGIN*2, offset, LCD_WIDTH
			- SQUARE_LRMARGIN * 2 - MAGIC_INTEGER_ERR);
		gfx_VertLine(offset + SQUARE_LRMARGIN*2, 0, LCD_HEIGHT
			- MAGIC_INTEGER_ERR);
	}

	// Draw the thick lines to recognize the boxes
	for (int i = 0; i <= 9; i += 3) {
		int offset = LCD_HEIGHT / 9 * i;
		// Horiz
		gfx_FillRectangle(SQUARE_LRMARGIN*2 - BOX_THICKNESS, offset
			- BOX_THICKNESS,
			LCD_HEIGHT + BOX_THICKNESS * 2 - MAGIC_INTEGER_ERR,
			BOX_THICKNESS * 2);
		// Vert
		gfx_FillRectangle(offset + SQUARE_LRMARGIN*2 - BOX_THICKNESS, 0,
			BOX_THICKNESS * 2, LCD_HEIGHT + BOX_THICKNESS * 2
			- MAGIC_INTEGER_ERR - 1);
	}

	for (int y = 0; y < 9; ++y) {
		for (int x = 0; x < 9; ++x) {
			if (tiles[y][x] == 0)
				continue;
			char digit = '0' + tiles[y][x];
			gfx_SetTextFGColor(
				(tiles_initial[y * SUDOKU_GRID_WH + x] == 0) ?
				RED : BLACK);
			gfx_SetTextXY(x * CELL_WIDTH
				+ SQUARE_LRMARGIN * 2 + CELL_NUM_PADDING,
				y * CELL_WIDTH + CELL_NUM_PADDING);
			gfx_PrintChar(digit);
		}
	}
}

static void load_random_board(void)
{
	extern uint8_t
	sudoku_boards[NUM_SUDOKU_BOARDS][SUDOKU_GRID_WH][SUDOKU_GRID_WH];
	int i = randInt(0, NUM_SUDOKU_BOARDS - 1);
	tiles_initial = (uint8_t *) sudoku_boards[i];
	memcpy(tiles, tiles_initial, sizeof tiles);
}

void sudoku_mainloop(void)
{
	load_random_board();
	update_candidate_set();

	for (;;) {
		draw();
		gfx_SwapDraw();

		uint8_t key;
		while (!(key = os_GetCSC()))
			;

		int num_to_insert = -1;

		switch (key) {
			case sk_Up:
				try_dec_coord(&cury);
				break;
			case sk_Down:
				try_inc_coord(&cury);
				break;
			case sk_Left:
				try_dec_coord(&curx);
				break;
			case sk_Right:
				try_inc_coord(&curx);
				break;
			case sk_Clear:
				return;
			case sk_Del:
				num_to_insert = 0;
				break;
			case sk_1:
				num_to_insert = 1;
				break;
			case sk_2:
				num_to_insert = 2;
				break;
			case sk_3:
				num_to_insert = 3;
				break;
			case sk_4:
				num_to_insert = 4;
				break;
			case sk_5:
				num_to_insert = 5;
				break;
			case sk_6:
				num_to_insert = 6;
				break;
			case sk_7:
				num_to_insert = 7;
				break;
			case sk_8:
				num_to_insert = 8;
				break;
			case sk_9:
				num_to_insert = 9;
				break;
		}

		if (validate_num_insert_at_cur(num_to_insert)) {
			change_candidate_set_at_cur(tiles[cury][curx], true);
			tiles[cury][curx] = num_to_insert;
			change_candidate_set_at_cur(num_to_insert, false);

			if (all(tiles, SUDOKU_GRID_WH * SUDOKU_GRID_WH, 1)) {
				const char *wonlines[] = {
					"You",
					"solved",
					"the",
					"board!",
					NULL
				};

				draw();
				g_list(wonlines, 5, 85);
				gfx_SwapDraw();

				while (!os_GetCSC())
					;
				return;
			}
		}
	}
}

static bool validate_num_insert_at_cur(int n)
{
	if (n < 0 || n > 9)
		return false;

	if (tiles_initial[cury * SUDOKU_GRID_WH + curx] != 0)
		return false;

	if (n == 0)
		return true;


	uint24_t box_y = cury / 3 * 3;
	uint24_t box_x = curx / 3 * 3;

	for (int i = 0; i < 9; ++i) {
		if (tiles[i][curx] == n ||
			tiles[cury][i] == n ||
			tiles[box_y + i / 3][box_x + i % 3] == n)
			return false;
	}
	return true;
}

static inline void try_dec_coord(uint24_t *c)
{
	if (*c > 0)
		--*c;
}

static inline void try_inc_coord(uint24_t *c)
{
	if (*c < SUDOKU_GRID_WH - 1)
		++*c;
}

static void update_candidate_set(void)
{
	uint24_t tempy, tempx;
	tempy = cury;
	tempx = curx;
	for (cury = 0; cury < SUDOKU_GRID_WH; ++cury) {
		for (curx = 0; curx < SUDOKU_GRID_WH; ++curx) {
			for (int n = 0; n <= 9; ++n)
				candidate_set[cury][curx][n] =
					validate_num_insert_at_cur(n);
		}
	}
	cury = tempy;
	curx = tempx;
}

static void change_candidate_set_at_cur(int n, bool presence)
{
	if (n == 0)
		return;
	uint24_t box_y = cury / 3 * 3;
	uint24_t box_x = curx / 3 * 3;
	for (int i = 0; i < SUDOKU_GRID_WH; ++i) {
		candidate_set[i][curx][n] = presence;
		candidate_set[cury][i][n] = presence;
		candidate_set[box_y + i / 3][box_x + i % 3][n] = presence;
	}
}