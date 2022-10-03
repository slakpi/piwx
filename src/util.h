/**
 * @file util.h
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define TINY_VALUE 1e-15
#define MAX_PATH   256

#define MEMBER_OFFSET(s, m) ((void *)&(((s *)0)->m))
#define COUNTOF(arr)        (sizeof(arr) / sizeof(*(arr)))
#define UNUSED(x)           ((void)(x))

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

/**
 * @brief   Safer version of @a strncpy.
 * @details @a strncpy will copy up to @a n characters to the destination and
 *          leaves null-termination up to the caller. This version takes an
 *          argument that is the size of the destination buffer, copies up to
 *          @a dest_sz - 1 characters and null terminates the buffer.
 * @param[out] dest    Destination buffer.
 * @param[in]  src     Source buffer.
 * @param[in]  dest_sz Size of the destination buffer.
 * @returns The number of characters that were copied or would be copied if the
 *          destination buffer were large enough.
 */
size_t strncpy_safe(char *dest, const char *src, size_t dest_sz);

#endif /* UTIL_H */
