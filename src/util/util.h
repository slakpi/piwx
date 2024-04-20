/**
 * @file util.h
 */
#if !defined UTIL_H
#define UTIL_H

#include <math.h>
#include <stddef.h>

#define TINY_VALUE 1e-15
#define MAX_PATH   256

#define COUNTOF(arr) (sizeof(arr) / sizeof(*(arr)))
#define UNUSED(x)    ((void)(x))

#define RAD_TO_DEG (180.0 / M_PI)
#define DEG_TO_RAD (M_PI / 180.0)

/**
 * @brief   Returns the maximum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The maximum of @a a and @a b.
 */
int max(int a, int b);

/**
 * @brief   Returns the minimum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The minimum of @a a and @a b.
 */
int min(int a, int b);

/**
 * @brief   Safer version of @a strncpy.
 * @details @a strncpy will copy up to @a n characters to the destination and
 *          leaves null-termination up to the caller. This version takes an
 *          argument that is the length of the destination buffer, copies up to
 *          @a destLen - 1 bytes, and null terminates the buffer.
 * @param[out] dest    Destination buffer.
 * @param[in]  destLen Length of the destination buffer in bytes.
 * @param[in]  src     Source buffer.
 * @returns The number of characters that were copied or would be copied if the
 *          destination buffer were large enough.
 */
size_t strncpy_safe(char *dest, size_t destLen, const char *src);

/**
 * @brief   Returns the maximum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The maximum of @a a and @a b.
 */
unsigned int umax(unsigned int a, unsigned int b);

/**
 * @brief   Returns the minimum of two values.
 * @param[in] a First value.
 * @param[in] b Second value.
 * @returns The minimum of @a a and @a b.
 */
unsigned int umin(unsigned int a, unsigned int b);

#endif /* UTIL_H */
