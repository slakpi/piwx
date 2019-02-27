#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
/*#include "fonts/sfmono6.h"*/
/*#include "fonts/sfmono8.h"*/
/*#include "fonts/sfmono10.h"*/
#include "fonts/sfmono16.h"
#include "wx.h"

#define FONT_PIXEL(data,pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}

static const char *shortArgs = "st:";
static const struct option longArgs[] = {
  { "stand-alone", no_argument,       0, 's' },
  { "test",        required_argument, 0, 't' },
  { 0,             0,                 0,  0  }
};

typedef struct __Font
{
  const char *bmp;
  int w, h;
  int cw, ch;
} Font;

typedef struct __Cell
{
  const char *start;
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
  memset(_cell, 0, sizeof(Cell));

  if (_c & 0x80)
    _c = 0;

  _cell->start = _font->bmp + (_c / 16 * _font->w * 4 * _font->ch) +
    (_c % 16 * _font->cw * 4);

  _cell->w = _font->cw;
  _cell->h = _font->ch;
  _cell->offset = (_font->w - _font->cw) * 4;
}

static u_int16_t alphaBlend(const u_int8_t _alpha, const RGB *_bkgnd,
  const RGB *_color)
{
  u_int16_t p;
  double a = _alpha / 255.0, ai = 1.0 - a;

  p  = (u_int16_t)((_color->r * a + _bkgnd->r * ai) * 0x1f);
  p <<= 6;

  p |= (u_int16_t)((_color->g * a + _bkgnd->g * ai) * 0x3f);
  p <<= 5;

  p |= (u_int16_t)((_color->b * a + _bkgnd->b * ai) * 0x1f);

  return p;
}

static void drawCharacter(u_int16_t *_out, int _offset, const Cell *_cell,
  const RGB *_bkgnd, const RGB *_color)
{
  const char *p = _cell->start;
  u_int16_t *o = _out;
  u_int8_t px[3];

  for (int r = 0; r < _cell->h; ++r)
  {
    for (int c = 0; c < _cell->w; ++c)
    {
      FONT_PIXEL(p, px);
      *o++ = alphaBlend(px[0], _bkgnd, _color);
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
  switch (_signo)
  {
  case SIGINT:
  case SIGTERM:
  case SIGHUP:
    run = 0;
    break;
  }
}

static int go()
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
  color.b = 1.0;

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

static void testSunriseSunset()
{
  struct tm sunrise, sunset;
  getSunriseSunset(&sunrise, &sunset);
}

int main(int _argc, char* _argv[])
{
  pid_t pid, sid;
  int c, t = 0, standAlone = 0;

  while ((c = getopt_long(_argc, _argv, shortArgs, longArgs, 0)) != -1)
  {
    switch (c)
    {
    case 's':
      standAlone = 1;
      break;
    case 't':
      t = atoi(optarg);
      break;
    }
  }

  switch (t)
  {
  case 0:
    break;
  case 1:
    testSunriseSunset();
    return 0;
  }

  if (standAlone)
    return go();

  pid = fork();

  if (pid < 0)
    return -1;
  if (pid > 0)
    return 0;

  umask(0);

  sid = setsid();

  if (sid < 0)
    return -1;

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  return go();
}
