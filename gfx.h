#ifndef GFX_H
#define GFX_H

#include <stddef.h>

#ifdef __ARM_NEON__
#include <arm_neon.h>

/**
 * @typedef RGBA
 * @brief   Normalized RGBA color using ARM NEON vector intrinsic.
 */
typedef float32x4_t RGBA;

#define RED(c)   ((c)[0])
#define GREEN(c) ((c)[1])
#define BLUE(c)  ((c)[2])
#define ALPHA(c) ((c)[3])
#endif

extern const RGBA rgbNull;
extern const RGBA rgbRed;
extern const RGBA rgbWhite;
extern const RGBA rgbBlack;

/**
 * @brief   Make an RGBA object from color components.
 * @details Clamps each component to the normalized range [0.0, 1.0] and
 *          initializes the new color object.
 * @param[in]  _r     Red component.
 * @param[in]  _g     Green component.
 * @param[in]  _b     Blue component.
 * @param[in]  _a     Alpha component.
 * @param[out] _color The output color.
 */
void makeColor(float _r, float _g, float _b, float _a, RGBA *_color);

/**
 * @brief   Make an RGBA object from RGBA8888 components.
 * @details Initializes a new color object with the unsigned, 8-bit components
            normalized to the range [0.0, 1.0].
 * @param[in]  _r     8-bit Red component.
 * @param[in]  _g     8-bit Green component.
 * @param[in]  _b     8-bit Blue component.
 * @param[in]  _a     8-bit Alpha component.
 * @param[out] _color The output color.
 */
void makeColorFromRGBA8888(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a,
                           RGBA *_color);

/**
 * @brief   Make an RGBA object from a foreground color and 8-bit alpha.
 * @details Initializes a new color object with the foreground color and an
 *          8-bit alpha component normalized to the range [0.0, 1.0].
 * @param[in]  _fg    The foreground color.
 * @param[in]  _a     8-bit Alpha component.
 * @param[out] _color The output color.
 */
void makeColorFromA8(const RGBA *_fg, uint8_t _a, RGBA *_color);

/**
 * @typedef Surface
 * @brief   Handle representing a drawing surface.
 */
typedef void *Surface;

/**
 * @brief   Allocate a new drawing surface.
 * @details Allocates a new drawing surface with the specified dimensions
 *          leaving the content of the surface uninitialized.
 * @param[in] _w Width of the drawing surface.
 * @param[in] _h Height of the drawing surface.
 * @returns Handle to the drawing surface or null if unable to allocate memory.
 */
Surface allocateSurface(int _w, int _h);

/**
 * @brief Free a drawing surface.
 * @param[in] _surface The drawing surface to free.
 */
void freeSurface(Surface _surface);

/**
 * @brief   Clear a drawing surface.
 * @details Clears the drawing surface by setting the pixels to zero.
 * @param[in] _surface The drawing surface to clear.
 */
void clearSurface(Surface _surface);

/**
 * @brief   Write a drawing surface to a RGBA8888 PNG file.
 * @details Writes the contents of a drawing surface to a PNG file of the same
 *          dimensions using RGBA8888. An existing file will be overwritten.
 * @param[in] _surface The drawing surface to output.
 * @param[in] _file    The name of the PNG file.
 * @returns Returns 0 if successful.
 */
int writeSurfaceToPNG(const Surface _surface, const char *_file);

/**
 * @brief   Write a drawing surface to a RGB565 BMP file.
 * @details Writes the contents of a drawing surface to a BMP file of the same
 *          dimensions using RGB565. An existing file will be overwritten.
 * @param[in] _surface The drawing surface to output.
 * @param[in] _file    The name of the BMP file.
 * @returns Returns 0 if successful.
 */
int writeSurfaceToBMP(const Surface _surface, const char *_file);

/**
 * @brief   Transfer the drawing surface to the framebuffer for display.
 * @details Converts the RGBA data to 16-bit RGB565 color and transfers the
 *          contents of the drawing surface to the framebuffer for display.
 * @param[in] _surface The drawing surface to display.
 * @returns Returns 0 if successful.
 */
int commitSurface(const Surface _surface);

/**
 * @typedef Bitmap
 * @brief   Handle representing a drawable bitmap.
 */
typedef void *Bitmap;

/**
 * @brief   Allocate a bitmap from a PNG file.
 * @details Attempts to load an RGBA, 32-bit PNG file.
 * @param[in] _pngFile The name of the PNG file. @a allocateBitmap generates the
 *                     full path to the resource internally.
 * @returns The bitmap or null if unable to load the PNG file.
 */
Bitmap allocateBitmap(const char *_pngFile);

/**
 * @brief Free a bitmap.
 * @param[in] _bmp The bitmap to free.
 */
void freeBitmap(Bitmap _bmp);

/**
 * @brief   Draw a bitmap to a target surface.
 * @details Draws @a _bmp to the target surface @a _surface at the specified
 *          point. Any part of the bitmap that falls outside of the target
 *          surface is ignored.
 * @param[in] _surface The target surface to receive the bitmap.
 * @param[in] _bmp     The bitmap to draw.
 * @param[in] _x       The starting column on the target surface.
 * @param[in] _y       The starting row on the target surface.
 */
void drawBitmap(Surface _surface, const Bitmap _bmp, int _x, int _y);

/**
 * @brief   Draw a bitmap to a target surface centered on a rectangle.
 * @details Draws @a _bmp to the target surface @a _surface with the center of
 *          the bitmap matched to the center of the specified rectangle on the
 *          target surface. Any part of the bitmap that falls outside of the
 *          target surface is ignored.
 * 
 *          The rectangle excludes the right and bottom, e.g. valid columns are
 *          in the interval [l, r) and valid rows are in the interval [t, b).
 * @param[in] _surface The target surface to receive the bitmap.
 * @param[in] _bmp     The bitmap to draw.
 * @param[in] _l       Left column of the rectangle.
 * @param[in] _t       Top row of the rectangle.
 * @param[in] _r       Right column of the rectangle.
 * @param[in] _b       Bottom row of the rectangle.
 */
void drawBitmapInBox(Surface _surface, const Bitmap _bmp, int _l, int _t,
                     int _r, int _b);

/**
 * @typedef Font
 * @brief   Handle representing a fixed-width font character map.
 * @details A font character map contains the following:
 * 
 *            0 . . . . No character
 *            1 . . . . Degree symbol
 *            2-19. . . No characters
 *            20-126. . Standard ASCII
 *            127 . . . No character
 */
typedef void *Font;

/**
 * @enum  FontSize
 * @brief Available font character maps.
 */
typedef enum {
  font_16pt,
  font_10pt,
  font_8pt,
  font_6pt
} FontSize;

/**
 * @brief   Allocate a font character map.
 * @param[in] _size The font character map to load.
 * @returns The font character map or null if it could not be loaded.
 */
Font allocateFont(FontSize _size);

/**
 * @brief Free a font character map.
 * @param[in] _font The font character map to free.
 */
void freeFont(Font _font);

/**
 * @brief   Get the fixed width of a font character.
 * @param[in] _font The font character map.
 * @returns The character width or 0 if the font character map is invalid.
 */
int getFontCharWidth(Font _font);

/**
 * @brief   Get the fixed height of a font character.
 * @param[in] _font The font character map.
 * @returns The character height or 0 if the font character map is invalid.
 */
int getFontCharHeight(Font _font);

/**
 * @brief Set the font foreground color.
 * @param[in] _font  The font character map to be used for drawing.
 * @param[in] _color The foreground color.
 */
void setTextColor(Font _font, const RGBA *_color);

/**
 * @brief Set the font background color.
 * @param[in] _font  The font character map to be used for drawing.
 * @param[in] _color The background color.
 */
void setBackgroundColor(Font _font, const RGBA *_bkgnd);

/**
 * @brief   Draw a text string.
 * @details Draw a text string to the target surface using the specified
 *          font character map. If any part of a character falls outside of the
 *          target surface, it is not drawn.
 * @param[in] _surface The target surface.
 * @param[in] _font    The font character map to be used for drawing.
 * @param[in] _x       The starting column.
 * @param[in] _y       The starting row. Characters are drawn down from this
 *                     row.
 * @param[in] _str     The text string to draw.
 * @param[in] _len     Length of the text string.
 */
void drawText(Surface _surface, const Font _font, int _x, int _y,
              const char *_str, size_t _len);

#endif
