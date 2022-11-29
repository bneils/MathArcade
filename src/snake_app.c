#include <graphx.h>
#include <tice.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <stdio.h>
#include <debug.h>

#include "common.h"

#define vertdata share.snake_bss.vertdata

#define FOOD_VALUE 1
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

static void move_tail(struct Snake *);
static inline struct Pos *next_vertex(struct Pos *vert);
static bool collcheck(struct Snake, struct Pos vert);
static struct Pos random_vert(void);
static void draw_snake(struct Snake);
static void draw_food(struct Pos);
static void keep_lte(uint8_t *, uint8_t *);
static bool iteredges(struct Snake snake, struct Pos *dp1, struct Pos *dp2);

void snake_mainloop(void)
{
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

	// Done to avoid repetitive logic
	food = *snake.head;

	for (;;) {
		if (snake.head->x == food.x && snake.head->y == food.y) {
			do {
				food = random_vert();
			} while (
				food.x == 0 || food.y == 0 ||
				food.x == SNAKE_GRID_WIDTH - 1 ||
				food.y == SNAKE_GRID_HEIGHT - 1 ||
				collcheck(snake, food)
			);

			tail_growth += FOOD_VALUE;
			score += FOOD_VALUE;
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

		// This is done because if we check if the head intersects
		// with any part of the snake, it will always return true
		// because the head is apart of the snake.
		struct Pos future_head = *snake.head;
		future_head.x += direction_table[head_dir][0];
		future_head.y += direction_table[head_dir][1];

		if (future_head.x < 0 || future_head.x >= SNAKE_GRID_WIDTH ||
			future_head.y < 0 || future_head.y >= SNAKE_GRID_HEIGHT)
			break;

		if (collcheck(snake, future_head)) {
			*snake.head = future_head;
			break;
		}

		// Changing the direction of the snake creates a new vertex
		if (prev_dir != head_dir)
			snake.head = next_vertex(snake.head);
		*snake.head = future_head;

		// Let the tail catch up before you (possibly) create a
		// new vertex. Bad behavior *might* occur otherwise, but are
		// only an unlikely contingency.
		if (tail_growth > 0)
			--tail_growth;
		else
			move_tail(&snake);

		usleep(SNAKE_UWAIT);
	}

	gfx_FillScreen(WHITE);
	draw_snake(snake);
	gfx_SetColor(RED);
	gfx_FillRectangle(
		snake.head->x * SNAKE_PX_STRIDE, snake.head->y * SNAKE_PX_STRIDE,
		SNAKE_PX_STRIDE, SNAKE_PX_STRIDE
	);

	// 2^24 - 1 = 16,777,215 (8 chars)
	// "SCORE: " (7 chars) + score (8 chars) + '\0' (1 char) = 16
	char str_score_msg[32];
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
static void draw_snake(struct Snake snake)
{
	gfx_SetColor(SNAKE_COLOR);

	struct Pos p1, p2;

	iteredges(snake, NULL, NULL);
	while (iteredges(snake, &p1, &p2)) {
		gfx_FillRectangle(
			p1.x * SNAKE_PX_STRIDE, p1.y * SNAKE_PX_STRIDE,
			(p2.x - p1.x + 1) * SNAKE_PX_STRIDE, (p2.y - p1.y + 1) * SNAKE_PX_STRIDE
		);
	}
}

static void draw_food(struct Pos food)
{
	gfx_SetColor(FOOD_COLOR);
	gfx_FillRectangle(
		food.x * SNAKE_PX_STRIDE, food.y * SNAKE_PX_STRIDE,
		SNAKE_PX_STRIDE, SNAKE_PX_STRIDE
	);
}

/* Returns a random vertex in the snake grid. */
static struct Pos random_vert(void)
{
	struct Pos vert = {
		.x = randInt(0, SNAKE_GRID_WIDTH - 1),
		.y = randInt(0, SNAKE_GRID_HEIGHT - 1),
	};
	return vert;
}

/* Iterates the snake and checks if a vert intersects with any segment of it.
 * Returns true if any overlapping was found, and false otherwise.
 */
static bool collcheck(struct Snake snake, struct Pos vert)
{
	struct Pos p1, p2;

	iteredges(snake, NULL, NULL);
	while (iteredges(snake, &p1, &p2)) {
		if (p1.x <= vert.x && vert.x <= p2.x && p1.y <= vert.y && vert.y <= p2.y)
			return true;
	}
	return false;
}

/* Shift the tail vertex in the direction of the next
 * vertex in the snake vertex chain and changes the tail
 * if it has reached the next vertex.
 */
static void move_tail(struct Snake *snake) {
	struct Pos *next;
	int8_t dx, dy;

	if (!snake)
		return;

	next = next_vertex(snake->tail);

	dx = sign(next->x - snake->tail->x);
	dy = sign(next->y - snake->tail->y);
	snake->tail->x += dx;
	snake->tail->y += dy;

	// The head and tail vertices shouldn't be merged at all,
	// it breaks a lot of stuff
	if (next == snake->head)
		return;

	// If what tail and next point to are the same after this shift
	// then tail should be "disposed of" by changing the tail
	if (snake->tail->x == next->x && snake->tail->y == next->y)
		snake->tail = next;
}

/* Since vertdata is a circular array, this function simply returns
 * the next element after n.
 */
static inline struct Pos *next_vertex(struct Pos *n) {
	++n;
	return (n <= &vertdata[SNAKE_VERTDATA_LEN - 1]) ? n : vertdata;
}

/* Ensure that a <= b by swapping */
static void keep_lte(uint8_t *a, uint8_t *b) {
	if (*a > *b) {
		uint8_t t = *a;
		*a = *b;
		*b = t;
	}
}

/* Iterates through a snake's edges.
 * The function must first be called with NULL for dp1 and dp2 to
 * initialize the function's state to begin at the snake's tail.
 * Returns false if no edges exist (or if it was just initialized)
 * or true if dp1 and dp2 were set.
 */
static bool iteredges(struct Snake snake, struct Pos *dp1, struct Pos *dp2)
{
	static struct Pos *node;
	struct Pos *next;

	if (dp1 == NULL && dp2 == NULL) {
		node = snake.tail;
		return false;
	}

	if (node == snake.head)
		node = NULL;

	if (node == NULL)
		return false;

	next = next_vertex(node);
	*dp1 = *node;
	*dp2 = *next;
	keep_lte(&dp1->x, &dp2->x);
	keep_lte(&dp1->y, &dp2->y);

	node = next;
	return true;
}