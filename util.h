/**
 * @file util.h
 */
#ifndef UTIL_H
#define UTIL_H

/**
 * @brief   Returns the minimum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The minimum of @a a and @a b.
 */
int min(int a, int b);

/**
 * @brief   Returns the maximum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The maximum of @a a and @a b.
 */
int max(int a, int b);

typedef int boolean;

#define TRUE  1
#define FALSE 0

#define COUNTOF(arr) (sizeof(arr) / sizeof(*(arr)))

#endif /* UTIL_H */
