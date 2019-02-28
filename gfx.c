#include <stdlib.h>
#include <string.h>
#include "gfx.h"

void allocateSurface(int _w, int _h, Surface *_surface)
{
  _surface->bmp = (u_int8_t*)malloc(sizeof(u_int8_t) * _w * 4 * _h);
  _surface->w = _w;
  _surface->h = _h;
}

void freeSurface(Surface *_surface)
{
  free(_surface->bmp);
  _surface->bmp = 0;
  _surface->w = 0;
  _surface->h = 0;
}
