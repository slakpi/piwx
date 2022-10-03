/**
 * @file led.c
 */
#include "led.h"
#include "util.h"
#include <string.h>
#include <ws2811/ws2811.h>

#define TARGET_FREQ WS2811_TARGET_FREQ
#define GPIO_PIN    18
#define DMA         10
/**
 * The library is incorrect for some WS2811 strings. The GBR constant is
 * really BRG ordering on the ALITOVE string.
 */
#define STRIP_TYPE  WS2811_STRIP_GBR
#define LED_COUNT   50

#define COLOR(r, g, b) ((b << 16) | (r << 8) | g)
#define COLOR_NONE     COLOR(0, 0, 0)
#define COLOR_VFR      COLOR(0, 255, 0)
#define COLOR_MVFR     COLOR(0, 0, 255)
#define COLOR_IFR      COLOR(255, 0, 0)
#define COLOR_LIFR     COLOR(255, 0, 255)
#define COLOR_WIND     COLOR(255, 192, 0)

static ws2811_led_t getColor(const PiwxConfig *cfg, WxStation *wx);

int updateLEDs(const PiwxConfig *cfg, WxStation *wx) {
  WxStation      *p = wx;
  int             i;
  ws2811_return_t ret;
  // clang-format off
  ws2811_t        ledstring = {
    .freq   = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
      {
        [0] =
          {
            .gpionum    = GPIO_PIN,
            .count      = LED_COUNT,
            .invert     = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
          },
        [1] =
          {
            .gpionum    = 0,
            .count      = 0,
            .invert     = 0,
            .brightness = 0,
          },
      },
  };
  // clang-format on

  // Expect the data pin and DMA channel values to be valid. Fail otherwise.

  switch (cfg->ledDataPin) {
  case 12:
  case 18:
    ledstring.channel[0].gpionum = cfg->ledDataPin;
    break;
  default:
    return -1;
  }

  switch (cfg->ledDMAChannel) {
  case 4:
  case 5:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
    ledstring.dmanum = cfg->ledDMAChannel;
    break;
  default:
    return -1;
  }

  // Initialize writing to the string.
  if (ws2811_init(&ledstring) != WS2811_SUCCESS) {
    return -1;
  }

  ledstring.channel[0].brightness = cfg->ledBrightness;

  // If the user has selected a nearest airport, scan the weather list for that
  // airport and check if the night brightness level should be used instead.
  if (cfg->nearestAirport) {
    while (p) {
      i = p->isNight;

      if (strcmp(cfg->nearestAirport, p->localId) != 0) {
        if (strcmp(cfg->nearestAirport, p->id) != 0) {
          i = 0;
        }
      }

      if (i == 1) {
        ledstring.channel[0].brightness = cfg->ledNightBrightness;
        break;
      }

      p = p->next;

      // Circular list.
      if (p == wx) {
        break;
      }
    }

    // Reset the pointer.
    p = wx;
  }

  // Assign the lights according to the configuration.
  while (p) {
    for (i = 0; i < MAX_LEDS; ++i) {
      if (!cfg->ledAssignments[i]) {
        continue;
      }

      if (strcmp(p->id, cfg->ledAssignments[i]) != 0) {
        continue;
      }

      ledstring.channel[0].leds[i] = getColor(cfg, p);
      break;
    }

    p = p->next;

    // Circular list.
    if (p == wx) {
      break;
    }
  }

  // Commit the color configuration to the string.
  ret = ws2811_render(&ledstring);

  // Cleanup.
  ws2811_fini(&ledstring);

  return (ret == WS2811_SUCCESS ? 0 : -1);
}

/**
 * @brief   Translate weather to a WS2811 color value.
 * @param [in] cfg PiWx configuration.
 * @param [in] wx  Current weather station.
 */
static ws2811_led_t getColor(const PiwxConfig *cfg, WxStation *wx) {
  // If a high-wind threshold has been set, check the speed and gust against
  // the threshold. If the wind exceeds the threshold use yellow if blink is
  // turned OFF or the blink state is zero. Otherwise, reset the blink state and
  // use the METAR color.
  if (cfg->highWindSpeed > 0) {
    if (max(wx->windSpeed, wx->windGust) >= cfg->highWindSpeed) {
      if (wx->blinkState == 0 || cfg->highWindBlink == 0) {
        wx->blinkState = 1;
        return COLOR_WIND;
      } else {
        wx->blinkState = 0;
      }
    }
  }

  switch (wx->cat) {
  case catVFR:
    return COLOR_VFR;
  case catMVFR:
    return COLOR_MVFR;
  case catIFR:
    return COLOR_IFR;
  case catLIFR:
    return COLOR_LIFR;
  default:
    return COLOR_NONE;
  }
}
