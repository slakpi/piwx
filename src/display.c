/**
 * @file display.c
 */
#include "display.h"
#include "geo.h"
#include "gfx.h"
#include "util.h"
#include "wx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define UPPER_DIV 81.0f
#define LOWER_DIV 122.0f

static void drawBackground(DrawResources resources);

static void drawCloudLayers(DrawResources *resources, const WxStation *station);

static void drawStationIdentifier(DrawResources resources, const char *ident);

static void drawStationFlightCategory(DrawResources resources, FlightCategory cat);

static void drawStationWeather(DrawResources resources, DominantWeather wx);

static void drawStationWxString(DrawResources resources, const char *wxString);

static void drawTempDewPointVisAlt(DrawResources *resources, const WxStation *station);

static void drawWindInfo(DrawResources *resources, const WxStation *station);

static void getCloudLayerText(const SkyCondition *sky, char *buf, size_t len);

static Icon getFlightCategoryIcon(FlightCategory cat);

static Icon getWeatherIcon(DominantWeather wx);

static Icon getWindIcon(int direction);

static void getWindDirectionText(char *buf, size_t len, int direction, int speed);

static void getWindSpeedText(char *buf, size_t len, int speed);

void clearFrame(DrawResources resources) {
  gfx_clearSurface(resources, gfx_Clear);
}

void drawDownloadInProgress(DrawResources resources) {
  Point2f center = {{GFX_SCREEN_WIDTH / 2.0f, GFX_SCREEN_HEIGHT / 2.0f}};

  gfx_clearSurface(resources, gfx_Clear);
  gfx_drawIcon(resources, iconDownloading, center);
}

void drawDownloadError(DrawResources resources) {
  Point2f center = {{GFX_SCREEN_WIDTH / 2.0f, GFX_SCREEN_HEIGHT / 2.0f}};

  gfx_clearSurface(resources, gfx_Clear);
  gfx_drawIcon(resources, iconDownloadErr, center);
}

void drawGlobe(DrawResources resources, time_t curTime, Position pos) {
  const Point2f       topLeft     = {{-GFX_SCREEN_WIDTH * 0.25f, 0.0f}};
  const Point2f       bottomRight = {{topLeft.coord.x + GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT}};
  const BoundingBox2D box         = {topLeft, bottomRight};

  // Adjust the latitude down by 10 degrees to place the station within the
  // weather phenomena box.
  Position eyePos = {pos.lat - 10.0f, pos.lon};

  gfx_drawGlobe(resources, eyePos, curTime, &box);
}

void drawStation(DrawResources resources, time_t curTime, const WxStation *station) {
  drawBackground(resources);
  drawStationIdentifier(resources, station->localId);
  drawStationFlightCategory(resources, station->cat);
  drawStationWeather(resources, station->wx);
  drawStationWxString(resources, station->wxString);
  drawCloudLayers(resources, station);
  drawWindInfo(resources, station);
  drawTempDewPointVisAlt(resources, station);
}

/**
 * @brief Draw the station weather background.
 * @param[in] resources The gfx context.
 */
static void drawBackground(DrawResources resources) {
  const Point2f lines[] = {{{0.0f, UPPER_DIV}},
                           {{GFX_SCREEN_WIDTH, UPPER_DIV}},
                           {{0.0f, LOWER_DIV}},
                           {{GFX_SCREEN_WIDTH, LOWER_DIV}}};

  // Draw the separator lines.
  gfx_drawLine(resources, &lines[0], gfx_White, 2.0f);
  gfx_drawLine(resources, &lines[2], gfx_White, 2.0f);
}

/**
 * @brief Draw the station identifier.
 * @param[in] resources The gfx context.
 * @param[in] ident     The weather station identifier.
 */
static void drawStationIdentifier(DrawResources resources, const char *ident) {
  CharInfo info       = {0};
  Point2f  bottomLeft = {0};

  if (!ident) {
    return;
  }

  if (!gfx_getFontInfo(resources, font16pt, &info)) {
    return;
  }

  bottomLeft.coord.y = info.cellSize.v[1];

  gfx_drawText(resources, font16pt, bottomLeft, ident, strlen(ident), gfx_White, vertAlignCell);
}

/**
 * @brief Draw a station's flight category icon.
 * @param[in] resources The gfx context.
 * @param[in] cat       The weather station's flight category.
 */
static void drawStationFlightCategory(DrawResources resources, FlightCategory cat) {
  const Point2f center = {{205.0f, 40.5f}};
  Icon          icon   = getFlightCategoryIcon(cat);
  gfx_drawIcon(resources, icon, center);
}

/**
 * @brief   Get the icon handle for a flight category.
 * @param[in] cat The flight category.
 * @returns An icon handle.
 */
static Icon getFlightCategoryIcon(FlightCategory cat) {
  switch (cat) {
  case catLIFR:
    return iconCatLIFR;
  case catIFR:
    return iconCatIFR;
  case catMVFR:
    return iconCatMVFR;
  case catVFR:
    return iconCatVFR;
  default:
    return iconCatUnk;
  }
}

/**
 * @brief Draw a station's dominant weather icon.
 * @param[in] resources The gfx context.
 * @param[in] wx        The dominant weather phenomenon.
 */
static void drawStationWeather(DrawResources resources, DominantWeather wx) {
  const Point2f center = {{278.0f, 40.5f}};
  Icon          icon   = getWeatherIcon(wx);
  gfx_drawIcon(resources, icon, center);
}

/**
 * @brief   Get an icon handle for a dominant weather phenomenon.
 * @param[in] wx Dominant weather phenomenon.
 * @returns An icon handle or iconCount if the weather phenomenon is invalid.
 */
static Icon getWeatherIcon(DominantWeather wx) {
  switch (wx) {
  case wxClearDay:
    return iconWxClearDay;
  case wxClearNight:
    return iconWxClearNight;
  case wxScatteredOrFewDay:
    return iconWxFewDay;
  case wxScatteredOrFewNight:
    return iconWxFewNight;
  case wxBrokenDay:
    return iconWxBrokenDay;
  case wxBrokenNight:
    return iconWxBrokenNight;
  case wxOvercast:
    return iconWxOvercast;
  case wxLightMistHaze:
  case wxObscuration:
    return iconWxFogHaze;
  case wxLightDrizzleRain:
    return iconWxChanceRain;
  case wxRain:
    return iconWxRain;
  case wxFlurries:
    return iconWxFlurries;
  case wxLightSnow:
    return iconWxChanceSnow;
  case wxSnow:
    return iconWxSnow;
  case wxLightFreezingRain:
    return iconWxChanceFZRA;
  case wxFreezingRain:
    return iconWxFZRA;
  case wxVolcanicAsh:
    return iconWxVolcanicAsh;
  case wxLightTstormsSqualls:
    return iconWxChanceTS;
  case wxTstormsSqualls:
    return iconWxThunderstorms;
  case wxFunnelCloud:
    return iconWxFunnelCloud;
  default:
    return iconCount; // Invalid
  }
}

/**
 * @brief Draw a station's weather phenomena string.
 * @param[in] resources The gfx context.
 * @param[in] wxString  The weather phenomena string.
 */
static void drawStationWxString(DrawResources resources, const char *wxString) {
  Point2f  bottomLeft = {0};
  CharInfo info       = {0};
  size_t   len        = 0;

  if (!wxString) {
    return;
  }

  // TODO: There's a weird rendering issue with the 8pt font that causes
  //       distortions in the lettering occasionally. For now, just use the 6pt
  //       font which does not seem to have the same problems and is still
  //       legible with shadowing.
  if (!gfx_getFontInfo(resources, font6pt, &info)) {
    return;
  }

  len                = strlen(wxString);
  bottomLeft.coord.x = (GFX_SCREEN_WIDTH - (info.cellSize.v[0] * len)) / 2.0f;
  bottomLeft.coord.y = LOWER_DIV - (LOWER_DIV - UPPER_DIV - info.cellSize.v[1]) / 2.0f;
  gfx_drawText(resources, font6pt, bottomLeft, wxString, len, gfx_White, vertAlignCell);
}

/**
 * @brief   Draws the cloud layers present at a station.
 * @details Draws layer information for the lowest ceiling and the next highest
 *          cloud layer, or, if there is no ceiling, the lowest and next highest
 *          cloud layers.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 */
static void drawCloudLayers(DrawResources *resources, const WxStation *station) {
  Point2f       bottomLeft = {{172.0f, LOWER_DIV + 10.0f}};
  CharInfo      info       = {0};
  SkyCondition *sky        = station->layers;
  char          buf[33]    = {0};

  if (!sky) {
    return;
  }

  if (!gfx_getFontInfo(resources, font6pt, &info)) {
    return;
  }

  bottomLeft.coord.y += info.capHeight;

  switch (sky->coverage) {
  case skyClear:
    strncpy_safe(buf, COUNTOF(buf), "Clear");
    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);
    return;
  case skyOvercastSurface:
    if (!station->hasVertVis || station->vertVis <= 0) {
      strncpy_safe(buf, COUNTOF(buf), "VV ---");
    } else {
      // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
      snprintf(buf, COUNTOF(buf), "VV %d", station->vertVis);
    }

    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);
    return;
  default:
    break;
  }

  // Find the ceiling.
  while (sky) {
    if (sky->coverage >= skyBroken) {
      if (sky->prev) {
        sky = sky->prev;
      }

      break;
    }

    sky = sky->next;
  }

  // No ceiling of broken or overcast, draw the lowest layer.
  if (!sky) {
    sky = station->layers;
  }

  // Draw the next highest layer if there is one.
  if (sky->next) {
    getCloudLayerText(sky->next, buf, COUNTOF(buf));
    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);
    bottomLeft.coord.y += info.capHeight + info.leading;
  }

  getCloudLayerText(sky, buf, COUNTOF(buf));
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);
}

/**
 * @brief Converts cloud layer data to a string METAR-style string.
 * @param[in] sky Cloud layer information.
 * @param[in] buf Buffer to receive the string.
 * @param[in] len Size of the string buffer.
 */
static void getCloudLayerText(const SkyCondition *sky, char *buf, size_t len) {
  const char *cover = "---";

  switch (sky->coverage) {
  case skyScattered:
    cover = "SCT";
    break;
  case skyFew:
    cover = "FEW";
    break;
  case skyBroken:
    cover = "BKN";
    break;
  case skyOvercast:
    cover = "OVC";
    break;
  default:
    break;
  }

  snprintf(buf, len, "%s %d", cover, sky->height); // NOLINT
}

/**
 * @brief Draws the wind information and direction icon.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 */
static void drawWindInfo(DrawResources *resources, const WxStation *station) {
  char     buf[33]    = {0};
  Point2f  bottomLeft = {{84.0f, LOWER_DIV + 10.0f}};
  CharInfo fontInfo   = {0};
  Vector2f iconInfo   = {0};
  Icon     icon       = getWindIcon(station->hasWindDir ? station->windDir : -1);

  if (!gfx_getFontInfo(resources, font6pt, &fontInfo)) {
    return;
  }

  if (!gfx_getIconInfo(resources, icon, &iconInfo)) {
    return;
  }

  iconInfo.coord.x = 10.0f + (iconInfo.coord.x / 2.0f);
  iconInfo.coord.y = bottomLeft.coord.y + (iconInfo.coord.y / 2.0f);
  gfx_drawIcon(resources, icon, iconInfo);

  getWindDirectionText(buf, COUNTOF(buf), station->hasWindDir ? station->windDir : -1,
                       station->windSpeed);
  bottomLeft.coord.y += fontInfo.capHeight;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);

  getWindSpeedText(buf, COUNTOF(buf), station->hasWindSpeed ? station->windSpeed : -1);
  bottomLeft.coord.y += fontInfo.capHeight + fontInfo.leading;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);

  getWindSpeedText(buf, COUNTOF(buf), station->hasWindGust ? station->windGust : -1);
  bottomLeft.coord.y += fontInfo.capHeight + fontInfo.leading;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_Yellow, vertAlignBaseline);
}

/**
 * @brief   Gets an icon handle for a wind direction.
 * @param[in] direction The wind direction.
 * @returns An icon handle or iconCount if the direction is invalid.
 */
static Icon getWindIcon(int direction) {
  if (direction < 0) {
    return iconWxWindUnk;
  }

  switch ((direction + 15) / 30 * 30) {
  case 0:
    // If the explicit wind direction is zero, this means winds calm. However,
    // if the wind direction is 1-14, the sector will still be centered on 0,
    // so handle both cases here.
    if (direction == 0) {
      return iconWxWindCalm;
    } else {
      return iconWxWind360;
    }
  case 30:
    return iconWxWind30;
  case 60:
    return iconWxWind60;
  case 90:
    return iconWxWind90;
  case 120:
    return iconWxWind120;
  case 150:
    return iconWxWind150;
  case 180:
    return iconWxWind180;
  case 210:
    return iconWxWind210;
  case 240:
    return iconWxWind240;
  case 270:
    return iconWxWind270;
  case 300:
    return iconWxWind300;
  case 330:
    return iconWxWind330;
  case 360:
    return iconWxWind360;
  default:
    return iconCount; // Invalid
  }
}

/**
 * @brief Converts a wind direction to text.
 * @param[out] buf       The wind direction text.
 * @param[in]  len       Length of @a buf.
 * @param[in]  direction The wind direction.
 * @param[in]  speed     The wind speed.
 */
static void getWindDirectionText(char *buf, size_t len, int direction, int speed) {
  if (direction > 0) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, len, "%d\x01", direction);
  } else if (direction == 0) {
    if (speed > 0) {
      strncpy_safe(buf, len, "Var");
    } else if (speed == 0) {
      strncpy_safe(buf, len, "Calm");
    } else {
      strncpy_safe(buf, len, "---");
    }
  } else {
    strncpy_safe(buf, len, "---");
  }
}

/**
 * @brief Converts a wind speed to text.
 * @param[out] buf     The wind speed text.
 * @param[in]  len     Length of @a buf.
 * @param[in]  speed   The wind speed.
 */
static void getWindSpeedText(char *buf, size_t len, int speed) {
  if (speed <= 0) {
    strncpy_safe(buf, len, "---");
  } else {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, len, "%dkt", speed);
  }
}

/**
 * @brief Draws the temperature, dewpoint, visibility, and altimeter setting
 *        information.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 */
static void drawTempDewPointVisAlt(DrawResources *resources, const WxStation *station) {
  CharInfo info       = {0};
  Point2f  bottomLeft = {0};
  char     buf[33]    = {0};

  if (!gfx_getFontInfo(resources, font6pt, &info)) {
    return;
  }

  if (!station->hasVisibility || station->visibility < 0) {
    strncpy_safe(buf, COUNTOF(buf), "---");
  } else if (station->visibility < 2) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "Vis %.1fsm", station->visibility);
  } else {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "Vis %.0fsm", station->visibility);
  }

  bottomLeft.coord.x = 172.0f;
  bottomLeft.coord.y = LOWER_DIV + 10.0f + (info.capHeight * 3.0f) + (info.leading * 2.0f);
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);

  if (station->hasTemp && station->hasDewPoint) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "%.0f\x01/%.0f\x01\x43", station->temp, station->dewPoint);
  } else if (station->hasTemp) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "%.0f\x01\x43/---", station->temp);
  } else if (station->hasDewPoint) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "---/%.0f\x01\x43", station->dewPoint);
  } else {
    strncpy_safe(buf, COUNTOF(buf), "---/---");
  }

  bottomLeft.coord.x = 5.0f;
  bottomLeft.coord.y += info.cellSize.v[1];
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);

  if (!station->hasAlt || station->alt < 0) {
    strncpy_safe(buf, COUNTOF(buf), "---");
  } else {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "%.2f\"", station->alt);
  }

  bottomLeft.coord.x = 172.0f;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gfx_White, vertAlignBaseline);
}
