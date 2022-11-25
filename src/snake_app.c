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

/* At no point should tail == head */
struct Snake {
	struct Pos *tail;
	struct Pos *head;
};

static void vert_tail_shift(struct Snake *);
static struct Pos *vert_wrap_next(struct Pos *vert);
static bool vert_snake_collcheck(struct Snake, struct Pos vert);

static struct Pos random_vert(void);

static void draw_snake(struct Snake);
static void draw_food(struct Pos);

void snake_mainloop(void) {
	struct Snake snake;
	struct Pos food;
	uint24_t score, tail_growth;
	enum LookDir head_dir;

	snake.tail = &vertdata[0];
	snake.head = &vertdata[1];
	score = 0;
	tail_growth = 0;

	*snake.head = *snake.tail = random_vert();
	// I'm doing this to give the player some time to react
	head_dir = (snake.head->x < SNAKE_GRID_WIDTH / 2) ? D_RIGHT : D_LEFT;

	// This is done instead of increasing tail_growth to avoid
	// repeating the logic of finding a spot that is untouched by the snake's head.
	food = *snake.head;
	
	for (;;) {
		if (snake.head->x == food.x && snake.head->y == food.y) {
			// It's important that the food doesn't spawn on the snake
			// as it would suck to wait for the snake to move off the food
			// to eat it.
			// Also, I hate it when the food spawns on the edge, it's usually
			// a game over.
			do {
				food = random_vert();
			} while (food.x == 0 || food.y == 0 ||
				food.x == SNAKE_GRID_WIDTH - 1 || food.y == SNAKE_GRID_HEIGHT - 1 ||
				vert_snake_collcheck(snake, food)
			);
			
			tail_growth += FOOD_GROWTH;
			score += FOOD_GROWTH;
		}

		gfx_FillScreen(WHITE);
		draw_snake(snake);
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
		struct Pos future_head = *snake.head;
		future_head.x += direction_table[head_dir][0];
		future_head.y += direction_table[head_dir][1];

		if (future_head.x < 0 || future_head.x >= SNAKE_GRID_WIDTH ||
				future_head.y < 0 || future_head.y >= SNAKE_GRID_HEIGHT) {
			break;
		}

		if (vert_snake_collcheck(snake, future_head)) {
			*snake.head = future_head; // To show graphically.
			break;
		}

		// Changing the direction of the snake creates a new vertex
		if (prev_dir != head_dir) {
			snake.head = vert_wrap_next(snake.head);
		}
		*snake.head = future_head;

		// Let the tail catch up before you (possibly) create a
		// new vertex. Bad behavior *might* occur otherwise, but are
		// only an unlikely contingency.
		if (tail_growth > 0) {
			--tail_growth;
		} else {
			vert_tail_shift(&snake);
		}

		
		usleep(SNAKE_UWAIT);
	}

	// To show the player where the head intersects with the snake, since
	// it'll be a bit hard to tell
	gfx_FillScreen(WHITE);
	draw_snake(snake);
	gfx_SetColor(RED);
	gfx_FillRectangle(
		snake.head->x * SNAKE_PX_STRIDE, snake.head->y * SNAKE_PX_STRIDE,
		SNAKE_PX_STRIDE, SNAKE_PX_STRIDE
	);

	// 2^24 - 1 = 16,777,215 (8 chars)
	// "SCORE: " (7 chars) + score (8 chars) + '\0' (1 char) = 16
	char str_score_msg[16];
	snprintf(str_score_msg, sizeof(str_score_msg), "SCORE: %d", score);
	
	gfx_SetColor(GRAY1);
	gfx_FillRectangle(15, 15, gfx_GetStringWidth(str_score_msg) + 10, 18);
	gfx_SetTextFGColor(BLACK);
	gfx_PrintStringXY(str_score_msg, 20, 20);
	gfx_SwapDraw();

	usleep(500000);

	while (!os_GetCSC())
		;
}

/* Draw the snake's vertices by connecting them */
static void draw_snake(struct Snake snake) {
	gfx_SetColor(SNAKE_COLOR);

	struct Pos *node;
	int x1, x2, y1, y2;

	node = snake.tail;
	while (node != snake.head) {
		struct Pos *next = vert_wrap_next(node);
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

static void draw_food(struct Pos food) {
	gfx_SetColor(FOOD_COLOR);
	gfx_FillRectangle(
		food.x * SNAKE_PX_STRIDE, food.y * SNAKE_PX_STRIDE,
		SNAKE_PX_STRIDE, SNAKE_PX_STRIDE
	);
}

/* Returns a random vertex in the snake grid. */
static struct Pos random_vert(void) {
	struct Pos vert = {
		.x = randInt(0, SNAKE_GRID_WIDTH - 1),
		.y = randInt(0, SNAKE_GRID_HEIGHT - 1),
	};
	return vert;
}

/* Iterates the snake and checks if a vert intersects with any segment of it.
 * Returns true if any overlapping was found, and false otherwise.
 */
static bool vert_snake_collcheck(struct Snake snake, struct Pos vert) {
	struct Pos *node = snake.tail;

	while (node != snake.head) {
		struct Pos *next = vert_wrap_next(node);
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
 * vertex in the snake vertex chain and changes the tail
 * if it has reached the next vertex.
 */
static void vert_tail_shift(struct Snake *snake) {
	struct Pos *next;
	int dx, dy;

	if (snake == NULL) {
		return;
	}

	next = vert_wrap_next(snake->tail);

	dx = sign(next->x - snake->tail->x);
	dy = sign(next->y - snake->tail->y);
	snake->tail->x += dx;
	snake->tail->y += dy;

	// Don't attempt to merge the tail and head vertices as that
	// causes a lot of problems
	if (next == snake->head) {
		return;
	}

	// If what tail and next point to are the same after this shift
	// then tail should be "disposed of" by changing the tail
	if (snake->tail->x == next->x && snake->tail->y == next->y) {
		snake->tail = next;
	}
}

/* Since vertdata is a circular array, this function simply returns
 * the next element after n.
 */
static struct Pos *vert_wrap_next(struct Pos *n) {
	++n;
	if (n > &vertdata[SNAKE_VERTDATA_LEN - 1]) {
		return vertdata;
	} else {
		return n;
	}
}