#ifndef SOKOBAN_DATA_H
#define SOKOBAN_DATA_H
#include <stdint.h>
#define SOKOBAN_NUM_LEVELS 40
// SOKOBAN_MAX_LEVEL_SIZE does not include header size
#define SOKOBAN_MAX_LEVEL_SIZE 102
#define SOKOBAN_LEVELS_SIZE 3171
extern uint16_t sokoban_level_table[];
extern uint8_t sokoban_levels[];
#endif // SOKOBAN_DATA_H