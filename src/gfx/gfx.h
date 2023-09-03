/**
 * @file gfx.h
 */
#if !defined GFX_H
#define GFX_H

#include "vec.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SCREEN_WIDTH      320.0f
#define SCREEN_HEIGHT     240.0f
#define INVALID_RESOURCES NULL

/**
 * @typedef DrawResources
 * @brief   Gfx context handle.
 */
typedef void *DrawResources;

/**
 * @enum  Font
 * @brief Gfx font handles.
 */
typedef enum { font6pt, font8pt, font10pt, font16pt, fontCount } Font;

/**
 * @struct CharInfo
 * @brief  Font character information in pixels.
 */
typedef struct {
  Vector2f cellSize;  // Cell dimensions
  float    baseline;  // Baseline position from bottom of cell
  float    capHeight; // Capital letter height
  float    xHeight;   // Lowercase x height
  float    leading;   // Line spacing
} CharInfo;

/**
 * @enum  CharVertAlign
 * @brief Vertical alignment of characters.
 */
typedef enum { vertAlignBaseline, vertAlignCell } CharVertAlign;

/**
 * @enum  Icon
 * @brief Gfx icon image handles.
 */
typedef enum {
  iconCatIFR,
  iconCatLIFR,
  iconCatMVFR,
  iconCatUnk,
  iconCatVFR,
  iconDownloadErr,
  iconDownloading,
  iconWxWind30,
  iconWxWind60,
  iconWxWind90,
  iconWxWind120,
  iconWxWind150,
  iconWxWind180,
  iconWxWind210,
  iconWxWind240,
  iconWxWind270,
  iconWxWind300,
  iconWxWind330,
  iconWxWind360,
  iconWxWindCalm,
  iconWxWindUnk,
  iconWxBrokenDay,
  iconWxBrokenNight,
  iconWxChanceFlurries,
  iconWxChanceFZRA,
  iconWxChanceRain,
  iconWxChanceSnow,
  iconWxChanceTS,
  iconWxClearDay,
  iconWxClearNight,
  iconWxFewDay,
  iconWxFewNight,
  iconWxFlurries,
  iconWxFogHaze,
  iconWxFunnelCloud,
  iconWxFZRA,
  iconWxOvercast,
  iconWxRain,
  iconWxSleet,
  iconWxSnow,
  iconWxThunderstorms,
  iconWxVolcanicAsh,
  iconCount
} Icon;

/**
 * @typedef Color4f
 * @brief   Floating-point RGBA.
 */
typedef Vector4f Color4f;

/**
 * @typedef Point2f
 * @brief   Floating-point 2D point.
 */
typedef Vector2f Point2f;

/**
 * @brief Clears the current drawing surface with the specified color.
 * @param[in] resources The gfx context.
 * @param[in] clear     The clear color.
 */
void gfx_clearSurface(DrawResources resources, Color4f clear);

/**
 * @brief   Commits the current drawing surface to the screen.
 * @param[in] resources The gfx context.
 * @returns True if able to write the OpenGL color buffer to the screen.
 */
bool gfx_commitToScreen(DrawResources resources);

/**
 * @brief Cleans up the specified gfx context.
 * @param[in,out] resources The gfx context to cleanup. The pointer must be
 *                          valid, but it may point to a NULL context. On
 *                          return, the pointer will point to a NULL context.
 */
void gfx_cleanupGraphics(DrawResources *resources);

/**
 * @brief Draw an icon centered on a point.
 * @param[in] resources The gfx context.
 * @param[in] icon      The icon to draw.
 * @param[in] center    The center of the icon in pixels.
 */
void gfx_drawIcon(DrawResources, Icon icon, Point2f center);

/**
 * @brief Draws a solid line with the given color and width.
 * @param[in] resources The gfx context.
 * @param[in] vertices  An array of two points that comprise the line.
 * @param[in] color     The color of the line.
 * @param[in] width     The width of the line.
 */
void gfx_drawLine(DrawResources resources, const Point2f *vertices, Color4f color, float width);

/**
 * @brief   Draw a solid quadrilateral with the given color.
 * @details The quad's vertices should be ordered as follows:
 *
 *          0 +---+ 1
 *            |  /|
 *            | / |
 *            |/  |
 *          2 +---+ 3
 *
 * @param[in] resources The gfx context.
 * @param[in] vertices  An array of four points that comprise the quad.
 * @param[in] color     The color of the rectangle.
 */
void gfx_drawQuad(DrawResources resources, const Point2f *vertices, Color4f color);

/**
 * @brief Draw a string of text with the given color.
 * @param[in] resources  The gfx context.
 * @param[in] font       The font to use.
 * @param[in] bottomLeft The bottom-left starting coordinates in pixels.
 * @param[in] text       The text string.
 * @param[in] len        Length of the text string in characters.
 * @param[in] textColor  The character color.
 * @param[in] valign     Character vertical alignment
 */
void gfx_drawText(DrawResources resources, Font font, Point2f bottomLeft, const char *text,
                  size_t len, Color4f textColor, CharVertAlign valign);

/**
 * @brief   Dumps the current drawing surface to a PNG image.
 * @param[in] resources The gfx context.
 * @param[in] path      The path to the PNG file.
 * @returns True if write succeeds, false otherwise.
 */
bool gfx_dumpSurfaceToPng(DrawResources resources, const char *path);

/**
 * @brief Get the character information for a font.
 * @param[in] resources The gfx context.
 * @param[in] font      The font to query.
 * @param[out] info     The character information.
 */
bool gfx_getFontInfo(DrawResources resources, Font font, CharInfo *info);

/**
 * @brief Get the width and height of an icon.
 * @param[in] resources The gfx context.
 * @param[in] icon      The icon to query.
 * @param[out] size     The icon dimenisons in pixels.
 */
bool gfx_getIconInfo(DrawResources resources, Icon icon, Vector2f *size);

/**
 * @brief Get error information from a gfx context.
 * @param[in] resources The gfx context.
 * @param[out] error    The EGL error code.
 * @param[out] msg      The EGL error message.
 * @param[in] len       Length of @a msg.
 */
void gfx_getGfxError(DrawResources resources, int *error, char *msg, size_t len);

/**
 * @brief   Initialize a new gfx context.
 * @param[out] resources The new gfx context.
 * @returns True if able to create a new gfx context, false otherwise.
 */
bool gfx_initGraphics(DrawResources *resources);

#endif /* GFX_H */
