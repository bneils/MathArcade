#include <graphx.h>
#include <tice.h>
#include <keypadc.h>
#include <sys/timers.h> // usleep
#include <stdio.h>

#include "snake_app.h"
#include "common.h"

#define vertdata share.snake_bss.vertdata

static void draw_snake(struct Vertex *tail, struct Vertex *head, const struct Vertex *l, const struct Vertex *r, struct Vertex *foodloc);
static bool check_point_on_edge(struct Vertex *tail, struct Vertex *head, const struct Vertex *l, const struct Vertex *r, struct Vertex *pt);
static int sign(int a);
static bool isbetween(int pt1, int n, int pt2);
static void random_food(struct Vertex *loc);
void snake_mainloop(void);

extern union Shared share;

void snake_mainloop(void) {
    struct Vertex *tailptr, *followptr, *headptr;
    // the *tailptr increments in the direction of the *followptr.
    // if tailptr==followptr or *tailptr==*followptr, nothing happens.
    // *headptr increments in the snake's direction.
    // when the snake changes direction, headptr shifts in the array by 1 and copies itself to the new location.
    tailptr = vertdata+0;
    headptr = followptr = vertdata+1;

    // represents the last element of the vertex data
    const struct Vertex *vertlast = vertdata + sizeof(vertdata) - 1;

    tailptr->x = 5;
    tailptr->y = 5;
    headptr->x = 6;
    headptr->y = 5;

    struct Vertex foodloc;

    random_food(&foodloc);
    uint24_t score=0;
    int dirx = 1, diry = 0;
    for (;;) {
        int num_tail_moves = 1;
        if (headptr->x == foodloc.x && headptr->y == foodloc.y) {
            do {
                random_food(&foodloc);
            } while (check_point_on_edge(tailptr, headptr, vertdata, vertlast, &foodloc));
            
            num_tail_moves = 0;
            ++score;
        }

        draw_snake(tailptr, headptr, vertdata, vertlast, &foodloc);
        bool dir_changed = false;
        
        kb_Scan();
        if (kb_Data[7] & kb_Left) {
            if (dirx == 1) goto skip_dir_change;
            if (dirx != -1) dir_changed = true;
            dirx = -1;
            diry = 0;
        } else if (kb_Data[7] & kb_Right) {
            if (dirx == -1) goto skip_dir_change;
            if (dirx != 1) dir_changed = true;
            dirx = 1;
            diry = 0;
        } else if (kb_Data[7] & kb_Up) {
            if (diry == 1) goto skip_dir_change;
            if (diry != -1) dir_changed = true;
            dirx = 0;
            diry = -1;
        } else if (kb_Data[7] & kb_Down) {
            if (diry == -1) goto skip_dir_change;
            if (diry != 1) dir_changed = true;
            dirx = 0;
            diry = 1;
        } else if (kb_Data[6] & kb_Clear) { // quit the game
            return;
        }
skip_dir_change: // for trying to go in opp directions

        if (dir_changed) {
            struct Vertex *old = headptr;
            headptr++;
            if (headptr > vertlast) headptr = vertdata; // another wrap around
            *headptr = *old;
            num_tail_moves++;
        }

        // predictive check for head pointer.
        struct Vertex next_loc = *headptr;
        next_loc.x += dirx;
        next_loc.y += diry;
        if (check_point_on_edge(tailptr, headptr, vertdata, vertlast, &next_loc)) {
            break;
        }
        *headptr = next_loc;
        
        if (headptr->x < 0 || headptr->x >= SNAKE_GRID_WIDTH || headptr->y < 0 || headptr->y >= SNAKE_GRID_HEIGHT)
            break; // they ded

        // dont know why the fix is to make the tail move more depending on whether or not the snake grew,
        // or if the snake change dirs. It's obvious to make the snake stop if 
        for (int i = 0; i < num_tail_moves; ++i) {
            int dx, dy;
            dx = sign(followptr->x - tailptr->x);
            dy = sign(followptr->y - tailptr->y);
            
            // if the tailptr VERTEX meets the next vertex, you need to merge the two vertices
            // by setting tailptr=followptr and moving followptr to the next vertex in the line.
            if (dx == 0 && dy == 0) {
                tailptr = followptr; // this is done to avoid a wrapping check, even if it's just +1
                followptr++;

                if (followptr > vertlast)
                    followptr = vertdata;
                continue;
            }

            // this follows the assumption that ((dx!=0) AND (dy!=0)) is a contradiction.
            tailptr->x += dx;
            tailptr->y += dy;
        }

        usleep(100000); // 0.1s
    }
    //ok so, there's 3072 cells = 64*48, so it makes sense there couldn't really be more food than that
    //that's just a weird math upper bound/intuition thing tho.
    //anyways,that woul take 5 bytes(including \0),plus 4 for "HS: "
    //that's 9 bytes. I'm reserving 10 here, so that's by reasoning for why this is ok.. maybe not the safest.
    //but it shouldn't buffer overflow.
    char str_score[10];
    snprintf(str_score, sizeof(str_score), "HS: %d", score);
    gfx_SetTextFGColor(GREEN);
    gfx_FillScreen(WHITE);
    gfx_PrintStringXY(str_score, GFX_LCD_WIDTH / 2, GFX_LCD_HEIGHT / 2);
    gfx_SwapDraw();

    //exhaust this shit
    while (os_GetCSC())
        ;
    usleep(500000);//yk what? I'm gonna wait .5 sec. It shouldn't even matter if they press a key.

    while (!os_GetCSC())
        ;
}

/* gives the sign of an integer. */
static int sign(int a) {
    if (a > 0) return 1;
    if (a < 0) return -1;
    return 0;
}

static void draw_snake(struct Vertex *tail, struct Vertex *head, const struct Vertex *l, const struct Vertex *r, struct Vertex *foodloc) {
    gfx_FillScreen(WHITE);
    gfx_SetColor(BLACK);

    const struct Vertex *node = tail;
    for (;;) {
        const struct Vertex *next = node + 1;
        if (next > r) next = l;

        // now, increment between the verts (you need to create a copy to not affect tail shit)
        struct Vertex px = *node;
        int dx = sign(next->x - node->x);
        int dy = sign(next->y - node->y);
        while (px.x != next->x || px.y != next->y) {
            gfx_FillRectangle(px.x*SNAKE_PX_STRIDE, px.y*SNAKE_PX_STRIDE, SNAKE_PX_STRIDE, SNAKE_PX_STRIDE);
            px.x += dx;
            px.y += dy;
        }
        gfx_FillRectangle(px.x*SNAKE_PX_STRIDE, px.y*SNAKE_PX_STRIDE, SNAKE_PX_STRIDE, SNAKE_PX_STRIDE); // the head does not get since it stops immediately once it reaches it

        if (next == head) break;    // last edge to be checked
        node = next; // done instead of increment node, because of check done to next previously
    }

    gfx_SetColor(GREEN);
    gfx_FillRectangle(foodloc->x*SNAKE_PX_STRIDE, foodloc->y*SNAKE_PX_STRIDE, SNAKE_PX_STRIDE, SNAKE_PX_STRIDE);

    gfx_SwapDraw();
}

static void random_food(struct Vertex *loc) {
    loc->x = random() % SNAKE_GRID_WIDTH;
    loc->y = random() % SNAKE_GRID_HEIGHT;
}

static bool check_point_on_edge(struct Vertex *tail, struct Vertex *head, const struct Vertex *l, const struct Vertex *r, struct Vertex *pt) {
    const struct Vertex *node = tail;
    for (;;) {
        const struct Vertex *next = node + 1;
        if (next > r) next = l;
        // found an edge that has pt on it
        if (isbetween(node->x, pt->x, next->x) && isbetween(node->y, pt->y, next->y)) return true;

        if (next == head) break;    // last edge to be checked
        node = next; // done instead of increment node, because of check done to next previously
    }
    return false;
}

static bool isbetween(int a, int n, int b) {
    //enforce a<=b
    if (a > b) { // swap
        int t = a;
        a = b;
        b = t;
    }
    return (a <= n && n <= b);
}
