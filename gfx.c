#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <png.h>
#include "gfx.h"

typedef struct __Bitmap
{
  png_bytep *rows;
  int color, bits;
  int w, h;
} _Bitmap;


typedef struct __Compose
{
  _Bitmap *from;
  int l, t, r, b;
  RGB color;
  RGB bkgnd;

  u_int8_t *to;
  int toff;
} Compose;

static void mix(const RGB *_a, const RGB *_b, RGB *_out)
{
  double aa, ba;
  RGB tmp;

  if (_a->a > 1.0 - DBL_EPSILON)
  {
    *_out = *_a;
    return;
  }

  if (_a->a < 1.0e-6)
  {
    *_out = *_b;
    return;
  }

  tmp.r = tmp.g = tmp.b = 0.0;
  tmp.a = 1.0 - (1.0 - _a->a) * (1.0 - _b->a);

  if (tmp.a < 1.0e-6)
  {
    tmp.a = 0.0;
    return;
  }

  aa = _a->a / tmp.a;
  ba = _b->a * (1.0 - _b->a) / tmp.a;
  tmp.r = _a->r * aa + _b->r * ba;
  tmp.g = _a->g * aa + _b->g * ba;
  tmp.b = _a->b * aa + _b->b * ba;

  *_out = tmp;
}

static void compose(const Compose *_cmp)
{
  int x, y;
  u_int8_t *p, *r;
  RGB tmp, final;

  p = _cmp->to;

  for (y = _cmp->t; y < _cmp->b; ++y)
  {
    r = _cmp->from->rows[y];

    for (x = _cmp->l; x < _cmp->r; )
    {
      if (_cmp->from->color == PNG_COLOR_TYPE_GRAY)
      {
        tmp = _cmp->color;
        tmp.a = r[x] / 255.0;
        mix(&tmp, &_cmp->bkgnd, &tmp);

        ++x;
      }
      else
      {
        tmp.r = r[x    ] / 255.0;
        tmp.g = r[x + 1] / 255.0;
        tmp.b = r[x + 2] / 255.0;
        tmp.a = r[x + 3] / 255.0;

        x += 4;
      }

      if (tmp.a > 1.0 - DBL_EPSILON)
        final = tmp;
      else
      {
        final.r = p[0] / 255.0;
        final.g = p[1] / 255.0;
        final.b = p[2] / 255.0;
        final.a = p[3] / 255.0;
        mix(&tmp, &final, &final);
      }

      *p++ = (u_int8_t)(final.r * 255.0);
      *p++ = (u_int8_t)(final.g * 255.0);
      *p++ = (u_int8_t)(final.b * 255.0);
      *p++ = (u_int8_t)(final.a * 255.0);
    }

    p += _cmp->toff;
  }
}

typedef struct __Surface
{
  u_int8_t *bmp;
  int w, h;
} _Surface;

Surface allocateSurface(int _w, int _h)
{
  _Surface *sfc = (_Surface*)malloc(sizeof(_Surface));
  sfc->bmp = (u_int8_t*)malloc(sizeof(u_int8_t) * _w * 4 * _h);
  sfc->w = _w;
  sfc->h = _h;

  return (Surface)sfc;
}

void freeSurface(Surface _surface)
{
  _Surface *sfc = (_Surface*)_surface;
  free(sfc->bmp);
  free(sfc);
}

int writeToFramebuffer(const Surface _surface)
{
  const _Surface *sfc = (const _Surface*)_surface;
  int fb;
  u_int16_t *buf, *q;
  u_int8_t *p;
  size_t i, px;
  double a;

  px = sfc->w * sfc->h;
  buf = (u_int16_t*)malloc(sizeof(u_int16_t) * px);
  p = sfc->bmp;
  q = buf;

  for (i = 0; i < px; ++i)
  {
    a = p[3] / 255.0;

    *q  = ((u_int16_t)(p[0] / 255.0 * a * 0x1f) << 11) |
          ((u_int16_t)(p[1] / 255.0 * a * 0x3f) << 5) |
           (u_int16_t)(p[2] / 255.0 * a * 0x1f);

    p += 4;
    ++q;
  }

  fb = open("/dev/fb1", O_WRONLY);
  if (fb < 0)
    return -1;

  write(fb, buf, sizeof(u_int16_t) * px);
  close(fb);

  return 0;
}

static void _freeBitmap(_Bitmap *_bmp)
{
  free(_bmp->rows);
  free(_bmp);
}

static _Bitmap* _allocateBitmap(const char *_png)
{
  FILE *png = NULL;
  png_byte sig[8];
  png_structp pngPtr = NULL;
  png_infop pngInfo = NULL;
  int ok = 0, y;
  _Bitmap *bmp = NULL;

  png = fopen(_png, "r");
  if (!png)
    return NULL;

  fread(sig, 1, 8, png);
  if (png_sig_cmp(sig, 0, 8))
    goto cleanup;

  pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!pngPtr)
    goto cleanup;

  pngInfo = png_create_info_struct(pngPtr);
  if (!pngInfo)
    goto cleanup;

  if (setjmp(png_jmpbuf(pngPtr)))
    goto cleanup;

  png_init_io(pngPtr, png);
  png_set_sig_bytes(pngPtr, 8);
  png_read_info(pngPtr, pngInfo);

  bmp = (_Bitmap*)malloc(sizeof(_Bitmap));
  bmp->w = png_get_image_width(pngPtr, pngInfo);
  bmp->h = png_get_image_height(pngPtr, pngInfo);
  bmp->color = png_get_color_type(pngPtr, pngInfo);
  bmp->bits = png_get_bit_depth(pngPtr, pngInfo);
  png_set_interlace_handling(pngPtr);
  png_read_update_info(pngPtr, pngInfo);

  if (setjmp(png_jmpbuf(pngPtr)))
    goto cleanup;

  bmp->rows = (png_bytep*)malloc(sizeof(png_bytep) * bmp->h);
  for (y = 0; y < bmp->h; ++y)
    bmp->rows[y] = (png_byte*)malloc(png_get_rowbytes(pngPtr, pngInfo));

  png_read_image(pngPtr, bmp->rows);

  ok = 1;

cleanup:
  if (png)
    fclose(png);
  if (pngPtr)
    png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
  if (bmp && !ok)
  {
    _freeBitmap(bmp);
    bmp = NULL;
  }

  return bmp;
}

Bitmap allocateBitmap(const char *_png)
{
  return (Bitmap)_allocateBitmap(_png);
}

void freeBitmap(Bitmap _bmp)
{
  _freeBitmap((_Bitmap*)_bmp);
}

typedef struct __Font
{
  _Bitmap *bmp;
  int cw, ch;
  RGB color, bkgnd;
} _Font;

Font allocateFont(FontSize _size)
{
  const char *f;
  _Font *font;

  switch (_size)
  {
  case font_16pt:
    f = "fonts/sfmono16.png";
    break;
  case font_10pt:
    f = "fonts/sfmono10.png";
    break;
  case font_8pt:
    f = "fonts/sfmono8.png";
    break;
  case font_6pt:
    f = "fonts/sfmono6.png";
    break;
  default:
    return NULL;
  }

  font = (_Font*)malloc(sizeof(_Font));
  font->bmp = _allocateBitmap(f);

  if (!font->bmp || font->bmp->color != PNG_COLOR_TYPE_GRAY ||
       font->bmp->bits != 8 || font->bmp->w % 16 != 0 ||
       font->bmp->h % 8 != 0)
  {
    free(font);
    return NULL;
  }

  font->cw = font->bmp->w / 16;
  font->ch = font->bmp->h / 8;
  font->color.a = 0.0;
  font->bkgnd.a = 0.0;

  return (Font)font;
}

void freeFont(Font _font)
{
  _Font *font = (_Font*)_font;
  freeBitmap(font->bmp);
  free(font);
}

void setTextColor(Font _font, RGB *_color)
{
  _Font *font = (_Font*)_font;
  font->color = *_color;
}

void setBackgroundColor(Font _font, RGB *_bkgnd)
{
  _Font *font = (_Font*)_font;
  font->bkgnd = *_bkgnd;
}

void drawText(Surface _surface, const Font _font, int _x, int _y,
  const char *_str, size_t _len)
{
  _Surface *sfc = (_Surface*)_surface;
  const _Font *font = (const _Font*)_font;
  Compose cmp;
  char c;
  int cx, cy;
  size_t i;

  if (font->cw > sfc->w || font->ch > sfc->h)
    return;

  cmp.from = font->bmp;
  cmp.color = font->color;
  cmp.bkgnd = font->bkgnd;

  cmp.to = sfc->bmp + (_y * sfc->w + _x) * 4;
  cmp.toff = (sfc->w - font->cw) * 4;

  cx = _x;
  cy = _y;

  for (i = 0; i < _len; ++i)
  {
    c = _str[i];

    if (c & 0x80)
      c = 0;

    if (cx + font->cw >= sfc->w)
    {
      cx = _x;
      cy += font->ch;
      cmp.to = sfc->bmp + (cy * sfc->w + cx) * 4;
    }

    if (cy + font->ch >= sfc->h)
      return;

    cmp.l = (c % 16) * font->cw;
    cmp.t = (c / 16) * font->ch;
    cmp.r = cmp.l + font->cw;
    cmp.b = cmp.t + font->ch;
    compose(&cmp);

    cx += font->cw;
    cmp.to += font->cw * 4;
  }
}
