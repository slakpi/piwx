/**
 * @file gfx.c
 */
#include "conf_file.h"
#include "gfx.h"
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
 * @struct _Bitmap
 * @brief  Private implementation of Bitmap
 */
typedef struct {
  Rectangle dim; /* Dimensions of the PNG. */
  RGBA **rows;   /* Row markers. */
} _Bitmap;

/**
 * @typedef _Surface
 * @brief   Private implementation of Surface.
 */
typedef _Bitmap _Surface;

/**
 * @struct Compose
 * @brief  Configuration of a compose operation.
 */
typedef struct {
  _Bitmap *from;    /* Source bitmap. */
  Rectangle source; /* Source rectangle. */
  _Surface *to;     /* Target surface. */
  int x, y;         /* Top-left corner of output. */
  RGBA color;       /* Foreground color. */
  RGBA bkgnd;       /* Background color. */
} Compose;

/**
 * @brief Private implementation of @a freeBitmap.
 * @param[in] _bmp The bitmap to free.
 */
static void _freeBitmap(_Bitmap *_bmp) {
  if (!_bmp) {
    return;
  }

  if (_bmp->rows) {
    if (_bmp->rows[0]) {
      free(_bmp->rows[0]);
    }

    free(_bmp->rows);
  }

  free(_bmp);
}

/**
 * @brief   Alpha blend two colors.
 * @details Blends the foreground color @a _fg with background color @a _bg.
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
 * @param[in]  _fg  The foreground color.
 * @param[in]  _bg  The background color.
 * @param[out] _out The final composite color. It is safe to reuse either the
 *                  foreground or background pointer for output.
 */
static void mix(const RGBA *_fg, const RGBA *_bg, RGBA *_out) {
  RGBA tmp;
  float ba, ao;

  if (!_fg || !_bg || !_out) {
    return;
  }

  // Shortcut the blend if the background color's alpha is sufficiently large.
  // There is no need to calculate a composite alpha.
  if (ALPHA(*_bg) > 1.0f - FLT_EPSILON) {
    tmp = (*_fg) * ALPHA(*_fg) + (*_bg) * (1.0f - ALPHA(*_fg));
    ALPHA(tmp) = 1.0f;

    *_out = tmp;

    return;
  }

  // Precalculate Ab * (1.0 - Af) and the final output alpha.
  ba = ALPHA(*_bg) * (1.0f - ALPHA(*_fg));
  ao = ALPHA(*_fg) + ba;

  // Shortcut the blend if the composite alpha is sufficiently small. Just set
  // the final color to clear black.
  if (ao < FLT_EPSILON) {
    *_out = rgbNull;
    return;
  }

  // Blend the RGB components.
  tmp = (*_fg) * ALPHA(*_fg) + (*_bg) * ba;
  tmp /= ao;
  ALPHA(tmp) = ao;

  *_out = tmp;
}

/**
 * @brief   Finds the intersection of two rectangles.
 * @param[in] _a      First rectangle.
 * @param[in] _b      Second rectangle.
 * @param[out] _isect Intersected rectangle. It is safe for this to be a pointer
 *                    to @a _a or @a _b.
 * @returns 0 if the intersection is non-empty.
 */
static int rectangleIntersection(const Rectangle *_a, const Rectangle *_b,
                                 Rectangle *_isect) {
  Rectangle tmp;

  tmp.l = max(_a->l, _b->l);
  tmp.t = max(_a->t, _b->t);
  tmp.r = min(_a->r, _b->r);
  tmp.b = min(_a->b, _b->b);

  *_isect = tmp;

  if (tmp.l >= tmp.r || tmp.t >= tmp.b) {
    return -1;
  }

  return 0;
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
 * @param[in] _cmp The compose operation details.
 */
static void compose(const Compose *_cmp) {
  int u, v, x, y;
  Rectangle source, target;
  RGBA tmp;

  if (!_cmp || !_cmp->to || !_cmp->from) {
    return;
  }

  // Initialize the rectangle to the same dimensions as the source, position it
  // at the target location, and intersect it with the surface bounds.
  target.l = _cmp->x;
  target.t = _cmp->y;
  target.r = target.l + (_cmp->source.r - _cmp->source.l);
  target.b = target.t + (_cmp->source.b - _cmp->source.t);

  if (rectangleIntersection(&_cmp->to->dim, &target, &target) != 0) {
    return;
  }

  // Adjust the source rectangle. We know the rectangle above will be the same
  // size or smaller than the source rectangle and can only move in the +x and
  // +y directions.
  source.l += (target.l - _cmp->x);
  source.t += (target.t - _cmp->y);
  source.r = source.l + target.r - target.l;
  source.b = source.t + target.b - target.t;

  // Intersect the new source rectangle with the bitmap bounds.
  if (rectangleIntersection(&_cmp->from->dim, &_cmp->source, &source) != 0) {
    return;
  }

  for (v = source.t, y = target.t; y < target.b; ++y, ++v) {
    for (u = source.l, x = target.l; x < target.r; ++x, ++u) {
      // Multiply by the foreground color, then mix with the background color.
      tmp = _cmp->from->rows[v][u];
      tmp *= _cmp->color;
      mix(&tmp, &_cmp->bkgnd, &tmp);

      if (ALPHA(tmp) > 1.0f - FLT_EPSILON) {
        // Shortcut the mix if the alpha is sufficiently large.
        _cmp->to->rows[y][x] = tmp;
      } else {
        // Mix with the surface color. The mix will shortcut if the alpha is
        // sufficiently small.
        mix(&tmp, &_cmp->to->rows[y][x], &_cmp->to->rows[y][x]);
      }
    }
  }
}

static _Bitmap *allocateEmptyBitmap(int _w, int _h) {
  _Bitmap *bmp;
  RGBA *p;
  int y;

  if (_w <= 0 || _h <= 0) {
    return NULL;
  }
  
  bmp = (_Bitmap *)malloc(sizeof(_Bitmap));
  
  if (!bmp) {
    return NULL;
  }

  bmp->dim.l = 0;
  bmp->dim.t = 0;
  bmp->dim.r = _w;
  bmp->dim.b = _h;
  bmp->rows = (RGBA **)malloc(sizeof(RGBA *) * _h);

  if (!bmp->rows) {
    goto cleanup;
  }

  bmp->rows[0] = (RGBA *)aligned_alloc(sizeof(RGBA), sizeof(RGBA) * _w * _h);

  if (!bmp->rows[0]) {
    goto cleanup;
  }

  p = bmp->rows[0];

  for (y = 1; y < _h; ++y) {
    p += _w;
    bmp->rows[y] = p;
  }

  clearSurface(bmp);

  return bmp;

cleanup:
  _freeBitmap(bmp);

  return NULL;
}

Surface allocateSurface(int _w, int _h) {
  return allocateEmptyBitmap(_w, _h);
}

void freeSurface(Surface _surface) {
  _freeBitmap((_Bitmap *)_surface);
}

void clearSurface(Surface _surface) {
  _Surface *sfc = (_Surface *)_surface;

  if (!sfc || !sfc->rows) {
    return;
  }

  memset(sfc->rows[0], 0, sizeof(RGBA) * sfc->dim.r * sfc->dim.b);
}

/**
 * @brief Convert a drawing surface to a RGB565 bitmap.
 * @param[in]  _sfc    The drawing surface.
 * @param[out] _bitmap The output buffer for the 16-bit RGB565 data.
 */
void ditherSurface(const _Surface *_sfc, u_int16_t *_bitmap) {
  u_int16_t *q;
  RGBA *p, tmp;
  size_t i, px;

  if (!_sfc) {
    return;
  }

  px = _sfc->dim.r * _sfc->dim.b;
  p = _sfc->rows[0];
  q = _bitmap;

  // Convert the 32-bit pixels to 16-bit RGB565. Pre-multiply with the alpha
  // value, then convert the components.
  for (i = 0; i < px; ++i) {
    tmp = *p * ALPHA(*p);

    *q = ((u_int16_t)(RED(tmp)   * 0x1f) << 11) |
         ((u_int16_t)(GREEN(tmp) * 0x3f) << 5)  |
          (u_int16_t)(BLUE(tmp)  * 0x1f);

    ++p;
    ++q;
  }
}

int writeSurfaceToPNG(const Surface _surface, const char *_file) {
  _Surface *sfc = (_Surface *)_surface;
  FILE *png = NULL;
  png_structp pngPtr = NULL;
  png_infop pngInfo = NULL;
  size_t rowBytes;
  png_bytep *rows = NULL;
  png_byte *t;
  RGBA *s;
  int ok = -1, x, y;

  if (!sfc) {
    return -1;
  }

  png = fopen(_file, "wb");

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
  rows = (png_bytep *)malloc(sizeof(png_bytep) * sfc->dim.b);
  rows[0] = (png_byte *)malloc(rowBytes * sfc->dim.b);

  for (y = 1; y < sfc->dim.b; ++y) {
    rows[y] = rows[y - 1] + rowBytes;
  }

  s = sfc->rows[0];
  t = rows[0];

  for (y = 0; y < sfc->dim.b; ++y) {
    for (x = 0; x < sfc->dim.r; ++x) {
      t[0] = (png_byte)(RED(*s)   * 255.0f);
      t[1] = (png_byte)(GREEN(*s) * 255.0f);
      t[2] = (png_byte)(BLUE(*s)  * 255.0f);
      t[3] = (png_byte)(ALPHA(*s) * 255.0f);

      ++s;
      t += 4;
    }
  }

  // Write the image data and finalize the PNG file.
  png_write_image(pngPtr, rows);
  png_write_end(pngPtr, NULL);

  ok = 0;

cleanup:
  if (png) {
    fclose(png);
  }

  if (pngPtr) {
    png_destroy_write_struct(&pngPtr, &pngInfo);
  }

  if (rows) {
    if (rows[0]) {
      free(rows[0]);
    }

    free(rows);
  }

  return ok;
}

/**
 * @struct _BITMAPFILEHEADER
 * @brief  Windows bitmap file header.
 */
typedef struct {
  u_int8_t signature[2]; /* Bitmap file signature. */
  u_int32_t fileSize;    /* Total bitmap file size. */
  u_int16_t reserved1;   /* Ignored. */
  u_int16_t reserved2;   /* Ignored. */
  u_int32_t pixelOffset; /* Offset to pixel data. */
} _BITMAPFILEHEADER;

/**
 * @struct _BITMAPINFOHEADER
 * @brief  Windows bitmap information header.
 */
typedef struct {
  u_int32_t size;            /* Header size. */
  u_int32_t width;           /* Bitmap width in pixels. */
  u_int32_t height;          /* Bitmap height in pixels. */
  u_int16_t planes;          /* Color planes, must be 1. */
  u_int16_t bpp;             /* Bits-per-pixel. */
  u_int32_t compression;     /* Compression method. */
  u_int32_t bmpSize;         /* Raw bitmap size, 0 for BI_RGB. */
  u_int32_t hRes;            /* Horizontal resolution px per meter. */
  u_int32_t vRes;            /* Vertical resolution px per meter. */
  u_int32_t paletteSize;     /* Colors in palette. */
  u_int32_t importantColors; /* Important colors in palette, 0. */
} _BITMAPINFOHEADER;

int writeSurfaceToBMP(const Surface _surface, const char *_file) {
  _Surface *sfc = (_Surface *)_surface;
  FILE *bmp = NULL;
  u_int16_t *buf = NULL;
  _BITMAPFILEHEADER hdr;
  _BITMAPINFOHEADER info;
  size_t px;
  int ok = -1;

  if (!sfc) {
    return -1;
  }

  hdr.signature[0] = 0x42;
  hdr.signature[1] = 0x4d;
  hdr.fileSize = sizeof(_BITMAPFILEHEADER) + sizeof(_BITMAPINFOHEADER) +
                 sizeof(u_int16_t) * sfc->dim.r * sfc->dim.b;
  hdr.reserved1 = 0;
  hdr.reserved2 = 0;
  hdr.pixelOffset = sizeof(_BITMAPFILEHEADER) + sizeof(_BITMAPINFOHEADER);

  info.size = sizeof(hdr);
  info.width = sfc->dim.r;
  info.height = sfc->dim.b;
  info.planes = 1;
  info.bpp = 16;
  info.compression = 0;
  info.bmpSize = 0;
  info.hRes = 0;
  info.vRes = 0;
  info.paletteSize = 0;
  info.importantColors = 0;

  bmp = fopen(_file, "wb");

  if (bmp < 0) {
    goto cleanup;
  }

  // Allocate a new buffer for the RGB565 data.
  px = sfc->dim.r * sfc->dim.b;
  buf = (u_int16_t *)malloc(sizeof(u_int16_t) * px);

  if (!buf) {
    goto cleanup;
  }

  // Dither the surface for the TFT display.
  ditherSurface(sfc, buf);

  fwrite(&hdr, sizeof(hdr), 1, bmp);
  fwrite(&info, sizeof(info), 1, bmp);
  fwrite(buf, sizeof(u_int16_t), px, bmp);

  ok = 0;

cleanup:
  if (bmp) {
    fclose(bmp);
  }

  if (buf) {
    free(buf);
  }

  return ok;
}

int commitSurface(const Surface _surface) {
  const _Surface *sfc = (const _Surface *)_surface;
  int fb;
  u_int16_t *buf = NULL;
  size_t px;
  int ok = -1;

  if (!sfc) {
    return -1;
  }

  fb = open("/dev/fb1", O_WRONLY);

  if (fb < 0) {
    goto cleanup;
  }

  // Allocate a new buffer for the RGB565 data.
  px = sfc->dim.r * sfc->dim.b;
  buf = (u_int16_t *)malloc(sizeof(u_int16_t) * px);

  if (!buf) {
    goto cleanup;
  }

  // Dither the surface for the TFT display.
  ditherSurface(sfc, buf);

  // Blt the 16-bit image to the framebuffer.
  write(fb, buf, sizeof(u_int16_t) * px);

  ok = 0;

cleanup:
  if (fb) {
    close(fb);
  }

  if (buf) {
    free(buf);
  }

  return ok;
}

/**
 * @brief   Private implementation of @a allocateBitmap.
 * @details Loads the specified PNG file into a @a _Bitmap.
 * @param[in] _png         The path of the PNG file to load.
 * @param[in] _colorFormat The desired color format.
 * @param[in] _bits        The desired bit depth.
 * @returns The initialized @a _Bitmap structure, or null if the PNG does not
 *          match the desired color format and bit depth or memory could not be
 *          allocated to hold the bitmap.
 */
static _Bitmap *_allocateBitmap(const char *_png, int _colorFormat, int _bits) {
  FILE *png = NULL;
  png_byte sig[8];
  png_structp pngPtr = NULL;
  png_infop pngInfo = NULL;
  png_bytep *rows = NULL;
  png_bytep ch;
  int ok = 0, row, col, color, bits;
  size_t rowBytes;
  _Bitmap *bmp = NULL;

  // Verify the designed format is supported.
  if (_colorFormat != PNG_COLOR_TYPE_GRAY &&
      _colorFormat != PNG_COLOR_TYPE_RGBA) {
    return NULL;
  }

  if (_bits != 8) {
    return NULL;
  }

  // Verify this is actually a PNG file.
  png = fopen(_png, "r");

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
  bits = png_get_bit_depth(pngPtr, pngInfo);

  png_set_interlace_handling(pngPtr);
  png_read_update_info(pngPtr, pngInfo);

  // Verify the image format matches the desired format.
  if (color != _colorFormat || bits != _bits) {
    goto cleanup;
  }

  // Attempt to allocate memory for the image.
  rowBytes = png_get_rowbytes(pngPtr, pngInfo);

  rows = (png_bytep *)malloc(sizeof(png_bytep) * bmp->dim.b);

  if (!rows) {
    goto cleanup;
  }

  rows[0] = (png_byte *)malloc(rowBytes * bmp->dim.b);

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
        RGBA *pixel = &bmp->rows[row][col];
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
        RGBA *pixel = &bmp->rows[row][col];
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
    if (rows[0]) {
      free(rows[0]);
    }

    free(rows);
  }

  if (bmp && !ok) {
    _freeBitmap(bmp);
    bmp = NULL;
  }

  return bmp;
}

Bitmap allocateBitmap(const char *_png) {
  char path[4096];
  return (Bitmap)_allocateBitmap(getPathForBitmap(_png, path, 4096),
                                 PNG_COLOR_TYPE_RGBA, 8);
}

void freeBitmap(Bitmap _bmp) {
  _freeBitmap((_Bitmap *)_bmp);
}

void drawBitmap(Surface _surface, const Bitmap _bitmap, int _x, int _y) {
  _Surface *sfc = (_Surface *)_surface;
  _Bitmap *bmp = (_Bitmap *)_bitmap;
  Compose cmp;

  if (!sfc || !bmp) {
    return;
  }

  cmp.from = bmp;
  cmp.source.l = 0;
  cmp.source.t = 0;
  cmp.source.r = bmp->dim.r;
  cmp.source.b = bmp->dim.b;

  cmp.to = sfc;
  cmp.x = _x;
  cmp.y = _y;

  cmp.color = rgbWhite;
  cmp.bkgnd = rgbNull;

  compose(&cmp);
}

void drawBitmapInBox(Surface _surface, const Bitmap _bitmap, int _l, int _t,
                     int _r, int _b) {
  _Bitmap *bmp = (_Bitmap *)_bitmap;

  if (!bmp) {
    return;
  }

  drawBitmap(_surface, _bitmap, _l + ((_r - _l) - bmp->dim.r) / 2,
             _t + ((_b - _t) - bmp->dim.b) / 2);
}

/**
 * @struct _Font
 * @brief  Private implementation of Font.
 */
typedef struct {
  _Bitmap *bmp;
  int cw, ch;
  RGBA color, bkgnd;
} _Font;

Font allocateFont(FontSize _size) {
  char path[4096];
  const char *f;
  _Font *font;

  switch (_size) {
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

  font = (_Font *)malloc(sizeof(_Font));

  if (!font) {
    return NULL;
  }

  font->bmp =
      _allocateBitmap(getPathForFont(f, path, 4096), PNG_COLOR_TYPE_GRAY, 8);

  // The font character map must be 16 characters by 8 characters.
  if (!font->bmp || font->bmp->dim.r % 16 != 0 || font->bmp->dim.b % 8 != 0) {
    free(font);
    return NULL;
  }

  font->cw = font->bmp->dim.r / 16;
  font->ch = font->bmp->dim.b / 8;
  font->color = rgbNull;
  font->bkgnd = rgbNull;

  return (Font)font;
}

void freeFont(Font _font) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return;
  }

  if (font->bmp) {
    freeBitmap(font->bmp);
  }

  free(font);
}

int getFontCharWidth(Font _font) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return 0;
  }

  return font->cw;
}

int getFontCharHeight(Font _font) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return 0;
  }

  return font->ch;
}

void setTextColor(Font _font, const RGBA *_color) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return;
  }

  font->color = *_color;
}

void setBackgroundColor(Font _font, const RGBA *_bkgnd) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return;
  }

  font->bkgnd = *_bkgnd;
}

void drawText(Surface _surface, const Font _font, int _x, int _y,
              const char *_str, size_t _len) {
  _Surface *sfc = (_Surface *)_surface;
  const _Font *font = (const _Font *)_font;
  Compose cmp;
  char c;
  size_t i;

  if (!sfc || !font) {
    return;
  }

  if (font->cw > sfc->dim.r || font->ch > sfc->dim.b) {
    return;
  }

  // Setup the common compose paramters.
  cmp.from = font->bmp;
  cmp.color = font->color;
  cmp.bkgnd = font->bkgnd;

  cmp.to = sfc;
  cmp.x = _x;
  cmp.y = _y;

  for (i = 0; i < _len; ++i) {
    c = _str[i];

    // Null character if in the high 7-bits.
    if (c & 0x80) {
      c = 0;
    }

    // Attempt to wrap the text string if exceeding the surface width.
    if (cmp.x + font->cw >= sfc->dim.r) {
      cmp.x = _x;
      cmp.y += font->ch;
    }

    // If exceeding the surface height, there is nothing left to do.
    if (cmp.y >= sfc->dim.b) {
      return;
    }

    // Compose the character on to the surface.
    cmp.source.l = (c % 16) * font->cw;
    cmp.source.t = (c / 16) * font->ch;
    cmp.source.r = cmp.source.l + font->cw;
    cmp.source.b = cmp.source.t + font->ch;

    compose(&cmp);

    cmp.x += font->cw;
  }
}
