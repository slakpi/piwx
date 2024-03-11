/**
 * @file gfx.h
 */
#if !defined GFX_H
#define GFX_H

#include "geo.h"
#include "vec.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define GFX_INVALID_RESOURCES NULL
#define GFX_SCREEN_WIDTH      320.0f
#define GFX_SCREEN_HEIGHT     240.0f

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
 * @enum  Layer
 * @brief Cached layer identifier.
 */
typedef enum {
  layerForeground, // Display foreground layer
  layerBackground, // Display background layer
  layerTempA,      // Gfx library temp layer (do not use directly)
  layerTempB,      // General-use temp layer
  layerCount
} Layer;

/**
 * @typedef Color4f
 * @brief   Floating-point RGBA.
 */
typedef Vector4f Color4f;

/**
 * @typedef Point3f
 * @brief   Floating-point 3D point.
 */
typedef Vector3f Point3f;

/**
 * @typedef Point2f
 * @brief   Floating-point 2D point.
 */
typedef Vector2f Point2f;

/**
 * @struct BoundingBox2D
 * @brief  A rectangular bounding box in pixels.
 */
typedef struct {
  Point2f topLeft;
  Point2f bottomRight;
} BoundingBox2D;

extern const Color4f gfx_Clear;
extern const Color4f gfx_Red;
extern const Color4f gfx_Blue;
extern const Color4f gfx_Green;
extern const Color4f gfx_Magenta;
extern const Color4f gfx_Yellow;
extern const Color4f gfx_Cyan;
extern const Color4f gfx_White;
extern const Color4f gfx_Black;

/**
 * @brief   Begin drawing to the specified cache layer.
 * @details If necessary, creates the framebuffer and texture objects to receive
 *          the draw commands. Draw commands must be followed by `gfx_endLayer`.
 *          The cached texture can be drawn with `gfx_drawLayer`.
 * @param[in] resources The gfx context.
 * @param[in] layer     The layer cache.
 */
void gfx_beginLayer(DrawResources resources, Layer layer);

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
 * @brief Draw the Earth centered on the given latitude and longitude.
 * @param[in] resources The gfx context.
 * @param[in] pos       Eye position over the globe.
 * @param[in] curTime   The current system time.
 * @param[in] box       The bounding box for the globe in pixels.
 */
void gfx_drawGlobe(DrawResources resources, Position pos, time_t curTime, const BoundingBox2D *box);

/**
 * @brief Draw an icon centered on a point.
 * @param[in] resources The gfx context.
 * @param[in] icon      The icon to draw.
 * @param[in] center    The center of the icon in pixels.
 */
void gfx_drawIcon(DrawResources resources, Icon icon, Point2f center);

/**
 * @brief Draws the cached layer.
 * @param[in] resources The gfx context.
 * @param[in] layer     The cached layer to draw.
 * @param[in] shadow    Draw a shadow of the layer contents.
 */
void gfx_drawLayer(DrawResources resources, Layer layer, bool shadow);

/**
 * @brief Draws a solid line with the given color and width.
 * @param[in] resources The gfx context.
 * @param[in] vertices  An array of two points that comprise the line.
 * @param[in] color     The color of the line.
 * @param[in] width     The width of the line.
 */
void gfx_drawLine(DrawResources resources, const Point2f *vertices, Color4f color, float width);

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
 * @brief Ends cached layer drawing.
 * @param[in] resources The gfx context.
 */
void gfx_endLayer(DrawResources resources);

/**
 * @brief Get the character information for a font.
 * @param[in]  resources The gfx context.
 * @param[in]  font      The font to query.
 * @param[out] info      The character information.
 */
bool gfx_getFontInfo(DrawResources resources, Font font, CharInfo *info);

/**
 * @brief Get the width and height of an icon.
 * @param[in]  resources The gfx context.
 * @param[in]  icon      The icon to query.
 * @param[out] size      The icon dimenisons in pixels.
 */
bool gfx_getIconInfo(DrawResources resources, Icon icon, Vector2f *size);

/**
 * @brief Get error information from a gfx context.
 * @param[in]  resources The gfx context.
 * @param[out] error     The EGL error code.
 * @param[out] msg       The EGL error message.
 * @param[in]  len       Length of @a msg.
 */
void gfx_getGfxError(DrawResources resources, int *error, char *msg, size_t len);

/**
 * @brief   Initialize a new gfx context.
 * @param[in]  fontResources  The path to PiWx's font resources.
 * @param[in]  imageResources The path to PiWx's image resources.
 * @param[out] resources      The new gfx context.
 * @returns True if able to create a new gfx context, false otherwise.
 */
bool gfx_initGraphics(const char *fontResources, const char *imageResources,
                      DrawResources *resources);

#endif /* GFX_H */
