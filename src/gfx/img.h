/**
 * @file img.h
 */
#if !defined IMG_H
#define IMG_H

#include <png.h>
#include <stdbool.h>

/**
 * @struct Png
 * @brief  Represents the pixels of a PNG file.
 */
typedef struct {
  png_byte    bits;          // Bits per pixel
  png_byte    color;         // PNG color format
  png_uint_32 width, height; // Dimensions of the image
  png_byte  **rows;          // Row pointers
} Png;

/**
 * @brief   Allocates a PNG object.
 * @details @a allocPng only supports 8-bit or 16-bit pixel depths, and only
 *          RGBA or grayscale color formats.
 * @param[out] png   The PNG object to initialize.
 * @param[in] bits   The number of bits per pixel.
 * @param[in] color  The PNG color format.
 * @param[in] width  The width of the image.
 * @param[in] height The height of the image.
 * @param[in] align  The alignment for the image memory.
 * @returns True if the bit depth, color, and dimensions are valid, false
 *          otherwise.
 */
bool allocPng(Png *png, png_byte bits, png_byte color, png_uint_32 width, png_uint_32 height,
              size_t align);

/**
 * @brief Frees an PNG object.
 * @param[in,out] png The PNG object to free.
 */
void freePng(Png *png);

/**
 * @brief   Load a PNG image from a file.
 * @details @a loadPng only supports 8-bit or 16-bit pixel depths, and only RGBA
 *          or grayscale color formats.
 * @param[out] png The PNG object to initialize.
 * @param[in] path The path to the PNG file.
 * @returns True if the load is successful, false otherwise.
 */
bool loadPng(Png *png, const char *path);

/**
 * @brief   Write a PNG image to a file.
 * @param[in] png   The PNG object to write out.
 * @param[out] path The path to the PNG file.
 * @returns True if the write is successful, false otherwise.
 */
bool writePng(const Png *png, const char *path);

#endif /* IMG_H */
