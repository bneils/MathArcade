#include <graphx.h>
#include <sys/util.h>
#include <tice.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "sokoban_app.h"
#include "common.h"

/*
To make this game you need the sprites:
brick wall
checkered floor
box
4-way player
box destination

all sprites will be 16x16
there are 4 tile sprites + 4 player sprites
that occupies 2kb.

the 8 levels occupy ~kb
*/

// *determined by a script
#define LEVEL_WIDTH 22
#define LEVEL_HEIGHT 17
#define NGAMES 8

//this is negative
#define LOFF ((GFX_LCD_WIDTH - (LEVEL_WIDTH * 16))/2)
#define pos(x, y) ((x)+(y)*LEVEL_WIDTH)

extern uint8_t sokoban_levels[NGAMES][LEVEL_HEIGHT][LEVEL_WIDTH];
static bool exitflag;

static void sokoban_player(int levelid);
static bool validate_board(uint8_t *level, int levelidx);

enum PlayerDirection {
	S_UP,
	S_DOWN,
	S_LEFT,
	S_RIGHT
};

int playerx, playery;
enum PlayerDirection dir = S_DOWN;



// heuristic
static int levelcellshifts[NGAMES][2] = {
	{0, 2},
	{3, 2},
	{2, 2},
	{0, 1},
	{2, 1},
	{4, 2},
	{4, 2},
	{3, -1},
};

static void draw_level(uint8_t *level, uint8_t levelidx) {
	int offx = levelcellshifts[levelidx][0] * 16;
	int offy = levelcellshifts[levelidx][1] * 16;
	gfx_FillScreen(WHITE);
	for (int y = 0; y < LEVEL_HEIGHT; ++y) {
		for (int x = 0; x < LEVEL_WIDTH; ++x) {
			bool isplayer=false;
			gfx_sprite_t *sprite;
			int i = y * LEVEL_WIDTH + x;
			switch (level[i]) {
				case S_BOX:
					sprite = sprite_sokoban_box;
					break;
				case S_WALL:
					sprite = sprite_sokoban_wall;
					break;
				case S_PLAYER:
					isplayer=true;
					switch (dir) {
						case S_UP: sprite = sprite_sokoban_up; break;
						case S_DOWN: sprite = sprite_sokoban_down; break;
						case S_RIGHT: sprite = sprite_sokoban_right; break;
						case S_LEFT:
						default: sprite = sprite_sokoban_left; break;
					}
					break;
				case S_DEST:
					sprite = sprite_sokoban_goal;
					break;
				case S_FLOOR:
				default:
					sprite=NULL;
					break;
			}
			if (isplayer) {
				if (sokoban_levels[levelidx][y][x] == S_DEST)
					gfx_Sprite(sprite_sokoban_goal, x*16+LOFF+offx,y*16+offy-1);
				gfx_TransparentSprite(sprite, x*16+LOFF+offx,y*16+offy-1);
			}
			else if (sprite) {
				gfx_Sprite(sprite, x*16+LOFF+offx,y*16+offy-1);
			}
		}
	}
}

void sokoban_mainloop(void) {
	exitflag = false;
	for (int i = 0; i < NGAMES; ++i) {
		gfx_FillScreen(WHITE);
		char s[16];
		sprintf(s, "LEVEL %d", i);
		gfx_PrintStringXY(s, (GFX_LCD_WIDTH - gfx_GetStringWidth(s) )/ 2, GFX_LCD_HEIGHT / 2);
		gfx_SwapDraw();
		usleep(1000000);
		sokoban_player(i);
		if (exitflag) return;
	}
}

static void sokoban_player(int levelid) {
	static uint8_t sokoban_level[LEVEL_HEIGHT][LEVEL_WIDTH];
	uint8_t *sokoban_old = (uint8_t *) sokoban_levels[levelid];
	memcpy(sokoban_level, sokoban_levels[levelid], sizeof(sokoban_levels[0]));

	int playerx, playery;
	enum SBTile lasttile = S_FLOOR; // that the player was on *

	for (int y = 0; y < LEVEL_HEIGHT; ++y) {
		for (int x = 0; x < LEVEL_WIDTH; ++x) {
			if (sokoban_level[y][x] == S_PLAYER) {
				playerx = x;
				playery = y;
				goto found_player;
			}
		}
	}
	return; // did not find player :(
found_player:
	for (;;) {
		int key;

		draw_level((uint8_t *) sokoban_level, levelid);
		gfx_SwapDraw();

		while (!(key = os_GetCSC()))
			;
		//validate if the player can move there.
		//a player can move somewhere if:
		//  there is at least 1 floor/dest in the direction they are going before a ray hits a wall
		int dx, dy;

		switch (key) {
			case sk_Left:
				dir = S_LEFT;
				dx = -1;
				dy = 0;
				break;
			case sk_Right:
				dir = S_RIGHT;
				dx = 1;
				dy = 0;
				break;
			case sk_Up:
				dir = S_UP;
				dx = 0;
				dy = -1;
				break;
			case sk_Down:
				dir = S_DOWN;
				dx = 0;
				dy = 1;
				break;
			case sk_Clear:
				exitflag=true;
				return;
			default:
				continue; // bad key, don't do anything.
		}

		int nextx, nexty;
		nextx = playerx+dx;
		nexty = playery+dy;
		bool can_move;
		
		if (sokoban_level[nexty][nextx] == S_WALL) can_move = false;
		else if (sokoban_level[nexty][nextx] == S_FLOOR || sokoban_level[nexty][nextx] == S_DEST) can_move = true;
		else if (sokoban_level[nexty][nextx] == S_BOX) {
			nextx+=dx;
			nexty+=dy;
			can_move = (sokoban_level[nexty][nextx] == S_FLOOR || sokoban_level[nexty][nextx] == S_DEST);
		} else {
			continue; // dunno?
		}
		if (!can_move) {
			continue;
		}
		sokoban_level[playery][playerx] = lasttile;
		playerx += dx;
		playery += dy;
		lasttile = sokoban_level[playery][playerx];
		// first case.
		if (sokoban_level[playery][playerx] == S_BOX) {
			sokoban_level[playery+dy][playerx+dx] = S_BOX;
			//ok so, the box may be located on a DEST tile (because we moved it there.)
			//so it is sufficient to check the original (old) tiledata to see if a destination was originally there.
			if (sokoban_old[pos(playerx,playery)] == S_DEST)
				lasttile = S_DEST; // this is done because when the player moves off it, the tile will be restored (for visual aspects)
		}
		if (lasttile == S_BOX) lasttile = S_FLOOR; // bug ig

		sokoban_level[playery][playerx] = S_PLAYER;
		
		if (validate_board((uint8_t *)sokoban_level, levelid))
			return; //  bc you completed it
	}
}

static bool validate_board(uint8_t *level, int levelidx) {
	uint8_t *levelold = (uint8_t *)sokoban_levels[levelidx];
	for (int i = 0; i < LEVEL_WIDTH*LEVEL_HEIGHT; ++i) {
		if (*levelold == S_DEST && *level != S_BOX)
			return false;
		++levelold;
		++level;
	}
	return true;
}