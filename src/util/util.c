/**
 * @file util.c
 * @ingroup UtilModule
 */
#include "util.h"
#include <string.h>

int max(int a, int b) {
  return (a > b ? a : b);
}

int min(int a, int b) {
  return (a < b ? a : b);
}

size_t strncpy_safe(char *dest, size_t destLen, const char *src) {
  // Based on Apple's implementation of @a strlcpy in XNU.

  const size_t srcLen = strlen(src);

  if (srcLen + 1 < destLen) {
    memcpy(dest, src, srcLen + 1); // NOLINT -- Size checked.
  } else if (destLen != 0) {
    memcpy(dest, src, destLen - 1); // NOLINT -- Size checked.
    dest[destLen - 1] = '\0';
  }

  return srcLen;
}

unsigned int umax(unsigned int a, unsigned int b) {
  return (a > b ? a : b);
}

unsigned int umin(unsigned int a, unsigned int b) {
  return (a < b ? a : b);
}
