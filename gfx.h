#ifndef GFX_H
#define GFX_H

typedef struct __RGB
{
  double r, g, b, a;
} RGB;

typedef void* Surface;

Surface allocateSurface(int _w, int _h);
void freeSurface(Surface _surface);
int writeToFramebuffer(const Surface _surface);

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
void setTextColor(Font, RGB *_color);
void setBackgroundColor(Font, RGB *_bkgnd);
void drawText(Surface _surface, const Font _font, int _x, int _y,
  const char *_str, size_t _len);

#endif
