#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "sfmono8.h"
#include "sfmono10.h"
#include "sfmono16.h"

#define FONT_PIXEL(data,pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}

#define ALPHA_BLEND_R(c, b, a)\
a = (unsigned char)((c * a / 255.0 + b * (1.0 - a / 255.0)) * 0x1f)
#define ALPHA_BLEND_G(c, b, a)\
a = (unsigned char)((c * a / 255.0 + b * (1.0 - a / 255.0)) * 0x3f)
#define ALPHA_BLEND_B(c, b, a)\
a = (unsigned char)((c * a / 255.0 + b * (1.0 - a / 255.0)) * 0x1f)

typedef struct __Font
{
  const u_int8_t *bmp;
  int w, h;
  int cw, ch;
} Font;

typedef struct __Cell
{
  const u_int8_t *start;
  int w, h;
  int offset;
} Cell;

typedef struct __Surface
{
  u_int16_t *bmp;
  int w, h;
} Surface;

typedef struct __RGB
{
  double r, g, b;
} RGB;

static inline int min(int _a, int _b)
{
  return (_a < _b ? _a : _b);
}

static inline int max(int _a, int _b)
{
  return (_a > _b ? _a : _b);
}

static void getCharacterCell(char _c, const Font *_font, Cell *_cell)
{
  int x, y;
  memset(_cell, 0, sizeof(Cell));

  if (_c & 0x80)
    _c = 0;

  _cell->start = _font->bmp + (_c / 16 * _font->w * 4 * _font->ch) +
    (_c % 16 * _font->cw * 4);

  _cell->w = _font->cw;
  _cell->h = _font->ch;
  _cell->offset = (_font->w - _font->cw) * 4;
}

static void drawCharacter(u_int16_t *_out, int _offset, const Cell *_cell,
  const RGB *_bkgnd, const RGB *_color)
{
  const u_int8_t *p = _cell->start;
  u_int16_t *o = _out;
  u_int8_t px[3];

  for (int r = 0; r < _cell->h; ++r)
  {
    for (int c = 0; c < _cell->w; ++c)
    {
      FONT_PIXEL(p, px);

      ALPHA_BLEND_R(_color->r, _bkgnd->r, px[0]);
      ALPHA_BLEND_G(_color->g, _bkgnd->g, px[1]);
      ALPHA_BLEND_B(_color->b, _bkgnd->b, px[2]);

      *o++ = (u_int16_t)(((px[0] & 0x1f) << 11) | (((px[1]) & 0x3f) << 5) |
        (px[2] & 0x1f));
    }

    p += _cell->offset;
    o += _offset;
  }
}

static void drawString(Surface *_out, int _x, int _y, const Font *_font,
  const RGB *_bkgnd, const RGB *_color, const char *_str, int _n)
{
  Cell c;
  const char *p = _str;
  u_int16_t *out;
  int cx, cy, offset;

  cx = min(max(_x, 0), _out->w);
  cy = min(max(_x, 0), _out->h);

  for (int i = 0; i < _n; ++i)
  {
    getCharacterCell(*p, _font, &c);

    if (cx + c.w >= _out->w)
    {
      cx = 0;
      cy += c.h;
    }

    if (cy + c.h >= _out->h)
      return;

    out = _out->bmp + cy * _out->w + cx;
    offset = _out->w - c.w;
    drawCharacter(out, offset, &c, _bkgnd, _color);

    ++p;
    cx += c.w;
  }
}

static const char* ids[] = {
  "KHIO", "KUAO", "KMMV", "KPDX", "KSPB", "KSLE", 0
};

static int run = 1;

static void signalHandler(int _signo)
{
  if (_signo == SIGINT)
    run = 0;
}

int main()
{
  Font f;
  RGB bkgnd, color;
  Surface out;
  int fb, i;

  signal(SIGINT, signalHandler);

  f.bmp = SFMONO16_data;
  f.w = SFMONO16_width;
  f.h = SFMONO16_height;
  f.cw = SFMONO16_cellWidth;
  f.ch = SFMONO16_cellHeight;

  bkgnd.r = 0.0;
  bkgnd.g = 0.0;
  bkgnd.b = 0.0;

  color.r = 1.0;
  color.g = 1.0;
  color.b = 0.0;

  out.w = 320;
  out.h = 240;
  out.bmp = (u_int16_t*)malloc(sizeof(u_int16_t) * out.w * out.h);

  i = 0;
  while (run)
  {
    fb = open("/dev/fb1", O_WRONLY);
    if (fb < 0)
    {
      fprintf(stderr, "Failed to open framebuffer device.\n");
      return -1;
    }

    memset(out.bmp, 0, sizeof(u_int16_t) * out.w * out.h);
    drawString(&out, 0, 0, &f, &bkgnd, &color, ids[i], 4);
    write(fb, out.bmp, sizeof(u_int16_t) * out.w * out.h);
    close(fb);

    usleep(1000000);

    if (ids[++i] == 0)
      i = 0;
  }

  free(out.bmp);

  return 0;
}
