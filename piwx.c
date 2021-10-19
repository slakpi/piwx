/**
 * @file piwx.c
 */
#include "conf_file.h"
#include "config.h"
#include "gfx.h"
#include "log.h"
#include "util.h"
#include "wx.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>
#ifdef WITH_LED_SUPPORT
#include "led.h"
#endif

#define BUTTONS  4
#define BUTTON_1 0x1
#define BUTTON_2 0x2
#define BUTTON_3 0x4
#define BUTTON_4 0x8

static const int   buttonPins[] = {17, 22, 23, 27};
static const char *shortArgs    = "stVv";
// clang-format off
static const struct option longArgs[] = {
  { "stand-alone", no_argument,       0, 's' },
  { "test",        no_argument,       0, 't' },
  { "verbose",     no_argument,       0, 'V' },
  { "version",     no_argument,       0, 'v' },
  { 0,             0,                 0,  0  }
};
// clang-format on

static int run = 1;

/**
 * @brief Signal handler callback.
 * @param[in]: _signo Signal number raised.
 */
static void signalHandler(int _signo) {
  switch (_signo) {
  case SIGINT:
  case SIGTERM:
  case SIGHUP:
    run = 0;
    break;
  }
}

/**
 * @brief Scans the buttons on the PiTFT.
 * @returns Bitmask of pressed buttons.
 */
static unsigned int scanButtons() {
  int          i;
  unsigned int buttons = 0;

  for (i = 0; i < BUTTONS; ++i) {
    if (digitalRead(buttonPins[i]) == LOW)
      buttons |= (1 << i);
  }

  return buttons;
}

/**
 * @brief Print the configuration values.
 * @param[in]: _config The PiWx configuration to print.
 */
static void printConfiguration(const PiwxConfig *_config) {
  int i;

  printf("Image Resources: %s\n", _config->imageResources);
  printf("Font Resources: %s\n", _config->fontResources);
  printf("Station Query: %s\n", _config->stationQuery);
  printf("Nearest Airport: %s\n", _config->nearestAirport);
  printf("Cycle Time: %d\n", _config->cycleTime);
  printf("High-Wind Speed: %d\n", _config->highWindSpeed);
  printf("LED Brightness: %d\n", _config->ledBrightness);
  printf("LED Night Brightness: %d\n", _config->ledNightBrightness);
  printf("LED Data Pin: %d\n", _config->ledDataPin);
  printf("LED DMA Channel: %d\n", _config->ledDMAChannel);
  printf("Log Level: %d\n", _config->logLevel);

  for (i = 0; i < MAX_LEDS; ++i) {
    if (_config->ledAssignments[i]) {
      printf("LED %d = %s\n", i + 1, _config->ledAssignments[i]);
    }
  }
}

/**
 * @brief Converts cloud layer data to a string METAR-style string.
 * @param[in]: _sky Cloud layer information.
 * @param[in]: _buf Buffer to receive the string.
 * @param[in]: _len Size of the string buffer.
 */
static void layerToString(const SkyCondition *_sky, char *_buf, size_t _len) {
  const char *cover = NULL;

  switch (_sky->coverage) {
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

  if (cover) {
    snprintf(_buf, _len, "%s %d", cover, _sky->height);
  } else {
    snprintf(_buf, _len, "--- %d", _sky->height);
  }
}

/**
 * @struct DrawResources
 * @brief Common drawing resources.
 */
typedef struct {
  Surface sfc;
  Font    font16;
  Font    font8;
  Font    font6;
  Bitmap  dlIcon;
  Bitmap  dlErr;
} DrawResources;

/**
 * @brief   Allocate common drawing resources.
 * @param[in]: _resources The structure to receive the resources.
 * @returns TRUE if successful, FALSE if otherwise.
 */
static boolean allocateDrawResources(DrawResources *_resources) {
  _resources->sfc    = allocateSurface(320, 240);
  _resources->font16 = allocateFont(font_16pt);
  _resources->font8  = allocateFont(font_8pt);
  _resources->font6  = allocateFont(font_6pt);
  _resources->dlIcon = allocateBitmap("downloading.png");
  _resources->dlErr  = allocateBitmap("download_err.png");

  if (!_resources->sfc || !_resources->font16 || !_resources->font8 ||
      !_resources->font6 || !_resources->dlIcon || !_resources->dlErr) {
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Frees the common drawing resources.
 * @param[in]: _resources The common resources to free.
 */
static void freeDrawResources(DrawResources *_resources) {
  freeSurface(_resources->sfc);
  freeFont(_resources->font16);
  freeFont(_resources->font8);
  freeFont(_resources->font6);
  freeBitmap(_resources->dlIcon);
  freeBitmap(_resources->dlErr);
}

/**
 * @brief Clears the screen.
 */
static void clearScreen(DrawResources *_resources) {
  clearSurface(_resources->sfc);
  commitSurface(_resources->sfc);
}

/**
 * @brief Draw the downloading status screen.
 * @param[in]: _resources The common resources for drawing.
 */
static void drawDownloadScreen(DrawResources *_resources) {
  clearSurface(_resources->sfc);
  drawBitmapInBox(_resources->sfc, _resources->dlIcon, 0, 0, 320, 240);
  commitSurface(_resources->sfc);
}

/**
 * @brief Draw the error status screen.
 * @param[in]: _resources The common resources for drawing.
 * @param[in]: _err       The last error code received.
 * @param[in]: _attempt   The number of attempts performed.
 */
static void drawErrorScreen(DrawResources *_resources, int _err, int _attempt) {
  char buf[33];
  int  len;

  clearSurface(_resources->sfc);

  setTextColor(_resources->font6, &rgbWhite);
  len = snprintf(buf, _countof(buf), "Error %d, Retry %d", _err, _attempt);
  drawText(_resources->sfc, _resources->font6, 0, 0, buf, len);

  drawBitmapInBox(_resources->sfc, _resources->dlErr, 0, 0, 320, 240);
  commitSurface(_resources->sfc);
}

/**
 * @brief   Loads the icon for a dominant weather phenomenon.
 * @param[in]: _wx The weather phenomenon.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getWeatherIcon(DominantWeather _wx) {
  switch (_wx) {
  case wxClearDay:
    return allocateBitmap("wx_clear_day.png");
  case wxClearNight:
    return allocateBitmap("wx_clear_night.png");
  case wxScatteredOrFewDay:
    return allocateBitmap("wx_few_day.png");
  case wxScatteredOrFewNight:
    return allocateBitmap("wx_few_night.png");
  case wxBrokenDay:
    return allocateBitmap("wx_broken_day.png");
  case wxBrokenNight:
    return allocateBitmap("wx_broken_night.png");
  case wxOvercast:
    return allocateBitmap("wx_overcast.png");
  case wxLightMistHaze:
  case wxObscuration:
    return allocateBitmap("wx_fog_haze.png");
  case wxLightDrizzleRain:
    return allocateBitmap("wx_chance_rain.png");
  case wxRain:
    return allocateBitmap("wx_rain.png");
  case wxFlurries:
    return allocateBitmap("wx_flurries.png");
  case wxLightSnow:
    return allocateBitmap("wx_chance_snow.png");
  case wxSnow:
    return allocateBitmap("wx_snow.png");
  case wxLightFreezingRain:
    return allocateBitmap("wx_chance_fzra.png");
  case wxFreezingRain:
    return allocateBitmap("wx_fzra.png");
  case wxVolcanicAsh:
    return allocateBitmap("wx_volcanic_ash.png");
  case wxLightTstormsSqualls:
    return allocateBitmap("wx_chance_ts.png");
  case wxTstormsSqualls:
    return allocateBitmap("wx_thunderstorms.png");
  case wxFunnelCloud:
    return allocateBitmap("wx_funnel_cloud.png");
  default:
    return NULL;
  }
}

/**
 * @brief   Loads the icon for a flight category.
 * @param[in]: _cat The station flight category.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getFlightCategoryIcon(FlightCategory _cat) {
  switch (_cat) {
  case catLIFR:
    return allocateBitmap("cat_lifr.png");
  case catIFR:
    return allocateBitmap("cat_ifr.png");
  case catMVFR:
    return allocateBitmap("cat_mvfr.png");
  case catVFR:
    return allocateBitmap("cat_vfr.png");
  default:
    return NULL;
  }
}

/**
 * @brief   Loads the icon for a wind direction.
 * @param[in]: _dir The wind direction.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getWindIcon(int _dir) {
  switch ((_dir + 15) / 30 * 30) {
  case 0:
    // If the explicit wind direction is zero, this means winds calm. However,
    // if the wind direction is 1-14, the sector will still be centered on 0,
    // so handle both cases here.
    if (_dir == 0) {
      return allocateBitmap("wind_calm.png");
    } else {
      return allocateBitmap("wind_360.png");
    }
  case 30:
    return allocateBitmap("wind_30.png");
  case 60:
    return allocateBitmap("wind_60.png");
  case 90:
    return allocateBitmap("wind_90.png");
  case 120:
    return allocateBitmap("wind_120.png");
  case 150:
    return allocateBitmap("wind_150.png");
  case 180:
    return allocateBitmap("wind_180.png");
  case 210:
    return allocateBitmap("wind_210.png");
  case 240:
    return allocateBitmap("wind_240.png");
  case 270:
    return allocateBitmap("wind_270.png");
  case 300:
    return allocateBitmap("wind_300.png");
  case 330:
    return allocateBitmap("wind_330.png");
  case 360:
    return allocateBitmap("wind_360.png");
  default:
    return NULL;
  }
}

/**
 * @brief Draws the wind information text.
 * @param[in]: _resources The common drawing resources.
 * @param[in]: _station   The weather station.
 */
static void drawWindText(DrawResources *_resources, WxStation *_station) {
  char buf[33];

  setTextColor(_resources->font6, &rgbWhite);

  if (_station->windDir > 0) {
    snprintf(buf, _countof(buf), "%d\x01", _station->windDir);
  } else {
    if (_station->windSpeed == 0) {
      strncpy(buf, "Calm", _countof(buf));
    } else {
      strncpy(buf, "Var", _countof(buf));
    }
  }

  drawText(_resources->sfc, _resources->font6, 84, 126, buf, strlen(buf));

  if (_station->windSpeed == 0) {
    strncpy(buf, "---", _countof(buf));
  } else {
    snprintf(buf, _countof(buf), "%dkt", _station->windSpeed);
  }

  drawText(_resources->sfc, _resources->font6, 84, 149, buf, strlen(buf));

  setTextColor(_resources->font6, &rgbRed);

  if (_station->windGust == 0) {
    strncpy(buf, "---", _countof(buf));
  } else {
    snprintf(buf, _countof(buf), "%dkt", _station->windGust);
  }

  drawText(_resources->sfc, _resources->font6, 84, 172, buf, strlen(buf));
}

/**
 * @brief   Draws the cloud layers present at a station.
 * @details Draws layer information for the lowest ceiling and the next highest
 *          cloud layer, or, if there is no ceiling, the lowest and next highest
 *          cloud layers.
 * @param[in]: _resources The common drawing resources.
 * @param[in]: _station   The weather station.
 */
static void drawCloudLayers(DrawResources *_resources, WxStation *_station) {
  SkyCondition *sky = _station->layers;
  char          buf[33];

  setTextColor(_resources->font6, &rgbWhite);

  switch (sky->coverage) {
  case skyClear:
    strncpy(buf, "Clear", _countof(buf));
    drawText(_resources->sfc, _resources->font6, 172, 126, buf, strlen(buf));
    break;
  case skyOvercastSurface:
    snprintf(buf, _countof(buf), "VV %d", _station->vertVis);
    drawText(_resources->sfc, _resources->font6, 172, 126, buf, strlen(buf));
    break;
  default:
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
      sky = _station->layers;
    }

    layerToString(sky, buf, _countof(buf));
    drawText(_resources->sfc, _resources->font6, 172, sky->next ? 149 : 126,
             buf, strlen(buf));

    // Draw the next highest layer if there is one.
    if (sky->next) {
      layerToString(sky->next, buf, _countof(buf));
      drawText(_resources->sfc, _resources->font6, 172, 126, buf, strlen(buf));
    }

    break;
  }
}

/**
 * @brief Draws the temperature, dewpoint, and visibility information.
 * @param[in]: _resources The common drawing resources.
 * @param[in]: _station   The weather station.
 */
static void drawTempDewPointVis(DrawResources *_resources,
                                WxStation *    _station) {
  char buf[33];

  setTextColor(_resources->font6, &rgbWhite);

  snprintf(buf, _countof(buf), "%dsm vis", _station->visibility);
  drawText(_resources->sfc, _resources->font6, 172, 172, buf, strlen(buf));

  snprintf(buf, _countof(buf), "%.0f\x01/%.0f\x01\x43", _station->temp,
           _station->dewPoint);
  drawText(_resources->sfc, _resources->font6, 0, 206, buf, strlen(buf));

  snprintf(buf, _countof(buf), "%.2f\"", _station->alt);
  drawText(_resources->sfc, _resources->font6, 172, 206, buf, strlen(buf));
}

/**
 * @brief Draws a station information screen.
 * @param[in]: _resources The common drawing resources.
 * @param[in]: _station   The weather station.
 */
static void drawStation(DrawResources *_resources, WxStation *_station) {
  Bitmap icon = NULL;
  char * str;
  int    w, x;
  size_t len;

  clearSurface(_resources->sfc);
  setTextColor(_resources->font16, &rgbWhite);
  setTextColor(_resources->font8, &rgbWhite);
  setTextColor(_resources->font6, &rgbWhite);

  // Draw the screen layout.
  icon = allocateBitmap("separators.png");

  if (icon) {
    drawBitmapInBox(_resources->sfc, icon, 0, 0, 320, 240);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the local or ICAO airport identifier.
  str = _station->localId;

  if (!str) {
    str = _station->id;
  }

  drawText(_resources->sfc, _resources->font16, 0, 0, str, strlen(str));

  // Draw the dominant weather phenomenon icon.
  icon = getWeatherIcon(_station->wx);

  if (icon) {
    drawBitmapInBox(_resources->sfc, icon, 236, 0, 320, 81);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the flight category icon.
  icon = getFlightCategoryIcon(_station->cat);

  if (icon) {
    drawBitmapInBox(_resources->sfc, icon, 174, 0, 236, 81);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the weather phenomena string.
  if (_station->wxString) {
    len = strlen(_station->wxString);
    w   = getFontCharWidth(_resources->font8);
    x   = len * w;
    x   = (320 - x) / 2;
    drawText(_resources->sfc, _resources->font8, x < 0 ? 0 : x, 81,
             _station->wxString, len);
  }

  // Draw the wind sock icon.
  icon = getWindIcon(_station->windDir);

  if (icon) {
    drawBitmap(_resources->sfc, icon, 10, 132);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the wind information.
  drawWindText(_resources, _station);

  // Draw the cloud layers.
  drawCloudLayers(_resources, _station);

  // Draw temperature, dewpoint, and visibility.
  drawTempDewPointVis(_resources, _station);

  // Update the screen.
  commitSurface(_resources->sfc);
}

/**
 * @brief   The program loop.
 * @details Test mode performs the weather query, then writes the first screen
 *          update to a PNG file before exiting.
 * @param[in]: _test    Run a test.
 * @param[in]: _verbose Output extra debug information.
 * @returns 0 if successful, non-zero otherwise.
 */
static int go(boolean _test, boolean _verbose) {
  PiwxConfig *  cfg = getPiwxConfig();
  DrawResources drawRes;
  WxStation *   wx = NULL, *curStation = NULL;
  time_t        nextUpdate = 0, nextBlink = 0, nextWx = 0, now;
  boolean       first = TRUE, draw;
  int           i, err;
  unsigned int  b, bl = 0, bc, retry = 0;

  if (_verbose) {
    printConfiguration(cfg);
  }

  if (!cfg->stationQuery) {
    return 0; // Nothing to do.
  }

  openLog(cfg->logLevel);
  writeLog(LOG_INFO, "Starting up.");

  // Setup wiringPi.
  wiringPiSetupGpio();

  for (i = 0; i < BUTTONS; ++i) {
    pinMode(buttonPins[i], INPUT);
    pullUpDnControl(buttonPins[i], PUD_UP);
  }

  // Intialize common draw resources.
  if (!allocateDrawResources(&drawRes)) {
    return -1;
  }

  do {
    draw = FALSE;

    // Scan the buttons. Mask off any buttons that were pressed on the last scan
    // and are either still pressed or were released.
    b  = scanButtons();
    bc = (~bl) & b;
    bl = b;

    now = time(0);

    // If this is the first run, the update time has expired, or someone pressed
    // the refresh button, then requery the weather data.
    if (first || now >= nextUpdate || (bc & BUTTON_1)) {
      if (first) {
        writeLog(LOG_DEBUG, "Performing startup weather query.");
      } else if (now >= nextUpdate) {
        writeLog(LOG_DEBUG, "Update time out: %lu >= %lu", now, nextUpdate);
      } else if (bc & BUTTON_1) {
        writeLog(LOG_DEBUG, "Update button pressed.");
      }

      if (wx) {
        freeStations(wx);
      }

      drawDownloadScreen(&drawRes);

      wx         = queryWx(cfg->stationQuery, &err);
      curStation = wx;
      first      = FALSE;
      draw       = (wx != NULL);
      nextUpdate = ((now / 1200) + 1) * 1200;
      nextWx     = now + 1;
      nextBlink  = 10;

      if (wx) {
        retry = 0;
#ifdef WITH_LED_SUPPORT
        updateLEDs(cfg, wx);
#endif
      } else {
        drawErrorScreen(&drawRes, err, ++retry);
#ifdef WITH_LED_SUPPORT
        updateLEDs(cfg, NULL);
#endif

        // Try again in 5 minutes.
        nextUpdate = now + 300;
      }
    }

    if (wx) {
      // Check the following:
      //   * Timeout expired? Move forward in the circular list.
      //   * Button 3 pressed? Move forward in the circular list.
      //   * Button 2 pressed? Move backward in the circular list.
      if (now >= nextWx || (bc & BUTTON_3)) {
        curStation = curStation->next;
        draw       = TRUE;
        nextWx     = now + cfg->cycleTime;
      } else if (bc & BUTTON_2) {
        curStation = curStation->prev;
        draw       = TRUE;
        nextWx     = now + cfg->cycleTime;
      }

      // If the blink timeout expired, update the LEDs.
      if (cfg->highWindSpeed > 0 && cfg->highWindBlink != 0 &&
          now > nextBlink) {
#ifdef WITH_LED_SUPPORT
        updateLEDs(cfg, wx);
#endif
        nextBlink = now + 1;
      }
    }

    // Nothing to do, sleep.
    if (!draw) {
      usleep(50000);
      continue;
    }

    drawStation(&drawRes, curStation);

    if (_test) {
      writeSurfaceToPNG(drawRes.sfc, "test.png");
      run = 0;
    }
  } while (run);

  writeLog(LOG_INFO, "Shutting down.");

  // Clear the screen, turn off the LEDs, and free common draw resources.
  clearScreen(&drawRes);
#ifdef WITH_LED_SUPPORT
  updateLEDs(cfg, NULL);
#endif
  freeDrawResources(&drawRes);

  closeLog();

  return 0;
}

/**
 * @brief The C-program entry point we all know and love.
 */
int main(int _argc, char *_argv[]) {
  pid_t   pid, sid;
  int     c;
  boolean t = FALSE, standAlone = FALSE, verbose = FALSE;

  // Parse the command line parameters.
  while ((c = getopt_long(_argc, _argv, shortArgs, longArgs, 0)) != -1) {
    switch (c) {
    case 's':
      standAlone = TRUE;
      break;
    case 't':
      standAlone = TRUE;
      t          = TRUE;
      break;
    case 'V':
      verbose = TRUE;
      break;
    case 'v':
      printf("piwx (%s)\n", GIT_COMMIT_HASH);
      return 0;
    }
  }

  // If running in standalone mode, setup the signal handlers and run.
  if (standAlone) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);
    return go(t, verbose);
  }

  // If not running in standalone mode, fork the process and setup as a daemon.
  pid = fork();

  if (pid < 0) {
    return -1; // Failed to fork.
  }

  if (pid > 0) {
    return 0; // Exit the parent process.
  }

  umask(0);

  sid = setsid();

  if (sid < 0) {
    return -1;
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGHUP, signalHandler);

  return go(0, 0);
}
