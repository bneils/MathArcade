#ifndef SOKOBAN_APP_H
#define SOKOBAN_APP_H

enum SBTile {
	S_FLOOR = 0,
	S_BOX,
	S_WALL,
	S_PLAYER,
	S_DEST,
};

void sokoban_mainloop(void);

#endif // SOKOBAN_APP_H