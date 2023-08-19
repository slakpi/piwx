/**
 * @file led.c
 */
#include "led.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
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

#define WS2811_COLOR(c)      (((c).b << 16) | ((c).r << 8) | (c).g)
#define MIX_BRIGHTNESS(c, b) ((((uint32_t)(c) << 8) * (b)) >> 16)

// clang-format off
#define COLOR_NONE {0, 0, 0}
#define COLOR_VFR  {0, 255, 0}
#define COLOR_MVFR {0, 0, 255}
#define COLOR_IFR  {255, 0, 0}
#define COLOR_LIFR {255, 0, 255}
#define COLOR_WIND {255, 192, 0}
// clang-format on

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} Color;

static const Color gColorVFR  = COLOR_VFR;
static const Color gColorMVFR = COLOR_MVFR;
static const Color gColorIFR  = COLOR_IFR;
static const Color gColorLIFR = COLOR_LIFR;
static const Color gColorWind = COLOR_WIND;

static ws2811_led_t getColor(const PiwxConfig *cfg, const WxStation *station);

int updateLEDs(const PiwxConfig *cfg, const WxStation *stations) {
  const WxStation *p = stations;
  int              i;
  ws2811_return_t  ret;
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
            // Use full brightness. Each light will control its own brightness
            // by scaling the color.
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
    if (p == stations) {
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
 * @param[in] cfg PiWx configuration.
 * @param[in] wx  Current weather station.
 */
static ws2811_led_t getColor(const PiwxConfig *cfg, const WxStation *station) {
  Color color      = COLOR_NONE;
  int   brightness = station->isNight ? cfg->ledNightBrightness : cfg->ledBrightness;

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
    break;
  }

  if (station->blinkState) {
    color = gColorWind;
  }

  color.r = MIX_BRIGHTNESS(color.r, brightness);
  color.g = MIX_BRIGHTNESS(color.g, brightness);
  color.b = MIX_BRIGHTNESS(color.b, brightness);

  return WS2811_COLOR(color);
}
