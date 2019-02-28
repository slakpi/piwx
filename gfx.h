#ifndef GFX_H
#define GFX_H

typedef void* Surface;

Surface allocateSurface(int _w, int _h);
void freeSurface(Surface _surface);

typedef void* Bitmap;

Bitmap allocateBitmap(const char *_pngFile);
void freeBitmap(Bitmap _bmp);

typedef void* Font;

typedef enum __FontSize
{
  font_16pt,
  font_10pt,
  font_8pt,
  font_6pt
} FontSize;

Font allocateFont(FontSize _size);
void freeFont(Font _font);

#endif
