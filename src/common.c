#include "common.h"
#include <keypadc.h>

union Shared share;

/* Searches a buffer for a non-zero value, returning true if there is,
 * or false if the buffer is all zero.
 */
bool any(const void *p, size_t nmemb, size_t size) {
	const char *b = p;
	for (size_t i = 0; i < nmemb * size; ++i) {
		if (b[i] != 0) {
			return true;
		}
	}
	return false;
}

/* Searches a buffer for a non-zero value, returning true if there is,
 * or false if the buffer is all zero.
 */
bool all(const void *p, size_t nmemb, size_t size) { 
	for (const void *r = p + nmemb * size; p < r; p += size) {
		if (!any(p, 1, size)) {
			return false;
		}
	}
	return true;
}

// code by jacobly
uint8_t get_single_key_pressed(void) {
	static uint8_t last_key;
	uint8_t only_key = 0;
	kb_Scan();
	for (uint8_t key = 1, group = 7; group; --group) {
		for (uint8_t mask = 1; mask; mask <<= 1, ++key) {
			if (kb_Data[group] & mask) {
				if (only_key) {
					last_key = 0;
					return 0;
				} else {
					only_key = key;
				}
			}
		}
	}
	if (only_key == last_key) {
		return 0;
	}
	last_key = only_key;
	return only_key;
}

/* Returns the sign of an integer. */
int sign(int a) {
	if (a > 0) return 1;
	if (a < 0) return -1;
	return 0;
}

void enforce_lt(int *a, int *b) {
	if (*a > *b) {
		int t = *a;
		*a = *b;
		*b = t;
	}
}