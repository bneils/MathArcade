#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>

#include "common.h"

#define MENU_LEFT_PADDING 20
#define MENU_TOP_PADDING 20
#define MENU_CURSOR_WIDTH 16

static inline void palette_init(void);
static void selection_screen(void);

/* must be null-terminated */
const char *list_items[] = {
	"sudoku",
	"sokoban",
	"2048",
	"snake",
	NULL
};

#define N_GAMES (sizeof(list_items) / sizeof(list_items[0]) - 1)

int main(void) {
	srandom(rtc_Time());
	gfx_Begin();
	gfx_SetDrawBuffer();
	palette_init();
	selection_screen();
	gfx_End();
}

/* Initializes the palette using the pre-generated array created by convimg.
 * The colors will be defined using an enum.
 */
static inline void palette_init(void) {
	gfx_SetPalette(global_palette, sizeof_global_palette,
		sprites_palette_offset);
	gfx_SetTextTransparentColor(TRANSPARENT);
	gfx_SetTransparentColor(TRANSPARENT);
	gfx_SetTextBGColor(TRANSPARENT);
}

/* Read user input and be responsible for booting into a game the user selects.
 * Returns true if the user requested the program to exit.
 */
static void selection_screen(void) {
	extern int listcur;

	const char *msgs[] = {
		"a) Use the up and down arrow",
		"keys to control the cursor.",
		"b) Press 2nd to boot the game.",
		"c) To exit, press Clear.",
		"d) Use arrow keys for movement.",
		"Also, Sudoku uses numpad. You",
		"can only place stuff if you're",
		"allowed to in that cell. FYI.",
		NULL,
	};

	// The image to be displayed by when a user hovers over the icon.
	const gfx_sprite_t *sprites[N_GAMES] = {
		sprite_sudoku,
		sprite_sokoban,
		sprite_2048,
		sprite_snake,
	};

	void (*gameloops[N_GAMES])(void) = {
		sudoku_mainloop,
		sokoban_mainloop,
		game2048_mainloop,
		snake_mainloop,
	};

	for (;;) {
		gfx_FillScreen(WHITE);
		g_list(list_items, MENU_LEFT_PADDING, MENU_TOP_PADDING);
		g_sel(MENU_LEFT_PADDING - MENU_CURSOR_WIDTH, MENU_TOP_PADDING);
		gfx_SetTextFGColor(BLUE);
		g_list(msgs, LCD_WIDTH - gfx_GetStringWidth(msgs[4]) - MENU_LEFT_PADDING, MENU_TOP_PADDING);
		gfx_ScaledSprite_NoClip(
			sprites[listcur],
			LCD_WIDTH / 2 - sprites[listcur]->width * 2, LCD_HEIGHT - sprites[listcur]->height*4,
			4, 4
		);
		gfx_SwapDraw();

		int key;
		while (!(key = os_GetCSC()))
			;

		if (key == sk_Up) {
			--listcur;
		} else if (key == sk_Down) {
			++listcur;
		} else if (key == sk_Clear) {
			return;
		} else if (key == sk_2nd) {
			gameloops[listcur]();
		} else {
			continue;
		}

		if (listcur < 0) {
			listcur = N_GAMES - 1;
		} else if (listcur >= (int) N_GAMES) {
			listcur = 0;
		}
	}
}
