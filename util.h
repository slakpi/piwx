#ifndef UTIL_H
#define UTIL_H

/**
 * @brief   Returns the minimum of two values.
 * @param[in] _a First value.
 * @param[in] _b Second value.
 * @returns The minimum of _a and _b.
 */
int min(int _a, int _b);

/**
 * @brief   Returns the maximum of two values.
 * @param[in] _a First value.
 * @param[in] _b Second value.
 * @returns The maximum of _a and _b.
 */
int max(int _a, int _b);

typedef int boolean;

#define TRUE 1
#define FALSE 0

#define _countof(arr) (sizeof(arr) / sizeof(*(arr)))

#endif
