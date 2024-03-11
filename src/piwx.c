/**
 * @file piwx.c
 */
#include "anim.h"
#include "conf_file.h"
#include "config.h"
#include "display.h"
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
#define NIGHT_INTERVAL_SEC     60

#define MIX_BRIGHTNESS(c, b) (((uint16_t)(c) * (b)) >> 8)

static const LEDColor gColorVFR     = {0, 255, 0};
static const LEDColor gColorMVFR    = {0, 0, 255};
static const LEDColor gColorIFR     = {255, 0, 0};
static const LEDColor gColorLIFR    = {255, 0, 255};
static const LEDColor gColorWind    = {255, 192, 0};
static const LEDColor gColorUnk     = {64, 64, 64};
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

static LEDColor getLEDColor(const PiwxConfig *cfg, const WxStation *station);

static void globePositionUpdate(Position pos, void *param);

static bool go(bool test, bool verbose);

static void printConfiguration(const PiwxConfig *config);

static unsigned int scanButtons(void);

static void setupGlobeAnimation(Animation *anim, Position origin, Position target, float duration,
                                Position *param);

static int setupGpio(void);

static void signalHandler(int signo);

static void updateDisplay(const PiwxConfig *cfg, DrawResources resources, const WxStation *station,
                          time_t now, Position globePos, const bool updateLayers[layerCount]);

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
  DrawResources resources = GFX_INVALID_RESOURCES;
  Animation     globeAnim = NULL;
  Position      globePos;

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
    bool         updateLayers[layerCount] = {false};
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

      drawDownloadInProgress(resources);

      if (!test) {
        gfx_commitToScreen(resources);
      }

      wx           = wx_queryWx(cfg->stationQuery, cfg->daylight, now, &err);
      curStation   = wx;
      globePos     = curStation->pos;
      first        = false;
      nextUpdate   = ((now / WX_UPDATE_INTERVAL_SEC) + 1) * WX_UPDATE_INTERVAL_SEC;
      nextWx       = now + cfg->cycleTime;
      nextBlink    = now + BLINK_INTERVAL_SEC;
      nextDayNight = now + NIGHT_INTERVAL_SEC;

      for (int i = 0; i < layerCount; ++i) {
        updateLayers[i] = true;
      }

      if (!wx) {
        drawDownloadError(resources);
        gfx_commitToScreen(resources);
        updateLEDs(cfg, NULL);

        // Try again at the retry interval rather than on the update interval
        // boundary.
        nextUpdate = now + WX_RETRY_INTERVAL_SEC;

        if (test) {
          break;
        }

        continue;
      }

      updateLEDs(cfg, wx);
    }

    if (wx) {
      WxStation *lastStation = curStation;

      // Check the following:
      //   * Timeout expired? Move forward in the circular list.
      //   * Button 3 pressed? Move forward in the circular list.
      //   * Button 2 pressed? Move backward in the circular list.
      if (now >= nextWx || (bc & BUTTON_3)) {
        curStation = curStation->next;
      } else if (bc & BUTTON_2) {
        curStation = curStation->prev;
      }

      if (curStation != lastStation) {
        nextWx = now + cfg->cycleTime;

        for (int i = 0; i < layerCount; ++i) {
          updateLayers[i] = true;
        }

        globePos = lastStation->pos;
        setupGlobeAnimation(&globeAnim, lastStation->pos, curStation->pos, cfg->cycleTime * 0.5f,
                            &globePos);
      }

      updateLayers[layerBackground] |= stepAnimation(globeAnim);

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

      updateDisplay(cfg, resources, curStation, now, globePos, updateLayers);

      if (test) {
        gfx_dumpSurfaceToPng(resources, "test.png");
        break;
      }
    }

    usleep(SLEEP_INTERVAL_USEC);
  } while (gRun);

  ret = true;

cleanup:
  writeLog(logInfo, "Shutting down.");

  wx_freeStations(wx);

  clearFrame(resources);
  gfx_commitToScreen(resources);
  gfx_cleanupGraphics(&resources);
  freeAnimation(globeAnim);

  updateLEDs(cfg, NULL);

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

  for (int i = 0; i < CONF_MAX_LEDS; ++i) {
    if (config->ledAssignments[i]) {
      printf("LED %d = %s\n", i + 1, config->ledAssignments[i]);
    }
  }
}

/**
 * @brief   Initializes the pigpio library.
 * @returns The result of @a gpioInitialise.
 */
static int setupGpio(void) {
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
static unsigned int scanButtons(void) {
  unsigned int buttons = 0;

  for (int i = 0; i < COUNTOF(gButtonPins); ++i) {
    if (gpioRead(gButtonPins[i]) == 0)
      buttons |= (1 << i);
  }

  return buttons;
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
 * @brief Update the display as necessary.
 * @param[in] cfg          PiWx configuration.
 * @param[in] resources    The gfx context.
 * @param[in] station      The weather station to update.
 * @param[in] now          The new observation time.
 * @param[in] globePos     Eye position over the globe.
 * @param[in] updateLayers The display layers to update.
 */
static void updateDisplay(const PiwxConfig *cfg, DrawResources resources, const WxStation *station,
                          time_t now, Position globePos, const bool updateLayers[layerCount]) {
  bool anyUpdate = false;

  for (int i = 0; i < layerCount; ++i) {
    anyUpdate |= updateLayers[i];
  }

  if (!anyUpdate) {
    return;
  }

  clearFrame(resources);

  if (cfg->drawGlobe && updateLayers[layerBackground]) {
    gfx_beginLayer(resources, layerBackground);
    clearFrame(resources);
    drawGlobe(resources, now, globePos);
    gfx_endLayer(resources);
  }

  gfx_drawLayer(resources, layerBackground, false);

  if (updateLayers[layerForeground]) {
    gfx_beginLayer(resources, layerTempB);
    clearFrame(resources);
    drawStation(resources, now, station);
    gfx_endLayer(resources);

    gfx_beginLayer(resources, layerForeground);
    clearFrame(resources);
    gfx_drawLayer(resources, layerTempB, true);
    gfx_endLayer(resources);
  }

  gfx_drawLayer(resources, layerForeground, false);

  gfx_commitToScreen(resources);
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
  LEDColor         colors[CONF_MAX_LEDS] = {0};
  const WxStation *p                     = stations;

  if (!stations) {
    led_setColors(cfg->ledDataPin, cfg->ledDMAChannel, NULL, 0);
    return;
  }

  while (p) {
    for (int i = 0; i < CONF_MAX_LEDS; ++i) {
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

  led_setColors(cfg->ledDataPin, cfg->ledDMAChannel, colors, CONF_MAX_LEDS);
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

/**
 * @brief Create or reset a globe animation.
 * @param[in,out] anim     Pointer to an @a Animation object. If the object
 *                         itself is null, a new object is create. Otherwise,
 *                         the existing object is reset.
 * @param[in]     origin   The origin position.
 * @param[in]     target   The target position.
 * @param[in]     duration The animation duration in seconds.
 * @param[in]     param    The position to animate.
 */
static void setupGlobeAnimation(Animation *anim, Position origin, Position target, float duration,
                                Position *param) {
  unsigned int steps;

  if (*anim) {
    resetPositionAnimation(*anim, origin, target);
    return;
  }

  steps = (unsigned int)((duration * 1000000.0f) / SLEEP_INTERVAL_USEC);
  *anim = makePositionAnimation(origin, target, steps, globePositionUpdate, param);
}

/**
 * @brief Globe position update callback.
 * @param[in] pos   The updated position.
 * @param[in] param The callback parameter.
 */
static void globePositionUpdate(Position pos, void *param) {
  Position *outPos = param;
  *outPos          = pos;
}
