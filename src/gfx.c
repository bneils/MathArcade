#include "common.h"
#include <graphx.h>

int listcur;

/* Takes a list of strings terminated by (char *) NULL and displays them on the
 * screen. 
 */
void g_list(const char *s[], int x, int y) {
	gfx_SetTextFGColor(BLACK);
	for (int i = 0; s[i] != NULL; ++i) {
		gfx_PrintStringXY(s[i], x, y);
		y += CHAR_HEIGHT;
	}
}

/* An arrow is printed before the ith row stored in listcur */
void g_sel(int x, int y) {
	extern int listcur;
	gfx_SetTextFGColor(BLACK);
	gfx_PrintStringXY("->", x, y + listcur * CHAR_HEIGHT);
}

