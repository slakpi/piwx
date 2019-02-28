#ifndef GFX_H
#define GFX_H

#include <sys/types.h>

typedef struct __Surface
{
  u_int8_t *bmp; /* RGBA bitmap */
  int w, h;
} Surface;

void allocateSurface(int _w, int _h, Surface *_out);
void freeSurface(Surface *_surface);

#endif
