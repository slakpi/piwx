/**
 * @file gfx.c
 */
#include "gfx.h"
#include "conf_file.h"
#include "util.h"
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const RGBA rgbNull  = {0.0f, 0.0f, 0.0f, 0.0f};
const RGBA rgbRed   = {1.0f, 0.0f, 0.0f, 1.0f};
const RGBA rgbWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const RGBA rgbBlack = {0.0f, 0.0f, 0.0f, 1.0f};

/**
 * @struct  Rectangle
 * @brief   Rectangle primitive.
 * @details The rectangle excludes the right and bottom such that valid
 *          coordinates are: x ~ [0, r) and y ~ [0, b).
 */
typedef struct {
  int l, t, r, b;
} Rectangle;

/**
 * @struct Bitmap_
 * @brief  Private implementation of Bitmap
 */
typedef struct {
  Rectangle dim;  // Dimensions of the PNG.
  RGBA **   rows; // Row markers.
} Bitmap_;

/**
 * @typedef Surface_
 * @brief   Private implementation of Surface.
 */
typedef Bitmap_ Surface_;

/**
 * @struct Compose
 * @brief  Configuration of a compose operation.
 */
typedef struct {
  Bitmap_ * from;   // Source bitmap.
  Rectangle source; // Source rectangle.
  Surface_ *to;     // Target surface.
  int       x, y;   // Top-left corner of output.
  RGBA      color;  // Foreground color.
  RGBA      bkgnd;  // Background color.
} Compose;

/**
 * @brief Private implementation of @a freeBitmap.
 * @param[in] bmp The bitmap to free.
 */
static void freeBitmap_(Bitmap_ *bmp) {
  if (!bmp) {
    return;
  }

  if (bmp->rows) {
    free(bmp->rows[0]);
  }

  free(bmp->rows);
  free(bmp);
}

/**
 * @brief   Alpha blend two colors.
 * @details Blends the foreground color @a fg with background color @a bg.
 *
 *          For Alpha:          Ao = Af + Ab * (1 - Af)
 *          For RGB components: [ Co = Cf * Af + Cb * Ab * (1 - Af) ] / Ao
 *
 *          If the alpha component of the background is sufficiently close to
 *          1.0, blend the foreground onto the background with the foreground's
 *          alpha value and set the final output alpha to 1.0.
 *
 *          If the composite alpha component is sufficiently close to 0.0, just
 *          output clear black.
 * @param[in]  fg  The foreground color.
 * @param[in]  bg  The background color.
 * @param[out] out The final composite color. It is safe to reuse either the
 *                 foreground or background pointer for output.
 */
static void mix(const RGBA *fg, const RGBA *bg, RGBA *out) {
  RGBA  tmp;
  float ba, ao;

  if (!(fg && bg && out)) {
    return;
  }

  // Shortcut the blend if the background color's alpha is sufficiently large.
  // There is no need to calculate a composite alpha.
  if (ALPHA(*bg) > 1.0f - FLT_EPSILON) {
    tmp        = (*fg) * ALPHA(*fg) + (*bg) * (1.0f - ALPHA(*fg));
    ALPHA(tmp) = 1.0f;

    *out = tmp;

    return;
  }

  // Precalculate Ab * (1.0 - Af) and the final output alpha.
  ba = ALPHA(*bg) * (1.0f - ALPHA(*fg));
  ao = ALPHA(*fg) + ba;

  // Shortcut the blend if the composite alpha is sufficiently small. Just set
  // the final color to clear black.
  if (ao < FLT_EPSILON) {
    *out = rgbNull;
    return;
  }

  // Blend the RGB components.
  tmp = (*fg) * ALPHA(*fg) + (*bg) * ba;
  tmp /= ao;
  ALPHA(tmp) = ao;

  *out = tmp;
}

/**
 * @brief   Finds the intersection of two rectangles.
 * @param[in] a      First rectangle.
 * @param[in] b      Second rectangle.
 * @param[out] isect Intersected rectangle. It is safe for this to be a pointer
 *                   to @a a or @a b.
 * @returns TRUE if the intersection is non-empty, FALSE otherwise.
 */
static boolean rectangleIntersection(const Rectangle *a, const Rectangle *b,
                                     Rectangle *isect) {
  Rectangle tmp;

  tmp.l = max(a->l, b->l);
  tmp.t = max(a->t, b->t);
  tmp.r = min(a->r, b->r);
  tmp.b = min(a->b, b->b);

  *isect = tmp;

  if (!(tmp.l < tmp.r && tmp.t < tmp.b)) {
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief   Perform a compose operation.
 * @details Blends the source rectangle from a bitmap into the target rectangle
 *          of a surface.
 *
 *          @a compose multiplies the source pixels with the foreground color,
 *          mixes the result with the background color, then mixes with the
 *          pixel on the target surface.
 *
 *          To perform a simple copy, set the foreground to opaque white and the
 *          background color to transparent black.
 * @param[in] cmp The compose operation details.
 */
static void compose(const Compose *cmp) {
  int       u, v, x, y;
  Rectangle source, target;
  RGBA      tmp;

  if (!(cmp && cmp->to && cmp->from)) {
    return;
  }

  // Initialize the rectangle to the same dimensions as the source, position it
  // at the target location, and intersect it with the surface bounds.
  target.l = cmp->x;
  target.t = cmp->y;
  target.r = target.l + (cmp->source.r - cmp->source.l);
  target.b = target.t + (cmp->source.b - cmp->source.t);

  if (!rectangleIntersection(&cmp->to->dim, &target, &target)) {
    return;
  }

  // Adjust the source rectangle. We know the rectangle above will be the same
  // size or smaller than the source rectangle and can only move in the +x and
  // +y directions.
  source   = cmp->source;
  source.l += (target.l - cmp->x);
  source.t += (target.t - cmp->y);
  source.r = source.l + target.r - target.l;
  source.b = source.t + target.b - target.t;

  // Intersect the new source rectangle with the bitmap bounds.
  if (!rectangleIntersection(&cmp->from->dim, &cmp->source, &source)) {
    return;
  }

  for (v = source.t, y = target.t; y < target.b; ++y, ++v) {
    for (u = source.l, x = target.l; x < target.r; ++x, ++u) {
      // Multiply by the foreground color, then mix with the background color.
      tmp = cmp->from->rows[v][u];
      tmp *= cmp->color;
      mix(&tmp, &cmp->bkgnd, &tmp);

      if (ALPHA(tmp) > 1.0f - FLT_EPSILON) {
        // Shortcut the mix if the alpha is sufficiently large.
        cmp->to->rows[y][x] = tmp;
      } else {
        // Mix with the surface color. The mix will shortcut if the alpha is
        // sufficiently small.
        mix(&tmp, &cmp->to->rows[y][x], &cmp->to->rows[y][x]);
      }
    }
  }
}

static Bitmap_ *allocateEmptyBitmap(int w, int h) {
  Bitmap_ *bmp;
  RGBA *   p;
  int      y;

  if (!(w > 0 && h > 0)) {
    return NULL;
  }

  bmp = malloc(sizeof(Bitmap_));

  if (!bmp) {
    return NULL;
  }

  bmp->dim.l = 0;
  bmp->dim.t = 0;
  bmp->dim.r = w;
  bmp->dim.b = h;
  bmp->rows  = malloc(sizeof(RGBA *) * h);

  if (!bmp->rows) {
    goto cleanup;
  }

  bmp->rows[0] = aligned_alloc(sizeof(RGBA), sizeof(RGBA) * w * h);

  if (!bmp->rows[0]) {
    goto cleanup;
  }

  p = bmp->rows[0];

  for (y = 1; y < h; ++y) {
    p += w;
    bmp->rows[y] = p;
  }

  clearSurface(bmp);

  return bmp;

cleanup:
  freeBitmap_(bmp);

  return NULL;
}

Surface allocateSurface(int w, int h) { return allocateEmptyBitmap(w, h); }

void freeSurface(Surface surface) { freeBitmap_((Bitmap_ *)surface); }

void clearSurface(Surface surface) {
  Surface_ *sfc = (Surface_ *)surface;

  if (!(sfc && sfc->rows)) {
    return;
  }

  memset(sfc->rows[0], 0, sizeof(RGBA) * sfc->dim.r * sfc->dim.b);
}

/**
 * @brief Convert a drawing surface to a RGB565 bitmap.
 * @param[in]  sfc    The drawing surface.
 * @param[out] bitmap The output buffer for the 16-bit RGB565 data.
 */
void ditherSurface(const Surface_ *sfc, u_int16_t *bitmap) {
  u_int16_t *q;
  RGBA *     p, tmp;
  size_t     i, px;

  if (!sfc) {
    return;
  }

  px = sfc->dim.r * sfc->dim.b;
  p  = sfc->rows[0];
  q  = bitmap;

  // Convert the 32-bit pixels to 16-bit RGB565. Pre-multiply with the alpha
  // value, then convert the components.
  for (i = 0; i < px; ++i) {
    tmp = *p * ALPHA(*p);

    *q = ((u_int16_t)(RED(tmp) * 0x1f) << 11) |
         ((u_int16_t)(GREEN(tmp) * 0x3f) << 5) | (u_int16_t)(BLUE(tmp) * 0x1f);

    ++p;
    ++q;
  }
}

boolean writeSurfaceToPNG(const Surface surface, const char *file) {
  Surface_ *  sfc     = (Surface_ *)surface;
  FILE *      png     = NULL;
  png_structp pngPtr  = NULL;
  png_infop   pngInfo = NULL;
  size_t      rowBytes;
  png_bytep * rows = NULL;
  png_byte *  t;
  RGBA *      s;
  int         x, y;
  boolean     ok = FALSE;

  if (!sfc) {
    return -1;
  }

  png = fopen(file, "wb");

  if (!png) {
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

  png_init_io(pngPtr, png);

  // Setup the ugly error handling for libpng. An error in the PNG functions
  // below will jump back here for error handling.
  if (setjmp(png_jmpbuf(pngPtr))) {
    goto cleanup;
  }

  // Setup the PNG image header with the properties of the image.
  png_set_IHDR(pngPtr, pngInfo, sfc->dim.r, sfc->dim.b, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(pngPtr, pngInfo);

  // Setup the row index pointers to the image data. The first element of the
  // array points to the beginning of the image. Each subsequent elements points
  // to the beginning of the next row which is `rowBytes` ahead of the current
  // row.
  rowBytes = png_get_rowbytes(pngPtr, pngInfo);
  rows     = malloc(sizeof(png_bytep) * sfc->dim.b);
  rows[0]  = malloc(rowBytes * sfc->dim.b);

  for (y = 1; y < sfc->dim.b; ++y) {
    rows[y] = rows[y - 1] + rowBytes;
  }

  s = sfc->rows[0];
  t = rows[0];

  for (y = 0; y < sfc->dim.b; ++y) {
    for (x = 0; x < sfc->dim.r; ++x) {
      t[0] = (png_byte)(RED(*s) * 255.0f);
      t[1] = (png_byte)(GREEN(*s) * 255.0f);
      t[2] = (png_byte)(BLUE(*s) * 255.0f);
      t[3] = (png_byte)(ALPHA(*s) * 255.0f);

      ++s;
      t += 4;
    }
  }

  // Write the image data and finalize the PNG file.
  png_write_image(pngPtr, rows);
  png_write_end(pngPtr, NULL);

  ok = TRUE;

cleanup:
  if (png) {
    fclose(png);
  }

  if (pngPtr) {
    png_destroy_write_struct(&pngPtr, &pngInfo);
  }

  if (rows) {
    free(rows[0]);
  }

  free(rows);

  return ok;
}

/**
 * @struct BITMAPFILEHEADER
 * @brief  Windows bitmap file header.
 */
typedef struct {
  u_int8_t  signature[2]; // Bitmap file signature.
  u_int32_t fileSize;     // Total bitmap file size.
  u_int16_t reserved1;    // Ignored.
  u_int16_t reserved2;    // Ignored.
  u_int32_t pixelOffset;  // Offset to pixel data.
} BITMAPFILEHEADER;

/**
 * @struct BITMAPINFOHEADER
 * @brief  Windows bitmap information header.
 */
typedef struct {
  u_int32_t size;            // Header size.
  u_int32_t width;           // Bitmap width in pixels.
  u_int32_t height;          // Bitmap height in pixels.
  u_int16_t planes;          // Color planes, must be 1.
  u_int16_t bpp;             // Bits-per-pixel.
  u_int32_t compression;     // Compression method.
  u_int32_t bmpSize;         // Raw bitmap size, 0 for BI_RGB.
  u_int32_t hRes;            // Horizontal resolution px per meter.
  u_int32_t vRes;            // Vertical resolution px per meter.
  u_int32_t paletteSize;     // Colors in palette.
  u_int32_t importantColors; // Important colors in palette, 0.
} BITMAPINFOHEADER;

boolean writeSurfaceToBMP(const Surface surface, const char *file) {
  Surface_ *       sfc = (Surface_ *)surface;
  FILE *           bmp = NULL;
  u_int16_t *      buf = NULL;
  BITMAPFILEHEADER hdr;
  BITMAPINFOHEADER info;
  size_t           px;
  boolean          ok = FALSE;

  if (!sfc) {
    return -1;
  }

  hdr.signature[0] = 0x42;
  hdr.signature[1] = 0x4d;
  hdr.fileSize     = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
                 sizeof(u_int16_t) * sfc->dim.r * sfc->dim.b;
  hdr.reserved1   = 0;
  hdr.reserved2   = 0;
  hdr.pixelOffset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

  info.size            = sizeof(hdr);
  info.width           = sfc->dim.r;
  info.height          = sfc->dim.b;
  info.planes          = 1;
  info.bpp             = 16;
  info.compression     = 0;
  info.bmpSize         = 0;
  info.hRes            = 0;
  info.vRes            = 0;
  info.paletteSize     = 0;
  info.importantColors = 0;

  bmp = fopen(file, "wb");

  if (bmp < 0) {
    goto cleanup;
  }

  // Allocate a new buffer for the RGB565 data.
  px  = sfc->dim.r * sfc->dim.b;
  buf = malloc(sizeof(u_int16_t) * px);

  if (!buf) {
    goto cleanup;
  }

  // Dither the surface for the TFT display.
  ditherSurface(sfc, buf);

  fwrite(&hdr, sizeof(hdr), 1, bmp);
  fwrite(&info, sizeof(info), 1, bmp);
  fwrite(buf, sizeof(u_int16_t), px, bmp);

  ok = TRUE;

cleanup:
  if (bmp) {
    fclose(bmp);
  }

  free(buf);

  return ok;
}

boolean commitSurface(const Surface surface) {
  const Surface_ *sfc = (const Surface_ *)surface;
  int             fb;
  u_int16_t *     buf = NULL;
  size_t          px;
  boolean         ok = FALSE;

  if (!sfc) {
    return -1;
  }

  fb = open("/dev/fb1", O_WRONLY);

  if (fb < 0) {
    goto cleanup;
  }

  // Allocate a new buffer for the RGB565 data.
  px  = sfc->dim.r * sfc->dim.b;
  buf = malloc(sizeof(u_int16_t) * px);

  if (!buf) {
    goto cleanup;
  }

  // Dither the surface for the TFT display.
  ditherSurface(sfc, buf);

  // Blt the 16-bit image to the framebuffer.
  write(fb, buf, sizeof(u_int16_t) * px);

  ok = TRUE;

cleanup:
  if (fb) {
    close(fb);
  }

  free(buf);

  return ok;
}

/**
 * @brief   Private implementation of @a allocateBitmap.
 * @details Loads the specified PNG file into a @a Bitmap_.
 * @param[in] pngFile     The path of the PNG file to load.
 * @param[in] colorFormat The desired color format.
 * @param[in] channelBits The desired bits per channel.
 * @returns The initialized @a Bitmap_ structure, or null if the PNG does not
 *          match the desired color format and bit depth or memory could not be
 *          allocated to hold the bitmap.
 */
static Bitmap_ *allocateBitmap_(const char *pngFile, int colorFormat,
                                int channelBits) {
  FILE *      png = NULL;
  png_byte    sig[8];
  png_structp pngPtr  = NULL;
  png_infop   pngInfo = NULL;
  png_bytep * rows    = NULL;
  png_bytep   ch;
  int         ok = 0, row, col, color, bits;
  size_t      rowBytes;
  Bitmap_ *   bmp = NULL;

  // Verify the designed format is supported.
  if (colorFormat != PNG_COLOR_TYPE_GRAY &&
      colorFormat != PNG_COLOR_TYPE_RGBA) {
    return NULL;
  }

  if (channelBits != 8) {
    return NULL;
  }

  // Verify this is actually a PNG file.
  png = fopen(pngFile, "r");

  if (!png) {
    return NULL;
  }

  fread(sig, 1, 8, png);

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

  png_init_io(pngPtr, png);
  png_set_sig_bytes(pngPtr, 8);
  png_read_info(pngPtr, pngInfo);

  // Initialize the bitmap structure with the PNG information.
  bmp = allocateEmptyBitmap(png_get_image_width(pngPtr, pngInfo),
                            png_get_image_height(pngPtr, pngInfo));

  if (!bmp) {
    goto cleanup;
  }

  color = png_get_color_type(pngPtr, pngInfo);
  bits  = png_get_bit_depth(pngPtr, pngInfo);

  png_set_interlace_handling(pngPtr);
  png_read_update_info(pngPtr, pngInfo);

  // Verify the image format matches the desired format.
  if (color != colorFormat || bits != channelBits) {
    goto cleanup;
  }

  // Attempt to allocate memory for the image.
  rowBytes = png_get_rowbytes(pngPtr, pngInfo);

  rows = malloc(sizeof(png_bytep) * bmp->dim.b);

  if (!rows) {
    goto cleanup;
  }

  rows[0] = malloc(rowBytes * bmp->dim.b);

  if (!rows[0]) {
    goto cleanup;
  }

  // Setup the row index pointers.
  for (row = 1; row < bmp->dim.b; ++row) {
    rows[row] = rows[row - 1] + rowBytes;
  }

  // Read the image.
  png_read_image(pngPtr, rows);

  // Convert the image to normalized RGBA. Grayscale images become the alpha
  // value for all white pixels to allow multiplication with a foreground color.
  if (color == PNG_COLOR_TYPE_GRAY) {
    ch = rows[0];

    for (row = 0; row < bmp->dim.b; ++row) {
      for (col = 0; col < bmp->dim.r; ++col) {
        RGBA *pixel   = &bmp->rows[row][col];
        RED(*pixel)   = 1.0f;
        GREEN(*pixel) = 1.0f;
        BLUE(*pixel)  = 1.0f;
        ALPHA(*pixel) = (*ch++) / 255.0f;
      }
    }
  } else if (color == PNG_COLOR_TYPE_RGBA) {
    ch = rows[0];

    for (row = 0; row < bmp->dim.b; ++row) {
      for (col = 0; col < bmp->dim.r; ++col) {
        RGBA *pixel   = &bmp->rows[row][col];
        RED(*pixel)   = *ch++;
        GREEN(*pixel) = *ch++;
        BLUE(*pixel)  = *ch++;
        ALPHA(*pixel) = *ch++;
        *pixel /= 255.0f;
      }
    }
  }

  ok = 1;

cleanup:
  if (png) {
    fclose(png);
  }

  if (pngPtr) {
    png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
  }

  if (rows) {
    free(rows[0]);
  }

  free(rows);

  if (!ok) {
    freeBitmap_(bmp);
    bmp = NULL;
  }

  return bmp;
}

Bitmap allocateBitmap(const char *pngFile) {
  char path[4096];
  return (Bitmap)allocateBitmap_(getPathForBitmap(pngFile, path, 4096),
                                 PNG_COLOR_TYPE_RGBA, 8);
}

void freeBitmap(Bitmap bmp) { freeBitmap_((Bitmap_ *)bmp); }

void drawBitmap(Surface surface, const Bitmap bitmap, int x, int y) {
  Surface_ *sfc = (Surface_ *)surface;
  Bitmap_ * bmp = (Bitmap_ *)bitmap;
  Compose   cmp;

  if (!(sfc && bmp)) {
    return;
  }

  cmp.from     = bmp;
  cmp.source.l = 0;
  cmp.source.t = 0;
  cmp.source.r = bmp->dim.r;
  cmp.source.b = bmp->dim.b;

  cmp.to = sfc;
  cmp.x  = x;
  cmp.y  = y;

  cmp.color = rgbWhite;
  cmp.bkgnd = rgbNull;

  compose(&cmp);
}

void drawBitmapInBox(Surface surface, const Bitmap bitmap, int l, int t, int r,
                     int b) {
  Bitmap_ *bmp = (Bitmap_ *)bitmap;

  if (!bmp) {
    return;
  }

  drawBitmap(surface, bitmap, l + ((r - l) - bmp->dim.r) / 2,
             t + ((b - t) - bmp->dim.b) / 2);
}

/**
 * @struct Font_
 * @brief  Private implementation of Font.
 */
typedef struct {
  Bitmap_ *bmp;
  int      cw, ch;
  RGBA     color, bkgnd;
} Font_;

Font allocateFont(FontSize size) {
  char        path[4096];
  const char *f;
  Font_ *     fnt;

  switch (size) {
  case font_16pt:
    f = "sfmono16.png";
    break;
  case font_10pt:
    f = "sfmono10.png";
    break;
  case font_8pt:
    f = "sfmono8.png";
    break;
  case font_6pt:
    f = "sfmono6.png";
    break;
  default:
    return NULL;
  }

  fnt = malloc(sizeof(Font_));

  if (!fnt) {
    return NULL;
  }

  fnt->bmp =
      allocateBitmap_(getPathForFont(f, path, 4096), PNG_COLOR_TYPE_GRAY, 8);

  // The font character map must be 16 characters by 8 characters.
  if (!fnt->bmp || fnt->bmp->dim.r % 16 != 0 || fnt->bmp->dim.b % 8 != 0) {
    free(fnt);
    return NULL;
  }

  fnt->cw    = fnt->bmp->dim.r / 16;
  fnt->ch    = fnt->bmp->dim.b / 8;
  fnt->color = rgbNull;
  fnt->bkgnd = rgbNull;

  return (Font)fnt;
}

void freeFont(Font font) {
  Font_ *fnt = (Font_ *)font;

  if (!fnt) {
    return;
  }

  freeBitmap(fnt->bmp);
  free(fnt);
}

int getFontCharWidth(Font font) {
  Font_ *fnt = (Font_ *)font;

  if (!fnt) {
    return 0;
  }

  return fnt->cw;
}

int getFontCharHeight(Font font) {
  Font_ *fnt = (Font_ *)font;

  if (!fnt) {
    return 0;
  }

  return fnt->ch;
}

void setTextColor(Font font, const RGBA *color) {
  Font_ *fnt = (Font_ *)font;

  if (!fnt) {
    return;
  }

  fnt->color = *color;
}

void setBackgroundColor(Font font, const RGBA *bkgnd) {
  Font_ *fnt = (Font_ *)font;

  if (!fnt) {
    return;
  }

  fnt->bkgnd = *bkgnd;
}

void drawText(Surface surface, const Font font, int x, int y, const char *str,
              size_t len) {
  Surface_ *   sfc = (Surface_ *)surface;
  const Font_ *fnt = (const Font_ *)font;
  Compose      cmp;
  char         c;
  size_t       i;

  if (!(sfc && fnt)) {
    return;
  }

  if (!(fnt->cw <= sfc->dim.r && fnt->ch <= sfc->dim.b)) {
    return;
  }

  // Setup the common compose paramters.
  cmp.from  = fnt->bmp;
  cmp.color = fnt->color;
  cmp.bkgnd = fnt->bkgnd;

  cmp.to = sfc;
  cmp.x  = x;
  cmp.y  = y;

  for (i = 0; i < len; ++i) {
    c = str[i];

    // Null character if in the high 7-bits.
    if (c & 0x80) {
      c = 0;
    }

    // Attempt to wrap the text string if exceeding the surface width.
    if (cmp.x + fnt->cw >= sfc->dim.r) {
      cmp.x = x;
      cmp.y += fnt->ch;
    }

    // If exceeding the surface height, there is nothing left to do.
    if (cmp.y >= sfc->dim.b) {
      return;
    }

    // Compose the character on to the surface.
    cmp.source.l = (c % 16) * fnt->cw;
    cmp.source.t = (c / 16) * fnt->ch;
    cmp.source.r = cmp.source.l + fnt->cw;
    cmp.source.b = cmp.source.t + fnt->ch;

    compose(&cmp);

    cmp.x += fnt->cw;
  }
}
