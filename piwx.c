/**
 * @file piwx.c
 */
#include "conf_file.h"
#include "gfx.h"
#include "log.h"
#include "util.h"
#include "wx.h"
#include <config.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>
#if defined WITH_LED_SUPPORT
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
 * @param[in] signo Signal number raised.
 */
static void signalHandler(int signo) {
  switch (signo) {
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
 * @param[in] config The PiWx configuration to print.
 */
static void printConfiguration(const PiwxConfig *config) {
  int i;

  printf("Image Resources: %s\n", config->imageResources);
  printf("Font Resources: %s\n", config->fontResources);
  printf("Station Query: %s\n", config->stationQuery);
  printf("Nearest Airport: %s\n", config->nearestAirport);
  printf("Cycle Time: %d\n", config->cycleTime);
  printf("High-Wind Speed: %d\n", config->highWindSpeed);
  printf("LED Brightness: %d\n", config->ledBrightness);
  printf("LED Night Brightness: %d\n", config->ledNightBrightness);
  printf("LED Data Pin: %d\n", config->ledDataPin);
  printf("LED DMA Channel: %d\n", config->ledDMAChannel);
  printf("Log Level: %d\n", config->logLevel);

  for (i = 0; i < MAX_LEDS; ++i) {
    if (config->ledAssignments[i]) {
      printf("LED %d = %s\n", i + 1, config->ledAssignments[i]);
    }
  }
}

/**
 * @brief Converts cloud layer data to a string METAR-style string.
 * @param[in] sky Cloud layer information.
 * @param[in] buf Buffer to receive the string.
 * @param[in] len Size of the string buffer.
 */
static void layerToString(const SkyCondition *sky, char *buf, size_t len) {
  const char *cover = NULL;

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

  if (cover) {
    snprintf(buf, len, "%s %d", cover, sky->height);
  } else {
    snprintf(buf, len, "--- %d", sky->height);
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
 * @param[in] resources The structure to receive the resources.
 * @returns TRUE if successful, FALSE if otherwise.
 */
static boolean allocateDrawResources(DrawResources *resources) {
  resources->sfc    = allocateSurface(320, 240);
  resources->font16 = allocateFont(font_16pt);
  resources->font8  = allocateFont(font_8pt);
  resources->font6  = allocateFont(font_6pt);
  resources->dlIcon = allocateBitmap("downloading.png");
  resources->dlErr  = allocateBitmap("download_err.png");

  if (!resources->sfc || !resources->font16 || !resources->font8 ||
      !resources->font6 || !resources->dlIcon || !resources->dlErr) {
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Frees the common drawing resources.
 * @param[in] resources The common resources to free.
 */
static void freeDrawResources(DrawResources *resources) {
  freeSurface(resources->sfc);
  freeFont(resources->font16);
  freeFont(resources->font8);
  freeFont(resources->font6);
  freeBitmap(resources->dlIcon);
  freeBitmap(resources->dlErr);
}

/**
 * @brief Clears the screen.
 */
static void clearScreen(DrawResources *resources) {
  clearSurface(resources->sfc);
  commitSurface(resources->sfc);
}

/**
 * @brief Draw the downloading status screen.
 * @param[in] resources The common resources for drawing.
 */
static void drawDownloadScreen(DrawResources *resources) {
  clearSurface(resources->sfc);
  drawBitmapInBox(resources->sfc, resources->dlIcon, 0, 0, 320, 240);
  commitSurface(resources->sfc);
}

/**
 * @brief Draw the error status screen.
 * @param[in] resources The common resources for drawing.
 * @param[in] err       The last error code received.
 * @param[in] attempt   The number of attempts performed.
 */
static void drawErrorScreen(DrawResources *resources, int err, int attempt) {
  char buf[33];
  int  len;

  clearSurface(resources->sfc);

  setTextColor(resources->font6, &rgbWhite);
  len = snprintf(buf, COUNTOF(buf), "Error %d, Retry %d", err, attempt);
  drawText(resources->sfc, resources->font6, 0, 0, buf, len);

  drawBitmapInBox(resources->sfc, resources->dlErr, 0, 0, 320, 240);
  commitSurface(resources->sfc);
}

/**
 * @brief   Loads the icon for a dominant weather phenomenon.
 * @param[in] wx The weather phenomenon.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getWeatherIcon(DominantWeather wx) {
  switch (wx) {
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
 * @param[in] cat The station flight category.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getFlightCategoryIcon(FlightCategory cat) {
  switch (cat) {
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
 * @param[in] dir The wind direction.
 * @returns The icon bitmap or null if no icon available or the icon fails to
 *          load.
 */
static Bitmap getWindIcon(int dir) {
  switch ((dir + 15) / 30 * 30) {
  case 0:
    // If the explicit wind direction is zero, this means winds calm. However,
    // if the wind direction is 1-14, the sector will still be centered on 0,
    // so handle both cases here.
    if (dir == 0) {
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
 * @param[in] resources The common drawing resources.
 * @param[in] station   The weather station.
 */
static void drawWindText(DrawResources *resources, WxStation *station) {
  char buf[33];

  setTextColor(resources->font6, &rgbWhite);

  if (station->windDir > 0) {
    snprintf(buf, COUNTOF(buf), "%d\x01", station->windDir);
  } else {
    if (station->windSpeed == 0) {
      strncpy(buf, "Calm", COUNTOF(buf));
    } else {
      strncpy(buf, "Var", COUNTOF(buf));
    }
  }

  drawText(resources->sfc, resources->font6, 84, 126, buf, strlen(buf));

  if (station->windSpeed == 0) {
    strncpy(buf, "---", COUNTOF(buf));
  } else {
    snprintf(buf, COUNTOF(buf), "%dkt", station->windSpeed);
  }

  drawText(resources->sfc, resources->font6, 84, 149, buf, strlen(buf));

  setTextColor(resources->font6, &rgbRed);

  if (station->windGust == 0) {
    strncpy(buf, "---", COUNTOF(buf));
  } else {
    snprintf(buf, COUNTOF(buf), "%dkt", station->windGust);
  }

  drawText(resources->sfc, resources->font6, 84, 172, buf, strlen(buf));
}

/**
 * @brief   Draws the cloud layers present at a station.
 * @details Draws layer information for the lowest ceiling and the next highest
 *          cloud layer, or, if there is no ceiling, the lowest and next highest
 *          cloud layers.
 * @param[in] resources The common drawing resources.
 * @param[in] station   The weather station.
 */
static void drawCloudLayers(DrawResources *resources, WxStation *station) {
  SkyCondition *sky = station->layers;
  char          buf[33];

  if (!sky) {
    return;
  }

  setTextColor(resources->font6, &rgbWhite);

  switch (sky->coverage) {
  case skyClear:
    strncpy(buf, "Clear", COUNTOF(buf));
    drawText(resources->sfc, resources->font6, 172, 126, buf, strlen(buf));
    break;
  case skyOvercastSurface:
    snprintf(buf, COUNTOF(buf), "VV %d", station->vertVis);
    drawText(resources->sfc, resources->font6, 172, 126, buf, strlen(buf));
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
      sky = station->layers;
    }

    layerToString(sky, buf, COUNTOF(buf));
    drawText(resources->sfc, resources->font6, 172, sky->next ? 149 : 126, buf,
             strlen(buf));

    // Draw the next highest layer if there is one.
    if (sky->next) {
      layerToString(sky->next, buf, COUNTOF(buf));
      drawText(resources->sfc, resources->font6, 172, 126, buf, strlen(buf));
    }

    break;
  }
}

/**
 * @brief Draws the temperature, dewpoint, and visibility information.
 * @param[in] resources The common drawing resources.
 * @param[in] station   The weather station.
 */
static void drawTempDewPointVis(DrawResources *resources, WxStation *station) {
  char buf[33];

  setTextColor(resources->font6, &rgbWhite);

  snprintf(buf, COUNTOF(buf), "%dsm vis", station->visibility);
  drawText(resources->sfc, resources->font6, 172, 172, buf, strlen(buf));

  snprintf(buf, COUNTOF(buf), "%.0f\x01/%.0f\x01\x43", station->temp,
           station->dewPoint);
  drawText(resources->sfc, resources->font6, 0, 206, buf, strlen(buf));

  snprintf(buf, COUNTOF(buf), "%.2f\"", station->alt);
  drawText(resources->sfc, resources->font6, 172, 206, buf, strlen(buf));
}

/**
 * @brief Draws a station information screen.
 * @param[in] resources The common drawing resources.
 * @param[in] station   The weather station.
 */
static void drawStation(DrawResources *resources, WxStation *station) {
  Bitmap icon = NULL;
  char * str;
  int    w, x;
  size_t len;

  clearSurface(resources->sfc);
  setTextColor(resources->font16, &rgbWhite);
  setTextColor(resources->font8, &rgbWhite);
  setTextColor(resources->font6, &rgbWhite);

  // Draw the screen layout.
  icon = allocateBitmap("separators.png");

  if (icon) {
    drawBitmapInBox(resources->sfc, icon, 0, 0, 320, 240);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the local or ICAO airport identifier.
  str = station->localId;

  if (!str) {
    str = station->id;
  }

  drawText(resources->sfc, resources->font16, 0, 0, str, strlen(str));

  // Draw the dominant weather phenomenon icon.
  icon = getWeatherIcon(station->wx);

  if (icon) {
    drawBitmapInBox(resources->sfc, icon, 236, 0, 320, 81);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the flight category icon.
  icon = getFlightCategoryIcon(station->cat);

  if (icon) {
    drawBitmapInBox(resources->sfc, icon, 174, 0, 236, 81);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the weather phenomena string.
  if (station->wxString) {
    len = strlen(station->wxString);
    w   = getFontCharWidth(resources->font8);
    x   = len * w;
    x   = (320 - x) / 2;
    drawText(resources->sfc, resources->font8, x < 0 ? 0 : x, 81,
             station->wxString, len);
  }

  // Draw the wind sock icon.
  icon = getWindIcon(station->windDir);

  if (icon) {
    drawBitmap(resources->sfc, icon, 10, 132);
    freeBitmap(icon);
    icon = NULL;
  }

  // Draw the wind information.
  drawWindText(resources, station);

  // Draw the cloud layers.
  drawCloudLayers(resources, station);

  // Draw temperature, dewpoint, and visibility.
  drawTempDewPointVis(resources, station);

  // Update the screen.
  commitSurface(resources->sfc);
}

/**
 * @brief   The program loop.
 * @details Test mode performs the weather query, then writes the first screen
 *          update to a PNG file before exiting.
 * @param[in] test    Run a test.
 * @param[in] verbose Output extra debug information.
 * @returns 0 if successful, non-zero otherwise.
 */
static int go(boolean test, boolean verbose) {
  PiwxConfig *  cfg = getPiwxConfig();
  DrawResources drawRes;
  WxStation *   wx = NULL, *curStation = NULL;
  time_t        nextUpdate = 0, nextBlink = 0, nextWx = 0, now;
  boolean       first = TRUE, draw;
  int           i, err;
  unsigned int  b, bl = 0, bc, retry = 0;

  if (verbose) {
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
#if defined WITH_LED_SUPPORT
        updateLEDs(cfg, wx);
#endif
      } else {
        drawErrorScreen(&drawRes, err, ++retry);
#if defined WITH_LED_SUPPORT
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
#if defined WITH_LED_SUPPORT
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

    if (test) {
      writeSurfaceToPNG(drawRes.sfc, "test.png");
      run = 0;
    }
  } while (run);

  writeLog(LOG_INFO, "Shutting down.");

  // Clear the screen, turn off the LEDs, and free common draw resources.
  clearScreen(&drawRes);
#if defined WITH_LED_SUPPORT
  updateLEDs(cfg, NULL);
#endif
  freeDrawResources(&drawRes);

  closeLog();

  return 0;
}

/**
 * @brief The C-program entry point we all know and love.
 */
int main(int argc, char *argv[]) {
  pid_t   pid, sid;
  int     c;
  boolean t = FALSE, standAlone = FALSE, verbose = FALSE;

  // Parse the command line parameters.
  while ((c = getopt_long(argc, argv, shortArgs, longArgs, 0)) != -1) {
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
