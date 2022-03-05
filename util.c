/**
 * @file util.c
 */
#include "util.h"
#include <string.h>

int min(int a, int b) { return (a < b ? a : b); }

int max(int a, int b) { return (a > b ? a : b); }

size_t strncpy_safe(char *dst, const char *src, size_t maxlen) {
  // Based on Apple's implementation of @a strlcpy in XNU.

  const size_t srclen = strlen(src);

  if (srclen + 1 < maxlen) {
    memcpy(dst, src, srclen + 1); // NOLINT -- Size checked.
  } else if (maxlen != 0) {
    memcpy(dst, src, maxlen - 1); // NOLINT -- Size checked.
    dst[maxlen - 1] = '\0';
  }

  return srclen;
}