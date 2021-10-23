/**
 * @file gfx.h
 */
#ifndef GFX_H
#define GFX_H

#if !defined __ARM_NEON__ && !defined __ARM_NEON
#error ARM NEON intrinsics required.
#endif

#include "util.h"
#include <arm_neon.h>
#include <stddef.h>

/**
 * @typedef RGBA
 * @brief   Normalized RGBA color using ARM NEON vector intrinsic.
 */
typedef float32x4_t RGBA;

#define RED(c)   ((c)[0])
#define GREEN(c) ((c)[1])
#define BLUE(c)  ((c)[2])
#define ALPHA(c) ((c)[3])

extern const RGBA rgbNull;
extern const RGBA rgbRed;
extern const RGBA rgbWhite;
extern const RGBA rgbBlack;

/**
 * @typedef Surface
 * @brief   Handle representing a drawing surface.
 */
typedef void *Surface;

/**
 * @brief   Allocate a new drawing surface.
 * @details Allocates a new drawing surface with the specified dimensions
 *          leaving the content of the surface uninitialized.
 * @param[in] w Width of the drawing surface.
 * @param[in] h Height of the drawing surface.
 * @returns Handle to the drawing surface or null if unable to allocate memory.
 */
Surface allocateSurface(int w, int h);

/**
 * @brief Free a drawing surface.
 * @param[in] surface The drawing surface to free.
 */
void freeSurface(Surface surface);

/**
 * @brief   Clear a drawing surface.
 * @details Clears the drawing surface by setting the pixels to zero.
 * @param[in] surface The drawing surface to clear.
 */
void clearSurface(Surface surface);

/**
 * @brief   Write a drawing surface to a RGBA8888 PNG file.
 * @details Writes the contents of a drawing surface to a PNG file of the same
 *          dimensions using RGBA8888. An existing file will be overwritten.
 * @param[in] surface The drawing surface to output.
 * @param[in] file    The name of the PNG file.
 * @returns Returns TRUE if successful, FALSE otherwise.
 */
boolean writeSurfaceToPNG(const Surface surface, const char *file);

/**
 * @brief   Write a drawing surface to a RGB565 BMP file.
 * @details Writes the contents of a drawing surface to a BMP file of the same
 *          dimensions using RGB565. An existing file will be overwritten.
 * @param[in] surface The drawing surface to output.
 * @param[in] file    The name of the BMP file.
 * @returns Returns TRUE if successful, FALSE otherwise.
 */
boolean writeSurfaceToBMP(const Surface surface, const char *file);

/**
 * @brief   Transfer the drawing surface to the framebuffer for display.
 * @details Converts the RGBA data to 16-bit RGB565 color and transfers the
 *          contents of the drawing surface to the framebuffer for display.
 * @param[in] surface The drawing surface to display.
 * @returns Returns TRUE if successful, FALSE otherwise.
 */
boolean commitSurface(const Surface surface);

/**
 * @typedef Bitmap
 * @brief   Handle representing a drawable bitmap.
 */
typedef void *Bitmap;

/**
 * @brief   Allocate a bitmap from a PNG file.
 * @details Attempts to load an RGBA, 32-bit PNG file.
 * @param[in] pngFile The name of the PNG file. @a allocateBitmap generates the
 *                    full path to the resource internally.
 * @returns The bitmap or null if unable to load the PNG file.
 */
Bitmap allocateBitmap(const char *pngFile);

/**
 * @brief Free a bitmap.
 * @param[in] bmp The bitmap to free.
 */
void freeBitmap(Bitmap bmp);

/**
 * @brief   Draw a bitmap to a target surface.
 * @details Draws @a bmp to the target surface @a surface at the specified
 *          point. Any part of the bitmap that falls outside of the target
 *          surface is ignored.
 * @param[in] surface The target surface to receive the bitmap.
 * @param[in] bmp     The bitmap to draw.
 * @param[in] x       The starting column on the target surface.
 * @param[in] y       The starting row on the target surface.
 */
void drawBitmap(Surface surface, const Bitmap bmp, int x, int y);

/**
 * @brief   Draw a bitmap to a target surface centered on a rectangle.
 * @details Draws @a bmp to the target surface @a surface with the center of
 *          the bitmap matched to the center of the specified rectangle on the
 *          target surface. Any part of the bitmap that falls outside of the
 *          target surface is ignored.
 *
 *          The rectangle excludes the right and bottom, e.g. valid columns are
 *          in the interval @a [l, r) and valid rows are in the interval
 *          @a [t, b).
 * @param[in] surface The target surface to receive the bitmap.
 * @param[in] bmp     The bitmap to draw.
 * @param[in] l       Left column of the rectangle.
 * @param[in] t       Top row of the rectangle.
 * @param[in] r       Right column of the rectangle.
 * @param[in] b       Bottom row of the rectangle.
 */
void drawBitmapInBox(Surface surface, const Bitmap bmp, int l, int t, int r,
                     int b);

/**
 * @typedef Font
 * @brief   Handle representing a 7-bit, fixed-width font character map.
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
typedef enum { font_6pt, font_8pt, font_10pt, font_16pt } FontSize;

/**
 * @brief   Allocate a font character map.
 * @param[in] size The font character map to load.
 * @returns The font character map or null if it could not be loaded.
 */
Font allocateFont(FontSize size);

/**
 * @brief Free a font character map.
 * @param[in] font The font character map to free.
 */
void freeFont(Font font);

/**
 * @brief   Get the fixed width of a font character.
 * @param[in] font The font character map.
 * @returns The character width or 0 if the font character map is invalid.
 */
int getFontCharWidth(Font font);

/**
 * @brief   Get the fixed height of a font character.
 * @param[in] font The font character map.
 * @returns The character height or 0 if the font character map is invalid.
 */
int getFontCharHeight(Font font);

/**
 * @brief Set the font foreground color.
 * @param[in] font  The font character map to be used for drawing.
 * @param[in] color The foreground color.
 */
void setTextColor(Font font, const RGBA *color);

/**
 * @brief Set the font background color.
 * @param[in] font  The font character map to be used for drawing.
 * @param[in] color The background color.
 */
void setBackgroundColor(Font font, const RGBA *bkgnd);

/**
 * @brief   Draw a text string.
 * @details Draw a text string to the target surface using the specified
 *          font character map. If any part of a character falls outside of the
 *          target surface, it is not drawn.
 * @param[in] surface The target surface.
 * @param[in] font    The font character map to be used for drawing.
 * @param[in] x       The starting column.
 * @param[in] y       The starting row. Characters are drawn down from this
 *                    row.
 * @param[in] str     The text string to draw.
 * @param[in] len     Length of the text string.
 */
void drawText(Surface surface, const Font font, int x, int y, const char *str,
              size_t len);

#endif /* GFX_H */
