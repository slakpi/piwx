#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "gfx.h"

typedef struct __Surface
{
  u_int8_t *bmp;
  int w, h;
} _Surface;

Surface allocateSurface(int _w, int _h)
{
  _Surface *surface = (_Surface*)malloc(sizeof(_Surface));
  surface->bmp = (u_int8_t*)malloc(sizeof(u_int8_t) * _w * 4 * _h);
  surface->w = _w;
  surface->h = _h;

  return (Surface)surface;
}

void freeSurface(Surface _surface)
{
  _Surface *surface = (_Surface*)_surface;
  free(surface->bmp);
  free(surface);
}

typedef struct __Bitmap
{
  png_bytep *rows;
  int color, bits;
  int w, h;
} _Bitmap;

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

  if (!font->bmp || font->bmp->color != PNG_COLOR_TYPE_GRAY_ALPHA ||
       font->bmp->bits != 8 || font->bmp->w % 16 != 0 ||
       font->bmp->h % 8 != 0)
  {
    free(font);
    return NULL;
  }

  font->cw = font->bmp->w / 16;
  font->ch = font->bmp->h / 8;

  return (Font)font;
}

void freeFont(Font _font)
{
  _Font *font = (_Font*)_font;
  freeBitmap(font->bmp);
  free(font);
}
