#include "gfx.h"
#include "config_helpers.h"
#include "util.h"
#include <fcntl.h>
#include <float.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const RGBA rgbRed   = {1.0, 0.0, 0.0, 1.0};
const RGBA rgbWhite = {1.0, 1.0, 1.0, 1.0};
const RGBA rgbBlack = {0.0, 0.0, 0.0, 1.0};

/**
 * @struct _Bitmap
 * @brief  Private implementation of Bitmap
 */
typedef struct {
  png_bytep *rows;                     /* Indexed PNG row pointers.           */
  int color;                           /* PNG color type.                     */
  int bits;                            /* Bit-depth of the PNG.               */
  int w, h;                            /* Dimensions of the PNG.              */
} _Bitmap;

/**
 * @struct Compose
 * @brief  Configuration of a compose operation.
 */
typedef struct {
  _Bitmap *from;                       /* Source bitmap.                      */
  int l, t, r, b;                      /* Rectange of interest. Excludes the
                                          right and bottom.                   */
  RGBA color;                          /* Foreground color.                   */
  RGBA bkgnd;                          /* Background color.                   */

  u_int8_t *to;                        /* Start of the first output row.      */
  int stride;                          /* Stride for copy.                    */
} Compose;

/**
 * @brief   Alpha blend two colors.
 * @details Blends the foreground color @a _fg with background color @a _bg.
 * 
 *          For RGB components: Cc = Cf * Af + Cb * Ab * (1 - Af)
 *          For Alpha:          Ac = Af + Ab * (1 - Af)
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
  double ba;

  if (!_fg || !_bg || !_out) {
    return;
  }

  // Shortcut the blend if the background color's alpha is sufficiently large.
  // There is no need to calculate a composite alpha.
  if (_bg->a > 1.0 - DBL_EPSILON) {
    tmp.r = _fg->r * _fg->a + _bg->r * (1.0 - _fg->a);
    tmp.g = _fg->g * _fg->a + _bg->g * (1.0 - _fg->a);
    tmp.b = _fg->b * _fg->a + _bg->b * (1.0 - _fg->a);
    tmp.a = 1.0;

    *_out = tmp;

    return;
  }

  // Precalculate Ab * (1.0 - Af) and the final output alpha.
  ba = _bg->a * (1.0 - _fg->a);
  tmp.a = _fg->a + ba;

  // Shortcut the blend if the composite alpha is sufficiently small. Just set
  // the final color to clear black.
  if (tmp.a < 1.0e-6) {
    _out->r = _out->g = _out->b = _out->a = 0.0;
    return;
  }

  // Blend the RGB components.
  tmp.r = (_fg->r * _fg->a + _bg->r * ba) / tmp.a;
  tmp.g = (_fg->g * _fg->a + _bg->g * ba) / tmp.a;
  tmp.b = (_fg->b * _fg->a + _bg->b * ba) / tmp.a;

  *_out = tmp;
}

/**
 * @brief   Perform a compose operation.
 * @details Blends the rectangle of interest from the source bitmap on to the
 *          target surface. The compose operation specifies the source bitmap
 *          and the rectangle of interest, a pointer to the output location on
 *          the target surface, and a stride to the start of the next output row
 *          location.
 * 
 *          Source:                       Target:
 *          +------------------+          +------------------+
 *          | [l,t]---+        |          |                  |
 *          |   |     |        |          |     to==>+-----+ |
 *          |   |     |        |          |          |     | |
 *          |   +---(r,b)      |          |    stride........|
 *          |                  |          |.......==>+-----+ |
 *          +------------------+          +------------------+
 * 
 *          For grayscale bitmaps, e.g. fonts, the foreground color specified in
 *          the Compose operation is used and blended with the background color
 *          specified.
 * @param[in] _cmp The compose operation details.
 */
static void compose(const Compose *_cmp) {
  int x, y;
  u_int8_t *d, *s;
  RGBA tmp, final;

  if (!_cmp || !_cmp->to || !_cmp->from) {
    return;
  }

  d = _cmp->to;

  for (y = _cmp->t; y < _cmp->b; ++y) {
    s = _cmp->from->rows[y];

    for (x = _cmp->l; x < _cmp->r;) {
      if (_cmp->from->color == PNG_COLOR_TYPE_GRAY) {
        // For grayscale, set the foreground color and use the gray value as the
        // alpha, then blend with the background.
        tmp = _cmp->color;
        tmp.a = s[x] / 255.0;
        mix(&tmp, &_cmp->bkgnd, &tmp);

        ++x;
      } else {
        // Directly translate the RGBA value in the PNG to a floating point
        // RGBA.
        tmp.r = s[x    ] / 255.0;
        tmp.g = s[x + 1] / 255.0;
        tmp.b = s[x + 2] / 255.0;
        tmp.a = s[x + 3] / 255.0;

        x += 4;
      }

      if (tmp.a > 1.0 - DBL_EPSILON) {
        // Shortcut the mix if the alpha is sufficiently large.
        final = tmp;
      } else {
        // Mix with the surface color. The mix will shortcut if the alpha is
        // sufficiently small.
        final.r = d[0] / 255.0;
        final.g = d[1] / 255.0;
        final.b = d[2] / 255.0;
        final.a = d[3] / 255.0;
        mix(&tmp, &final, &final);
      }

      // Write the color back to the surface.
      *d++ = (u_int8_t)(final.r * 255.0);
      *d++ = (u_int8_t)(final.g * 255.0);
      *d++ = (u_int8_t)(final.b * 255.0);
      *d++ = (u_int8_t)(final.a * 255.0);
    }

    // Stride to the next output point.
    d += _cmp->stride;
  }
}

/**
 * @struct _Surface
 * @brief  Private implementation of Surface.
 */
typedef struct {
  u_int8_t *bmp;                       /* Pixel data; 4 bytes per pixel.      */
  int w, h;                            /* Dimensions of the bitmap.           */
} _Surface;

Surface allocateSurface(int _w, int _h) {
  _Surface *sfc;

  if (_w <= 0 || _h <= 0) {
    return NULL;
  }
  
  sfc = (_Surface *)malloc(sizeof(_Surface));
  
  if (!sfc) {
    return NULL;
  }

  sfc->bmp = (u_int8_t *)malloc(sizeof(u_int8_t) * _w * 4 * _h);

  if (!sfc->bmp) {
    free(sfc);
    return NULL;
  }

  sfc->w = _w;
  sfc->h = _h;

  return (Surface)sfc;
}

void freeSurface(Surface _surface) {
  _Surface *sfc = (_Surface *)_surface;

  if (!sfc) {
    return;
  }

  if (sfc->bmp) {
    free(sfc->bmp);
  }

  free(sfc);
}

void clearSurface(Surface _surface) {
  _Surface *sfc = (_Surface *)_surface;

  if (!sfc || !sfc->bmp) {
    return;
  }

  memset(sfc->bmp, 0, sizeof(u_int8_t) * sfc->w * 4 * sfc->h);
}

int writeToFile(const Surface _surface, const char *_file) {
  _Surface *sfc = (_Surface *)_surface;
  FILE *png = NULL;
  png_structp pngPtr = NULL;
  png_infop pngInfo = NULL;
  size_t rowBytes;
  png_bytep *rows = NULL;
  int ok = -1, y;

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
  png_set_IHDR(pngPtr, pngInfo, sfc->w, sfc->h, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(pngPtr, pngInfo);

  // Setup the row index pointers to the image data. The first element of the
  // array points to the beginning of the image. Each subsequent elements points
  // to the beginning of the next row which is `rowBytes` ahead of the current
  // row.
  rowBytes = png_get_rowbytes(pngPtr, pngInfo);
  rows = (png_bytep *)malloc(sizeof(png_bytep) * sfc->h);
  rows[0] = (png_bytep)sfc->bmp;

  for (y = 1; y < sfc->h; ++y) {
    rows[y] = rows[y - 1] + rowBytes;
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
    free(rows);
  }

  return ok;
}

int writeToFramebuffer(const Surface _surface) {
  const _Surface *sfc = (const _Surface *)_surface;
  int fb;
  u_int16_t *buf, *q;
  u_int8_t *p;
  size_t i, px;
  double a;
  int ok = -1;

  if (!sfc) {
    return -1;
  }

  fb = open("/dev/fb1", O_WRONLY);

  if (fb < 0) {
    goto cleanup;
  }

  // Allocate a new buffer for the RGB565 data.
  px = sfc->w * sfc->h;
  buf = (u_int16_t *)malloc(sizeof(u_int16_t) * px);
  p = sfc->bmp;
  q = buf;

  if (!buf) {
    goto cleanup;
  }

  // Convert the 32-bit pixels to 16-bit RGB565. Scale the components to [0, 1],
  // multiply them with their alpha value, then scale R and B up to 5 bits and
  // G up to 6-bits.
  for (i = 0; i < px; ++i) {
    a = p[3] / 255.0;

    *q = ((u_int16_t)(p[0] / 255.0 * a * 0x1f) << 11) |
         ((u_int16_t)(p[1] / 255.0 * a * 0x3f) << 5) |
          (u_int16_t)(p[2] / 255.0 * a * 0x1f);

    p += 4;
    ++q;
  }

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

  return 0;
}

/**
 * @brief Private implementation of @a freeBitmap.
 * @param[in] _bmp The bitmap to free.
 */
static void _freeBitmap(_Bitmap *_bmp) {
  if (!_bmp) {
    return;
  }

  if (_bmp->rows[0]) {
    free(_bmp->rows[0]);
  }

  if (_bmp->rows) {
    free(_bmp->rows);
  }

  free(_bmp);
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
  int ok = 0, y;
  size_t rowBytes;
  _Bitmap *bmp = NULL;

  png = fopen(_png, "r");

  if (!png) {
    return NULL;
  }

  fread(sig, 1, 8, png);

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

  png_init_io(pngPtr, png);
  png_set_sig_bytes(pngPtr, 8);
  png_read_info(pngPtr, pngInfo);

  // Initialize the bitmap structure with the PNG information.
  bmp = (_Bitmap *)malloc(sizeof(_Bitmap));

  if (!bmp) {
    goto cleanup;
  }

  bmp->w = png_get_image_width(pngPtr, pngInfo);
  bmp->h = png_get_image_height(pngPtr, pngInfo);
  bmp->color = png_get_color_type(pngPtr, pngInfo);
  bmp->bits = png_get_bit_depth(pngPtr, pngInfo);

  png_set_interlace_handling(pngPtr);
  png_read_update_info(pngPtr, pngInfo);

  // Verify the image matches the desired format.
  if (bmp->color != _colorFormat || bmp->bits != _bits) {
    goto cleanup;
  }

  // Attempt to allocate memory for the image.
  rowBytes = png_get_rowbytes(pngPtr, pngInfo);

  bmp->rows = (png_bytep *)malloc(sizeof(png_bytep) * bmp->h);

  if (!bmp->rows) {
    goto cleanup;
  }

  bmp->rows[0] = (png_byte *)malloc(rowBytes * bmp->h);

  if (!bmp->rows[0]) {
    goto cleanup;
  }

  // Setup the row index pointers.
  for (y = 1; y < bmp->h; ++y) {
    bmp->rows[y] = bmp->rows[y - 1] + rowBytes;
  }

  // Read the image.
  png_read_image(pngPtr, bmp->rows);

  ok = 1;

cleanup:
  if (png) {
    fclose(png);
  }

  if (pngPtr) {
    png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
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
  int tx, ty, sx, sy, bytes;

  if (!sfc || !bmp) {
    return;
  }

  if (bmp->color == PNG_COLOR_TYPE_GRAY)
    bytes = 1;
  else
    bytes = 4;

  // Clamp the target location to the surface boundaries.
  tx = max(0, min(_x, sfc->w - 1));
  ty = max(0, min(_y, sfc->h - 1));

  // Setup the compose by intersecting the bitmap's rectangle at the target
  // location with the target surface's boundaries. (tx, ty) - (_x, _y) is the
  // offset into the bitmap to account for the surface boundaries.
  sx = tx - _x;
  sy = ty - _y;

  // Check for an empty intersection.
  if (sx < 0 || sx >= bmp->w || sy < 0 || sy >= bmp->h) {
    return;
  }  

  cmp.from = bmp;
  cmp.l = sx;
  cmp.t = sy;
  cmp.r = min(sfc->w - tx, bmp->w - sx) * bytes;
  cmp.b = min(sfc->h - ty, bmp->h - sy);

  cmp.to = sfc->bmp + (ty * sfc->w + tx) * 4;
  cmp.stride = (sfc->w - min(sfc->w - tx, bmp->w)) * 4;

  compose(&cmp);
}

void drawBitmapInBox(Surface _surface, const Bitmap _bitmap, int _l, int _t,
                     int _r, int _b) {
  _Bitmap *bmp = (_Bitmap *)_bitmap;

  if (!bmp) {
    return;
  }

  drawBitmap(_surface, _bitmap, _l + ((_r - _l) - bmp->w) / 2,
             _t + ((_b - _t) - bmp->h) / 2);
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
    return;
  }

  font->bmp =
      _allocateBitmap(getPathForFont(f, path, 4096), PNG_COLOR_TYPE_GRAY, 8);

  // The font character map must be 16 characters by 8 characters.
  if (!font->bmp || font->bmp->w % 16 != 0 || font->bmp->h % 8 != 0) {
    free(font);
    return NULL;
  }

  font->cw = font->bmp->w / 16;
  font->ch = font->bmp->h / 8;
  font->color.a = 0.0;
  font->bkgnd.a = 0.0;

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
    return;
  }

  return font->cw;
}

int getFontCharHeight(Font _font) {
  _Font *font = (_Font *)_font;

  if (!font) {
    return;
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
  int tx, ty, px;
  size_t i;

  if (!sfc || !font) {
    return;
  }

  if (font->cw > sfc->w || font->ch > sfc->h) {
    return;
  }

  // Clamp the target location to the surface boundaries.
  tx = min(max(_x, 0), sfc->w - 1);
  ty = min(max(_y, 0), sfc->h - 1);

  // Check if it is even possible to draw characters from this position.
  if (tx + font->cw > sfc->w || ty + font->ch > sfc->h) {
    return;
  }

  // Setup the common compose paramters.
  cmp.from = font->bmp;
  cmp.color = font->color;
  cmp.bkgnd = font->bkgnd;

  cmp.to = sfc->bmp + (ty * sfc->w + tx) * 4;
  cmp.stride = (sfc->w - font->cw) * 4;

  px = tx;

  for (i = 0; i < _len; ++i) {
    c = _str[i];

    // Null character if in the high 7-bits.
    if (c & 0x80) {
      c = 0;
    }

    // Attempt to wrap the text string if exceeding the surface width.
    if (px + font->cw >= sfc->w) {
      px = tx;
      ty += font->ch;
      cmp.to = sfc->bmp + (ty * sfc->w + px) * 4;
    }

    // If exceeding the surface height, there is nothing left to do.
    if (ty + font->ch >= sfc->h) {
      return;
    }

    // Compose the character on to the surface.
    cmp.l = (c % 16) * font->cw;
    cmp.t = (c / 16) * font->ch;
    cmp.r = cmp.l + font->cw;
    cmp.b = cmp.t + font->ch;
    compose(&cmp);

    px += font->cw;
    cmp.to += font->cw * 4;
  }
}
