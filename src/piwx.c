/**
 * @file piwx.c
 */
#include "conf_file.h"
#include "config.h"
#include "gfx.h"
#include "led.h"
#include "log.h"
#include "util.h"
#include "wx.h"
#include <getopt.h>
#include <math.h>
#include <pigpio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BUTTON_1 0x1
#define BUTTON_2 0x2
#define BUTTON_3 0x4
#define BUTTON_4 0x8

#define NO_UPDATE    0x0
#define UPDATE_BLINK 0x1
#define UPDATE_NIGHT 0x2

#define WX_UPDATE_INTERVAL_SEC 1200
#define WX_RETRY_INTERVAL_SEC  300
#define SLEEP_INTERVAL_USEC    50000
#define BLINK_INTERVAL_SEC     1
#define NIGHT_INTERVAL_SEC     120

#define MIX_BRIGHTNESS(c, b) ((((uint16_t)(c) << 8) * (b)) >> 8)

static const LEDColor gColorVFR     = {0, 255, 0};
static const LEDColor gColorMVFR    = {0, 0, 255};
static const LEDColor gColorIFR     = {255, 0, 0};
static const LEDColor gColorLIFR    = {255, 0, 255};
static const LEDColor gColorWind    = {255, 192, 0};
static const LEDColor gColorUnk     = {64, 64, 64};
static const Color4f  gClearColor   = {{1.0f, 1.0f, 1.0f, 0.0f}};
static const Color4f  gWhite        = {{1.0f, 1.0f, 1.0f, 1.0f}};
static const Color4f  gRed          = {{1.0f, 0.0f, 0.0f, 1.0f}};
static const float    gUpperDiv     = 81.0f;
static const float    gLowerDiv     = 122.0f;
static const int      gButtonPins[] = {17, 22, 23, 27};
static const char    *gShortArgs    = "tVv";
// clang-format off
static const struct option gLongArgs[] = {
  { "test",        no_argument,       0, 't' },
  { "verbose",     no_argument,       0, 'V' },
  { "version",     no_argument,       0, 'v' },
  { 0,             0,                 0,  0  }
};
// clang-format on

static bool gRun;

static void drawBackground(DrawResources resources);

static void drawCloudLayers(DrawResources *resources, const WxStation *station);

static void drawDownloadErrorScreen(DrawResources resources, bool commit);

static void drawDownloadScreen(DrawResources resources, bool commit);

static void drawStationIdentifier(DrawResources resources, const WxStation *station);

static void drawStationFlightCategory(DrawResources resources, const WxStation *station);

static void drawStationScreen(DrawResources resources, const WxStation *station, bool commit);

static void drawStationWeather(DrawResources resources, const WxStation *station);

static void drawStationWxString(DrawResources resources, const WxStation *station);

static void drawTempDewPointVisAlt(DrawResources *resources, const WxStation *station);

static void drawWindInfo(DrawResources *resources, const WxStation *station);

static void getCloudLayerText(const SkyCondition *sky, char *buf, size_t len);

static Icon getFlightCategoryIcon(FlightCategory cat);

static LEDColor getLEDColor(const PiwxConfig *cfg, const WxStation *station);

static Icon getWeatherIcon(DominantWeather wx);

static Icon getWindIcon(int direction);

static void getWindDirectionText(const WxStation *station, char *buf, size_t len);

static void getWindSpeedText(const WxStation *station, bool gust, char *buf, size_t len);

static bool go(bool test, bool verbose);

static void printConfiguration(const PiwxConfig *config);

static unsigned int scanButtons();

static int setupGpio();

static void signalHandler(int signo);

static void updateLEDs(const PiwxConfig *cfg, const WxStation *stations);

static bool updateStation(const PiwxConfig *cfg, WxStation *station, uint32_t update, time_t now);

static bool updateStations(const PiwxConfig *cfg, WxStation *stations, uint32_t update, time_t now);

/**
 * @brief The C-program entry point we all know and love.
 */
int main(int argc, char *argv[]) {
  int  c;
  bool test = false, verbose = false;

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // Parse the command line parameters.
  while ((c = getopt_long(argc, argv, gShortArgs, gLongArgs, 0)) != -1) {
    switch (c) {
    case 't':
      test = true;
      break;
    case 'V':
      verbose = true;
      break;
    case 'v':
      printf("piwx %s (%s)\n", RELEASE, GIT_COMMIT_HASH);
      return 0;
    }
  }

  gRun = true;

  return go(test, verbose) ? 0 : -1;
}

/**
 * @brief Signal handler callback.
 * @param[in] signo Signal number raised.
 */
static void signalHandler(int signo) {
  switch (signo) {
  case SIGINT:
  case SIGTERM:
  case SIGHUP:
    gRun = false;
    break;
  }
}

/**
 * @brief   The program loop.
 * @details Test mode performs the weather query, then writes the first screen
 *          update to a PNG file before exiting.
 * @param[in] test    Run a test.
 * @param[in] verbose Output extra debug information.
 * @returns true if successful, false otherwise.
 */
static bool go(bool test, bool verbose) {
  PiwxConfig *cfg =
      conf_getPiwxConfig(INSTALL_PREFIX, IMAGE_RESOURCES, FONT_RESOURCES, CONFIG_FILE);
  WxStation    *wx = NULL, *curStation = NULL;
  time_t        nextUpdate = 0, nextBlink = 0, nextDayNight = 0, nextWx = 0;
  bool          first = true, ret = false;
  DrawResources resources = INVALID_RESOURCES;

  if (verbose) {
    printConfiguration(cfg);
  }

  if (!cfg->stationQuery) {
    return 0; // Nothing to do, but not an error.
  }

  openLog(LOG_FILE, cfg->logLevel);
  writeLog(logInfo, "Starting up.");

  if (setupGpio() < 0) {
    writeLog(logWarning, "Failed to initialize pigpio.\n");
    goto cleanup;
  }

  if (!gfx_initGraphics(cfg->fontResources, cfg->imageResources, &resources)) {
    writeLog(logWarning, "Failed to initialize graphics.");
    goto cleanup;
  }

  do {
    bool         draw = false;
    unsigned int b = 0, bl = 0, bc;
    int          err;
    time_t       now    = time(NULL);
    int          update = NO_UPDATE;

    // Scan the buttons. Mask off any buttons that were pressed on the last scan
    // and are either still pressed or were released.
    bl = b;
    b  = scanButtons();
    bc = (~bl) & b;

    // If this is the first run, the update time has expired, or someone pressed
    // the refresh button, then requery the weather data.
    if (first || now >= nextUpdate || (bc & BUTTON_1)) {
      if (first) {
        writeLog(logDebug, "Performing startup weather query.");
      } else if (now >= nextUpdate) {
        writeLog(logDebug, "Update time out: %lu >= %lu", now, nextUpdate);
      } else if (bc & BUTTON_1) {
        writeLog(logDebug, "Update button pressed.");
      }

      wx_freeStations(wx);

      drawDownloadScreen(resources, !test);

      wx           = wx_queryWx(cfg->stationQuery, cfg->daylight, &err);
      curStation   = wx;
      first        = false;
      draw         = (wx != NULL);
      nextUpdate   = ((now / WX_UPDATE_INTERVAL_SEC) + 1) * WX_UPDATE_INTERVAL_SEC;
      nextWx       = now + cfg->cycleTime;
      nextBlink    = now + BLINK_INTERVAL_SEC;
      nextDayNight = now + NIGHT_INTERVAL_SEC;

      if (wx) {
        updateLEDs(cfg, wx);
      } else {
        drawDownloadErrorScreen(resources, !test);
        updateLEDs(cfg, NULL);

        // Try again at the retry interval rather than on the update interval
        // boundary.
        nextUpdate = now + WX_RETRY_INTERVAL_SEC;

        if (test) {
          break;
        }
      }
    }

    if (wx) {
      // Check the following:
      //   * Timeout expired? Move forward in the circular list.
      //   * Button 3 pressed? Move forward in the circular list.
      //   * Button 2 pressed? Move backward in the circular list.
      if (now >= nextWx || (bc & BUTTON_3)) {
        curStation = curStation->next;
        draw       = true;
        nextWx     = now + cfg->cycleTime;
      } else if (bc & BUTTON_2) {
        curStation = curStation->prev;
        draw       = true;
        nextWx     = now + cfg->cycleTime;
      }

      // If the blink timeout expired, update the LEDs.
      if (now > nextBlink) {
        update |= UPDATE_BLINK;
        nextBlink = now + BLINK_INTERVAL_SEC;
      }

      if (now > nextDayNight) {
        update |= UPDATE_NIGHT;
        nextDayNight = now + NIGHT_INTERVAL_SEC;
      }

      if (updateStations(cfg, wx, update, now)) {
        updateLEDs(cfg, wx);
      }
    }

    // Nothing to do, sleep.
    if (!draw) {
      usleep(SLEEP_INTERVAL_USEC);
      continue;
    }

    drawStationScreen(resources, curStation, !test);

    if (test) {
      gfx_dumpSurfaceToPng(resources, "test.png");
      break;
    }
  } while (gRun);

  ret = true;

cleanup:
  writeLog(logInfo, "Shutting down.");

  if (!test) {
    gfx_clearSurface(resources, gClearColor);
    gfx_commitToScreen(resources);
  }

  updateLEDs(cfg, NULL);
  gfx_cleanupGraphics(&resources);
  wx_freeStations(wx);
  gpioTerminate();
  closeLog();

  return ret;
}

/**
 * @brief Print the configuration values.
 * @param[in] config The PiWx configuration to print.
 */
static void printConfiguration(const PiwxConfig *config) {
  printf("Image Resources: %s\n", config->imageResources);
  printf("Font Resources: %s\n", config->fontResources);
  printf("Station Query: %s\n", config->stationQuery);
  printf("Cycle Time: %d\n", config->cycleTime);
  printf("High-Wind Speed: %d\n", config->highWindSpeed);
  printf("LED Brightness: %d\n", config->ledBrightness);
  printf("LED Night Brightness: %d\n", config->ledNightBrightness);
  printf("LED Data Pin: %d\n", config->ledDataPin);
  printf("LED DMA Channel: %d\n", config->ledDMAChannel);
  printf("Log Level: %d\n", config->logLevel);

  for (int i = 0; i < MAX_LEDS; ++i) {
    if (config->ledAssignments[i]) {
      printf("LED %d = %s\n", i + 1, config->ledAssignments[i]);
    }
  }
}

/**
 * @brief   Initializes the pigpio library.
 * @returns The result of @a gpioInitialise.
 */
static int setupGpio() {
  int ret, cfg;

  // Turn off internal signal handling so that the library does not force an
  // exit before we can cleanup.
  cfg = gpioCfgGetInternals();
  cfg |= PI_CFG_NOSIGHANDLER;
  gpioCfgSetInternals(cfg);

  ret = gpioInitialise();

  if (ret < 0) {
    return ret;
  }

  for (int i = 0; i < COUNTOF(gButtonPins); ++i) {
    gpioSetMode(gButtonPins[i], PI_INPUT);
    gpioSetPullUpDown(gButtonPins[i], PI_PUD_UP);
  }

  return ret;
}

/**
 * @brief Scans the buttons on the PiTFT.
 * @returns Bitmask of pressed buttons.
 */
static unsigned int scanButtons() {
  unsigned int buttons = 0;

  for (int i = 0; i < COUNTOF(gButtonPins); ++i) {
    if (gpioRead(gButtonPins[i]) == 0)
      buttons |= (1 << i);
  }

  return buttons;
}

/**
 * @brief Draws the downloading weather screen.
 * @param[in] resources The gfx context.
 * @param[in] commit    Commit the surface to the screen.
 */
static void drawDownloadScreen(DrawResources resources, bool commit) {
  Point2f center = {{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}};

  gfx_clearSurface(resources, gClearColor);
  gfx_drawIcon(resources, iconDownloading, center);

  if (commit) {
    gfx_commitToScreen(resources);
  }
}

/**
 * @brief Draws the weather download error screen.
 * @param[in] resources The gfx context.
 * @param[in] commit    Commit the surface to the screen.
 */
static void drawDownloadErrorScreen(DrawResources resources, bool commit) {
  Point2f center = {{SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}};

  gfx_clearSurface(resources, gClearColor);
  gfx_drawIcon(resources, iconDownloadErr, center);

  if (commit) {
    gfx_commitToScreen(resources);
  }
}

/**
 * @brief Draw a station's weather information.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 * @param[in] commit    Commit the surface to the screen.
 */
static void drawStationScreen(DrawResources resources, const WxStation *station, bool commit) {
  gfx_clearSurface(resources, gClearColor);

  drawBackground(resources);
  drawStationIdentifier(resources, station);
  drawStationFlightCategory(resources, station);
  drawStationWeather(resources, station);
  drawStationWxString(resources, station);
  drawCloudLayers(resources, station);
  drawWindInfo(resources, station);
  drawTempDewPointVisAlt(resources, station);

  if (commit) {
    gfx_commitToScreen(resources);
  }
}

/**
 * @brief Draw the station weather background.
 * @param[in] resources The gfx context.
 */
static void drawBackground(DrawResources resources) {
  const Point2f lines[] = {{{0.0f, gUpperDiv}},
                           {{SCREEN_WIDTH, gUpperDiv}},
                           {{0.0f, gLowerDiv}},
                           {{SCREEN_WIDTH, gLowerDiv}}};

  gfx_drawLine(resources, &lines[0], gWhite, 2.0f);
  gfx_drawLine(resources, &lines[2], gWhite, 2.0f);
}

/**
 * @brief Draw the station identifier.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 */
static void drawStationIdentifier(DrawResources resources, const WxStation *station) {
  CharInfo info       = {0};
  Point2f  bottomLeft = {0};

  if (!gfx_getFontInfo(resources, font16pt, &info)) {
    return;
  }

  bottomLeft.coord.y = info.cellSize.v[1];

  gfx_drawText(resources, font16pt, bottomLeft, station->localId, strlen(station->localId), gWhite,
               vertAlignCell);
}

/**
 * @brief Draw a station's flight category icon.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 */
static void drawStationFlightCategory(DrawResources resources, const WxStation *station) {
  const Point2f center = {{205.0f, 40.5f}};
  Icon          icon   = getFlightCategoryIcon(station->cat);
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
 * @param[in] station   The weather station information.
 */
static void drawStationWeather(DrawResources resources, const WxStation *station) {
  const Point2f center = {{278.0f, 40.5f}};
  Icon          icon   = getWeatherIcon(station->wx);
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
 * @param[in] station   The weather station information.
 */
static void drawStationWxString(DrawResources resources, const WxStation *station) {
  Point2f  bottomLeft = {0};
  CharInfo info       = {0};
  size_t   len        = 0;

  if (!station->wxString) {
    return;
  }

  if (!gfx_getFontInfo(resources, font8pt, &info)) {
    return;
  }

  len                = strlen(station->wxString);
  bottomLeft.coord.x = (SCREEN_WIDTH - (info.cellSize.v[0] * len)) / 2.0f;
  bottomLeft.coord.y = gUpperDiv + info.cellSize.v[1];
  gfx_drawText(resources, font8pt, bottomLeft, station->wxString, len, gWhite, vertAlignCell);
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
  Point2f       bottomLeft = {{172.0f, gLowerDiv + 10.0f}};
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
    strncpy_safe(buf, "Clear", COUNTOF(buf));
    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);
    return;
  case skyOvercastSurface:
    if (station->vertVis < 0) {
      strncpy_safe(buf, "VV ---", COUNTOF(buf));
    } else {
      // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
      snprintf(buf, COUNTOF(buf), "VV %d", station->vertVis);
    }

    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);
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
    gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);
    bottomLeft.coord.y += info.capHeight + info.leading;
  }

  getCloudLayerText(sky, buf, COUNTOF(buf));
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);
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
  Point2f  bottomLeft = {{84.0f, gLowerDiv + 10.0f}};
  CharInfo fontInfo   = {0};
  Vector2f iconInfo   = {0};
  Icon     icon       = getWindIcon(station->windDir);

  if (!gfx_getFontInfo(resources, font6pt, &fontInfo)) {
    return;
  }

  if (!gfx_getIconInfo(resources, icon, &iconInfo)) {
    return;
  }

  iconInfo.coord.x = 10.0f + (iconInfo.coord.x / 2.0f);
  iconInfo.coord.y = bottomLeft.coord.y + (iconInfo.coord.y / 2.0f);
  gfx_drawIcon(resources, icon, iconInfo);

  getWindDirectionText(station, buf, COUNTOF(buf));
  bottomLeft.coord.y += fontInfo.capHeight;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);

  getWindSpeedText(station, false, buf, COUNTOF(buf));
  bottomLeft.coord.y += fontInfo.capHeight + fontInfo.leading;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);

  getWindSpeedText(station, true, buf, COUNTOF(buf));
  bottomLeft.coord.y += fontInfo.capHeight + fontInfo.leading;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gRed, vertAlignBaseline);
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
 * @param[in]  station The weather station information.
 * @param[out] buf     The wind direction text.
 * @param[in]  len     Length of @a buf.
 */
static void getWindDirectionText(const WxStation *station, char *buf, size_t len) {
  if (station->windDir > 0) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, len, "%d\x01", station->windDir);
  } else if (station->windDir == 0) {
    if (station->windSpeed > 0) {
      strncpy_safe(buf, "Var", len);
    } else if (station->windSpeed == 0) {
      strncpy_safe(buf, "Calm", len);
    } else {
      strncpy_safe(buf, "---", len);
    }
  } else {
    strncpy_safe(buf, "---", len);
  }
}

/**
 * @brief Converts a wind speed to text.
 * @param[in]  station The weather station information.
 * @param[in]  gust    Convert gust speed.
 * @param[out] buf     The wind speed text.
 * @param[in]  len     Length of @a buf.
 */
static void getWindSpeedText(const WxStation *station, bool gust, char *buf, size_t len) {
  int speed = (gust ? station->windGust : station->windSpeed);

  if (speed <= 0) {
    strncpy_safe(buf, "---", len);
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

  if (station->visibility < 0) {
    strncpy_safe(buf, "---", COUNTOF(buf));
  } else if (station->visibility < 2) {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "Vis %.1fsm", station->visibility);
  } else {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "Vis %.0fsm", station->visibility);
  }

  bottomLeft.coord.x = 172.0f;
  bottomLeft.coord.y = gLowerDiv + 10.0f + (info.capHeight * 3.0f) + (info.leading * 2.0f);
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);

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
    strncpy_safe(buf, "---/---", COUNTOF(buf));
  }

  bottomLeft.coord.x = 0.0f;
  bottomLeft.coord.y += info.cellSize.v[1];
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);

  if (station->alt < 0) {
    strncpy_safe(buf, "---", COUNTOF(buf));
  } else {
    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(buf, COUNTOF(buf), "%.2f\"", station->alt);
  }

  bottomLeft.coord.x = 172.0f;
  gfx_drawText(resources, font6pt, bottomLeft, buf, strlen(buf), gWhite, vertAlignBaseline);
}

/**
 * @brief   Perform periodic updates of weather station display state.
 * @param[in] cfg      PiWx configuration.
 * @param[in] stations List of weather stations.
 * @param[in] update   The state items to update.
 * @param[in] now      The new observation time.
 * @returns True if the LED string needs to be updated.
 */
static bool updateStations(const PiwxConfig *cfg, WxStation *stations, uint32_t update,
                           time_t now) {
  WxStation *p          = stations;
  bool       updateLEDs = false;

  while (true) {
    updateLEDs |= updateStation(cfg, p, update, now);

    p = p->next;

    // Circular list.
    if (p == stations) {
      break;
    }
  }

  return updateLEDs;
}

/**
 * @brief   Update the display state of a station.
 * @param[in] cfg     PiWx configuration.
 * @param[in] station The weather station to update.
 * @param[in] update  The state items to update.
 * @param[in] now     The new observation time.
 * @returns True if the station's LED needs to be updated.
 */
static bool updateStation(const PiwxConfig *cfg, WxStation *station, uint32_t update, time_t now) {
  bool updateLED = false;

  if (update & UPDATE_NIGHT) {
    bool wasNight = station->isNight;
    wx_updateDayNightState(station, cfg->daylight, now);
    updateLED |= (wasNight != station->isNight);
  }

  if ((update & UPDATE_BLINK) && cfg->highWindSpeed > 0) {
    bool wasOn          = station->blinkState;
    bool exceeds        = max(station->windSpeed, station->windGust) >= cfg->highWindSpeed;
    station->blinkState = exceeds && ((!station->blinkState) || (cfg->highWindBlink == 0));
    updateLED |= (wasOn != station->blinkState);
  }

  return updateLED;
}

/**
 * @brief Update the LEDs assigned to weather stations.
 * @param[in] cfg      PiWx configuration.
 * @param[in] stations List of weather stations.
 */
static void updateLEDs(const PiwxConfig *cfg, const WxStation *stations) {
  LEDColor         colors[MAX_LEDS] = {0};
  const WxStation *p                = stations;

  if (!stations) {
    led_setColors(cfg->ledDataPin, cfg->ledDMAChannel, NULL, 0);
    return;
  }

  while (p) {
    for (int i = 0; i < MAX_LEDS; ++i) {
      if (!cfg->ledAssignments[i]) {
        continue;
      }

      if (strcmp(p->id, cfg->ledAssignments[i]) != 0) {
        continue;
      }

      colors[i] = getLEDColor(cfg, p);
      break;
    }

    p = p->next;

    // Circular list.
    if (p == stations) {
      break;
    }
  }

  led_setColors(cfg->ledDataPin, cfg->ledDMAChannel, colors, MAX_LEDS);
}

/**
 * @brief   Get the LED color for a weather report.
 * @param[in] cfg     PiWx configuration.
 * @param[in] station The weather station.
 * @returns The flight category color or high wind color as appropriate.
 */
static LEDColor getLEDColor(const PiwxConfig *cfg, const WxStation *station) {
  LEDColor color      = {0};
  int      brightness = station->isNight ? cfg->ledNightBrightness : cfg->ledBrightness;

  switch (station->cat) {
  case catVFR:
    color = gColorVFR;
    break;
  case catMVFR:
    color = gColorMVFR;
    break;
  case catIFR:
    color = gColorIFR;
    break;
  case catLIFR:
    color = gColorLIFR;
    break;
  default:
    color = gColorUnk;
    break;
  }

  if (station->blinkState) {
    color = gColorWind;
  }

  brightness = min(max(brightness, 0), UINT8_MAX);

  color.r = MIX_BRIGHTNESS(color.r, (uint8_t)brightness);
  color.g = MIX_BRIGHTNESS(color.g, (uint8_t)brightness);
  color.b = MIX_BRIGHTNESS(color.b, (uint8_t)brightness);

  return color;
}
