#include <graphx.h>
#include <tice.h>
#include <keypadc.h>
#include <sys/timers.h> // usleep
#include <stdio.h>

#include "snake_app.h"
#include "common.h"

#define vertdata share.snake_bss.vertdata

/* How many nodes does food add to the snake*/
#define FOOD_GROWTH 1
#define SNAKE_COLOR BLACK
#define FOOD_COLOR GREEN
#define SNAKE_UWAIT 100000

static char direction_table[][2] = {
	{-1,  0 },
	{ 1,  0 },
	{ 0, -1 },
	{ 0,  1 },
};

enum LookDir {
	D_LEFT = 0,
	D_RIGHT,
	D_UP,
	D_DOWN,
};

static struct Vertex *vert_tail_shift(struct Vertex *tail);
static struct Vertex *vert_wrap_next(struct Vertex *vert);
static bool vert_snake_collcheck(struct Vertex *tail, struct Vertex *head, struct Vertex vert);

static struct Vertex random_vert(void);

static void draw_snake(struct Vertex *tail, struct Vertex *head);
static void draw_food(struct Vertex food);

void snake_mainloop(void);

extern union Shared share;

void snake_mainloop(void) {
	struct Vertex *tail, *head, food;
	uint24_t score, tail_growth;
	enum LookDir head_dir;

	tail = &vertdata[0];
	head = &vertdata[1];
	score = 0;
	tail_growth = 0;

	tail->x = 5;
	tail->y = 5;
	*head = *tail;
	head_dir = D_RIGHT;

	// Give the snake some food to make it look like an actual
	// snake
	food = *head;
	
	for (;;) {
		if (head->x == food.x && head->y == food.y) {
			do {
				food = random_vert();
			} while (vert_snake_collcheck(tail, head, food));
			tail_growth += FOOD_GROWTH;
			score += FOOD_GROWTH;
		}

		gfx_FillScreen(WHITE);
		draw_snake(tail, head);
		draw_food(food);
		gfx_SwapDraw();

		uint8_t key = get_single_key_pressed();
		enum LookDir prev_dir = head_dir;

		switch (key) {
			case sk_Left:
				if (head_dir != D_RIGHT)
					head_dir = D_LEFT;
				break;
			case sk_Right:
				if (head_dir != D_LEFT)
					head_dir = D_RIGHT;
				break;
			case sk_Up:
				if (head_dir != D_DOWN)
					head_dir = D_UP;
				break;
			case sk_Down:
				if (head_dir != D_UP)
					head_dir = D_DOWN;
				break;
			case sk_Clear:
				return;
		}

		// This is done because if we check if the head intersects with any part of the snake,
		// it will always return true because the head is apart of the snake.
		struct Vertex future_head = *head;
		future_head.x += direction_table[head_dir][0];
		future_head.y += direction_table[head_dir][1];

		if (future_head.x < 0 || future_head.x >= SNAKE_GRID_WIDTH ||
				future_head.y < 0 || future_head.y >= SNAKE_GRID_HEIGHT) {
			break;
		}

		if (vert_snake_collcheck(tail, head, future_head)) {
			break;
		}

		// Let the tail catch up before you (possibly) create a
		// new vertex. Bad behavior *might* occur otherwise, but are
		// only an unlikely contingency.
		if (tail_growth > 0) {
			--tail_growth;
		} else {
			tail = vert_tail_shift(tail);
		}

		// Changing the direction of the snake creates a new vertex
		if (prev_dir != head_dir) {
			head = vert_wrap_next(head);
		}
		*head = future_head;
		usleep(SNAKE_UWAIT);
	}

	// 2^24 - 1 = 16,777,215 (8 chars)
	// "HS: " (4 chars) + score (8 chars) + '\0' (1 char) = 13
	// Round up to 16
	char str_score_msg[16];
	snprintf(str_score_msg, sizeof(str_score_msg), "HS: %d", score);
	gfx_SetTextFGColor(BLACK);
	gfx_PrintStringXY(str_score_msg, GFX_LCD_WIDTH / 2, GFX_LCD_HEIGHT / 2);
	gfx_SwapDraw();

	usleep(500000);

	while (!os_GetCSC())
		;
}

/* Draw the snake's vertices by connecting them */
static void draw_snake(struct Vertex *tail, struct Vertex *head) {
	gfx_SetColor(SNAKE_COLOR);

	struct Vertex *node = tail;
	int x1, x2, y1, y2;

	while (node != head) {
		struct Vertex *next = vert_wrap_next(node);
		x1 = node->x;
		x2 = next->x;
		enforce_lt(&x1, &x2);

		y1 = node->y;
		y2 = next->y;
		enforce_lt(&y1, &y2);

		gfx_FillRectangle(
			x1 * SNAKE_PX_STRIDE, y1 * SNAKE_PX_STRIDE,
			(x2 - x1 + 1) * SNAKE_PX_STRIDE, (y2 - y1 + 1) * SNAKE_PX_STRIDE
		);
		node = next;
	}
}

static void draw_food(struct Vertex food) {
	gfx_SetColor(FOOD_COLOR);
	gfx_FillRectangle(
		food.x * SNAKE_PX_STRIDE, food.y * SNAKE_PX_STRIDE,
		SNAKE_PX_STRIDE, SNAKE_PX_STRIDE
	);
}

/* Returns a random vertex in the snake grid. */
static struct Vertex random_vert(void) {
	struct Vertex vert = {
		.x = randInt(0, SNAKE_GRID_WIDTH - 1),
		.y = randInt(0, SNAKE_GRID_HEIGHT - 1),
	};
	return vert;
}

/* Iterates the snake and checks if a vert intersects with any segment of it.
 * Returns true if any overlapping was found, and false otherwise.
 */
static bool vert_snake_collcheck(struct Vertex *tail, struct Vertex *head, struct Vertex vert) {
	struct Vertex *node = tail;

	while (node != head) {
		struct Vertex *next = vert_wrap_next(node);
		int x1, x2, y1, y2;
		x1 = node->x;
		x2 = next->x;
		enforce_lt(&x1, &x2);

		y1 = node->y;
		y2 = next->y;
		enforce_lt(&y1, &y2);

		if (x1 <= vert.x && vert.x <= x2 && y1 <= vert.y && vert.y <= y2) {
			return true;
		}
		node = next;
	}
	return false;
}

/* Shift the tail vertex in the direction of the next
 * vertex in the snake vertex chain.
 * Returns the new tail vertex pointer.
 */
static struct Vertex *vert_tail_shift(struct Vertex *tail) {
	struct Vertex *next;
	int dx, dy;

	next = vert_wrap_next(tail);

	dx = sign(next->x - tail->x);
	dy = sign(next->y - tail->y);
	tail->x += dx;
	tail->y += dy;
	// If what tail and next point to are the same,
	// then tail should be "disposed of" by returning a
	// different pointer
	if (tail->x == next->x && tail->y == next->y) {
		return next;
	} else {
		return tail;
	}
}

/* Since vertdata is a circular array, this function simply returns
 * the next element after n.
 */
static struct Vertex *vert_wrap_next(struct Vertex *n) {
	++n;
	if (n > &vertdata[SNAKE_VERTDATA_LEN - 1]) {
		return vertdata;
	} else {
		return n;
	}
}