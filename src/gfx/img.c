/**
 * @file img.c
 */
#include "img.h"
#include <png.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static size_t calcRowBytes(png_byte bits, png_byte color, png_uint_32 width);

static bool validateBits(png_byte bits);

static bool validateColor(png_byte color);

bool allocPng(Png *png, png_byte bits, png_byte color, png_uint_32 width, png_uint_32 height,
              size_t align) {
  size_t rowBytes;

  if (!png) {
    return false;
  }

  freePng(png);

  if (!validateBits(bits) || !validateColor(color)) {
    return false;
  }

  if (width < 1 || height < 1) {
    return false;
  }

  png->bits   = bits;
  png->color  = color;
  png->width  = width;
  png->height = height;

  // Attempt to allocate memory for the image.
  rowBytes  = calcRowBytes(bits, color, width);
  png->rows = malloc(sizeof(png_bytep) * png->height);

  if (!png->rows) {
    goto cleanup;
  }

  png->rows[0] = aligned_alloc(align, rowBytes * png->height);

  if (!png->rows[0]) {
    goto cleanup;
  }

  // Setup the row index pointers.
  for (png_uint_32 row = 1; row < png->height; ++row) {
    png->rows[row] = png->rows[row - 1] + rowBytes;
  }

#if defined _DEBUG
  memset(png->rows[0], 255, rowBytes * png->height); // NOLINT -- Size known.
#endif

  return true;

cleanup:
  freePng(png);

  return false;
}

void freePng(Png *png) {
  if (!png) {
    return;
  }

  if (png->rows) {
    free(png->rows[0]);
  }

  free(png->rows);

  memset(png, 0, sizeof(*png)); // NOLINT -- Size known.
}

bool loadPng(Png *png, const char *path) {
  FILE       *pngFile = NULL;
  png_byte    sig[8]  = {0};
  png_byte    bits    = 0;
  png_byte    color   = 0;
  png_structp pngPtr  = NULL;
  png_infop   pngInfo = NULL;
  png_uint_32 width = 0, height = 0;
  bool        ok = false;

  if (!png || !path) {
    return false;
  }

  memset(png, 0, sizeof(*png)); // NOLINT -- Size known.

  pngFile = fopen(path, "r");

  if (!pngFile) {
    return false;
  }

  fread(sig, 1, 8, pngFile);

  // Verify this is actually a PNG file.
  if (png_sig_cmp(sig, 0, 8)) {
    goto cleanup;
  }

  pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!pngPtr) {
    goto cleanup;
  }

  pngInfo = png_create_info_struct(pngPtr);

  if (!pngInfo) {
    goto cleanup;
  }

  // Setup the ugly error handling for libpng. An error in the PNG functions
  // below will jump back here for error handling.
  if (setjmp(png_jmpbuf(pngPtr))) {
    goto cleanup;
  }

  png_init_io(pngPtr, pngFile);
  png_set_sig_bytes(pngPtr, 8);
  png_read_info(pngPtr, pngInfo);

  // Initialize the bitmap structure with the PNG information. Accept only 8-bit
  // color channels.
  width  = png_get_image_width(pngPtr, pngInfo);
  height = png_get_image_height(pngPtr, pngInfo);
  color  = png_get_color_type(pngPtr, pngInfo);
  bits   = png_get_bit_depth(pngPtr, pngInfo);

  if (!allocPng(png, bits, color, width, height, 4)) {
    goto cleanup;
  }

  png_set_interlace_handling(pngPtr);
  png_read_update_info(pngPtr, pngInfo);
  png_read_image(pngPtr, png->rows);

  ok = true;

cleanup:
  if (pngFile) {
    fclose(pngFile);
  }

  if (pngPtr) {
    png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
  }

  if (!ok) {
    freePng(png);
  }

  return ok;
}

bool writePng(const Png *png, const char *path) {
  FILE       *pngFile = NULL;
  png_structp pngPtr  = NULL;
  png_infop   pngInfo = NULL;
  bool        ok      = false;

  if (!png) {
    return false;
  }

  pngFile = fopen(path, "wb");

  if (!pngFile) {
    goto cleanup;
  }

  pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!pngPtr) {
    goto cleanup;
  }

  pngInfo = png_create_info_struct(pngPtr);

  if (!pngInfo) {
    goto cleanup;
  }

  png_init_io(pngPtr, pngFile);

  // Setup the ugly error handling for libpng. An error in the PNG functions
  // below will jump back here for error handling.
  if (setjmp(png_jmpbuf(pngPtr))) {
    goto cleanup;
  }

  // Setup the PNG image header with the properties of the image and write it.
  png_set_IHDR(pngPtr, pngInfo, png->width, png->height, png->bits, png->color, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(pngPtr, pngInfo);

  // Write the image data and finalize the PNG file.
  png_write_image(pngPtr, png->rows);
  png_write_end(pngPtr, NULL);

  ok = true;

cleanup:
  if (pngFile) {
    fclose(pngFile);
  }

  if (pngPtr) {
    png_destroy_write_struct(&pngPtr, &pngInfo);
  }

  return ok;
}

/**
 * @brief Validates that the bit depth is 8- or 16-bits.
 * @param[in] bits Bit depth to test.
 */
static bool validateBits(png_byte bits) { return (bits == 8 || bits == 16); }

/**
 * @brief Validates that the color format is RGB, RGBA or grayscale.
 * @param[in] color PNG color format.
 */
static bool validateColor(png_byte color) {
  return (color == PNG_COLOR_TYPE_RGB || color == PNG_COLOR_TYPE_RGBA ||
          color == PNG_COLOR_TYPE_GRAY);
}

/**
 * @brief   Calculates the number of bytes per row given the image
 *          specifications.
 * @param[in] bits  Bit depth
 * @param[in] color PNG color format
 * @param[in] width Number of pixels in a row.
 * @returns The number of bytes in a single row or zero if the image
 *          specifications are invalid.
 */
static size_t calcRowBytes(png_byte bits, png_byte color, png_uint_32 width) {
  switch (color) {
  case PNG_COLOR_TYPE_RGB:
    return (bits * width * 3) >> 3;
  case PNG_COLOR_TYPE_RGBA:
    return (bits * width * 4) >> 3;
  case PNG_COLOR_TYPE_GRAY:
    return (bits * width) >> 3;
  default:
    return 0;
  }
}
