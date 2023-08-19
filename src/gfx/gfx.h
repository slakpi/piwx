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
typedef enum { FONT_6PT, FONT_8PT, FONT_10PT, FONT_16PT, FONT_COUNT } Font;

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
typedef enum { VERT_ALIGN_BASELINE, VERT_ALIGN_CELL } CharVertAlign;

/**
 * @enum  Icon
 * @brief Gfx icon image handles.
 */
typedef enum {
  ICON_CAT_IFR,
  ICON_CAT_LIFR,
  ICON_CAT_MVFR,
  ICON_CAT_UNK,
  ICON_CAT_VFR,
  ICON_DOWNLOAD_ERR,
  ICON_DOWNLOADING,
  ICON_WX_WIND_30,
  ICON_WX_WIND_60,
  ICON_WX_WIND_90,
  ICON_WX_WIND_120,
  ICON_WX_WIND_150,
  ICON_WX_WIND_180,
  ICON_WX_WIND_210,
  ICON_WX_WIND_240,
  ICON_WX_WIND_270,
  ICON_WX_WIND_300,
  ICON_WX_WIND_330,
  ICON_WX_WIND_360,
  ICON_WX_WIND_CALM,
  ICON_WX_WIND_UNK,
  ICON_WX_BROKEN_DAY,
  ICON_WX_BROKEN_NIGHT,
  ICON_WX_CHANCE_FLURRIES,
  ICON_WX_CHANCE_FZRA,
  ICON_WX_CHANCE_RAIN,
  ICON_WX_CHANCE_SNOW,
  ICON_WX_CHANCE_TS,
  ICON_WX_CLEAR_DAY,
  ICON_WX_CLEAR_NIGHT,
  ICON_WX_FEW_DAY,
  ICON_WX_FEW_NIGHT,
  ICON_WX_FLURRIES,
  ICON_WX_FOG_HAZE,
  ICON_WX_FUNNEL_CLOUD,
  ICON_WX_FZRA,
  ICON_WX_OVERCAST,
  ICON_WX_RAIN,
  ICON_WX_SLEET,
  ICON_WX_SNOW,
  ICON_WX_THUNDERSTORMS,
  ICON_WX_VOLCANIC_ASH,

  ICON_COUNT
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
void clearSurface(DrawResources resources, Color4f clear);

/**
 * @brief   Commits the current drawing surface to the screen.
 * @param[in] resources The gfx context.
 * @returns True if able to write the OpenGL color buffer to the screen.
 */
bool commitToScreen(DrawResources resources);

/**
 * @brief Cleans up the specified gfx context.
 * @param[in,out] resources The gfx context to cleanup. The pointer must be
 *                          valid, but it may point to a NULL context. On
 *                          return, the pointer will point to a NULL context.
 */
void cleanupGraphics(DrawResources *resources);

/**
 * @brief Draw an icon centered on a point.
 * @param[in] resources The gfx context.
 * @param[in] icon      The icon to draw.
 * @param[in] center    The center of the icon in pixels.
 */
void drawIcon(DrawResources, Icon icon, Point2f center);

/**
 * @brief Draws a solid line with the given color and width.
 * @param[in] resources The gfx context.
 * @param[in] vertices  An array of two points that comprise the line.
 * @param[in] color     The color of the line.
 * @param[in] width     The width of the line.
 */
void drawLine(DrawResources resources, const Point2f *vertices, Color4f color, float width);

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
void drawQuad(DrawResources resources, const Point2f *vertices, Color4f color);

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
void drawText(DrawResources resources, Font font, Point2f bottomLeft, const char *text, size_t len,
              Color4f textColor, CharVertAlign valign);

/**
 * @brief   Dumps the current drawing surface to a PNG image.
 * @param[in] resources The gfx context.
 * @param[in] path      The path to the PNG file.
 * @returns True if write succeeds, false otherwise.
 */
bool dumpSurfaceToPng(DrawResources resources, const char *path);

/**
 * @brief Get the character information for a font.
 * @param[in] resources The gfx context.
 * @param[in] font      The font to query.
 * @param[out] info     The character information.
 */
bool getFontInfo(DrawResources resources, Font font, CharInfo *info);

/**
 * @brief Get the width and height of an icon.
 * @param[in] resources The gfx context.
 * @param[in] icon      The icon to query.
 * @param[out] size     The icon dimenisons in pixels.
 */
bool getIconInfo(DrawResources resources, Icon icon, Vector2f *size);

/**
 * @brief Get error information from a gfx context.
 * @param[in] resources The gfx context.
 * @param[out] error    The EGL error code.
 * @param[out] msg      The EGL error message.
 * @param[in] len       Length of @a msg.
 */
void getGfxError(DrawResources resources, int *error, char *msg, size_t len);

/**
 * @brief   Initialize a new gfx context.
 * @param[out] resources The new gfx context.
 * @returns True if able to create a new gfx context, false otherwise.
 */
bool initGraphics(DrawResources *resources);

#endif /* GFX_H */
