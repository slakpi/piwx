/**
 * @file simd.h
 */
#if !defined SIMD_H
#define SIMD_H

#include <stdint.h>

/**
 * @brief   Convert an RGBA8888 pixel to RGB565 with premultiplied alpha.
 * @param[in] p Pointer to the four color components.
 * @returns The RGB565 value.
 */
uint16_t ditherPixel(const uint8_t *p);

#endif /* SIMD_H */
