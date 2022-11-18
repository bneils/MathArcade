#include "common.h"

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
