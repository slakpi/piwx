/**
 * @file simd.c
 */
#if defined __ARM_NEON
#include <arm_neon.h>
#endif
#include <stdint.h>

/**
 * @brief   Convert a RGBA8888 pixel to RGB565 with premultiplied alpha.
 * @details Naively, premultiplying the alpha would require going into floating-
 *          point land with a division:
 *
 *               A
 *          C * --- = CA       Where C is Red, Green, or Blue
 *              255
 *
 *          If 256 is used as the denominator instead, the divison can be
 *          replaced by shifts:
 *
 *          C'  = C << 8       Multiply the color component by 256
 *          CA' = C' * A       Multiply by the integer alpha
 *          CA  = CA' >> 16    Divide by 65,536. In terms of bits:
 *                             .n x 2^16 * .m x 2^8 = (.n x .m) x 2^24
 *                             Dividing by 2^16 discards the "fractional" bits
 *                             leaving the most significant 8-bits.
 *
 *          Using this method, a color value of 255 with an alpha of 255 equates
 *          to a final value of 254. The alternative is adding 1 to the alpha
 *          An alpha of 0 would still result in a color value of 0, and an alpha
 *          of 255 would keep the same color value. Color values would just be
 *          biased up 1-bit in some cases.
 *
 *          Loading values into NEON vectors is a little unintuitive since it
 *          requires shuffing the values around in registers. NEON does not have
 *          a uint8x4_t, so the four color values have to be combined into a
 *          single lane of a uint32x2_t. They can then be split into a
 *          uint8x8_t, promoted to a uint16x8_t, then split again and promoted
 *          to a uint32x4_t.
 *
 *          In experiments, Clang 14 inlines this function when used in a tight
 *          loop.
 * @param[in] p Pointer to the four color components.
 * @returns The RGB565 value.
 */
#if defined __ARM_NEON
uint16_t ditherPixel(const uint8_t *p) {
  uint32x2_t z    = {0, 0};
  // [ r8, g8, b8, a8 ] => { rgba32, 0 }
  uint32x2_t t32v = vld1_lane_u32((uint32_t *)p, z, 0);
  // { rgba32, 0 } => { r8, g8, b8, a8, 0, 0, 0, 0 }
  uint8x8_t  t8v  = vreinterpret_u8_u32(t32v);
  // { r8, g8, b8, a8, 0, 0, 0, 0 } => { r16, g16, b16, a16, 0, 0, 0, 0 }
  uint16x8_t t16v = vmovl_u8(t8v);
  // { r16, g16, b16, a16, 0, 0, 0, 0 } => { r32, g32, b32, a32 }
  uint32x4_t cv   = vmovl_u16(vget_low_u16(t16v));
  // { a32, a32, a32, a32 }
  uint32x4_t av   = vdupq_n_u32((uint32_t)cv[3] + 1);

  cv = vshlq_n_u32(cv, 8);
  cv = cv * av;
  cv = vshrq_n_u32(cv, 16);

  return (uint16_t)(((cv[0] >> 3) & 0x1f) << 11) | (uint16_t)(((cv[1] >> 2) & 0x3f) << 5) |
         (uint16_t)((cv[2] >> 3) & 0x1f);
}
#endif
