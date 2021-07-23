#include "led.h"
#include "util.h"
#include <string.h>
#include <ws2811/ws2811.h>

#define TARGET_FREQ WS2811_TARGET_FREQ
#define GPIO_PIN 18
#define DMA 10
/**
 * The library is incorrect for some WS2811 strings. The GBR constant is
 * really BRG ordering on the ALITOVE string.
 */
#define STRIP_TYPE WS2811_STRIP_GBR
#define LED_COUNT 50

/**
 * @brief   Translate weather to a WS2811 color value.
 * @param [in] _cfg PiWx configuration.
 * @param [in] _wx  Current weather station.
 */
static ws2811_led_t getColor(const PiwxConfig *_cfg, WxStation *_wx) {
  char r = 0, g = 0, b = 0;

  // If a high-wind threshold has been set, check the speed and gust against
  // the threshold. If the wind exceeds the threshold use yellow if blink is
  // turned OFF or the blink state is zero. Otherwise, reset the blink state and
  // use the METAR color.
  if (_cfg->highWindSpeed > 0) {
    if (max(_wx->windSpeed, _wx->windGust) >= _cfg->highWindSpeed) {
      if (_wx->blinkState == 0 || _cfg->highWindBlink == 0) {
        _wx->blinkState = 1;
        return (255 << 8) | 192;
      } else {
        _wx->blinkState = 0;
      }
    }
  }

  switch (_wx->cat) {
  case catVFR:
    g = 255;
    break;
  case catMVFR:
    b = 255;
    break;
  case catIFR:
    r = 255;
    break;
  case catLIFR:
    r = 255;
    b = 255;
    break;
  case catInvalid:
    break; // No color.
  }

  return (b << 16) | (r << 8) | g;
}

int updateLEDs(const PiwxConfig *_cfg, WxStation *_wx) {
  WxStation *p = _wx;
  int i;
  ws2811_return_t ret;
  ws2811_t ledstring = {
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
      {
        [0] =
          {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
          },
        [1] =
          {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
          },
      },
  };

  // Expect the data pin and DMA channel values to be valid. Fail otherwise.

  switch (_cfg->ledDataPin) {
  case 12:
  case 18:
    ledstring.channel[0].gpionum = _cfg->ledDataPin;
    break;
  default:
    return -1;
  }

  switch (_cfg->ledDMAChannel) {
  case 4:
  case 5:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
    ledstring.dmanum = _cfg->ledDMAChannel;
    break;
  default:
    return -1;
  }

  // Initialize writing to the string.
  if (ws2811_init(&ledstring) != WS2811_SUCCESS) {
    return -1;
  }

  ledstring.channel[0].brightness = _cfg->ledBrightness;

  // If the user has selected a nearest airport, scan the weather list for that
  // airport and check if the night brightness level should be used instead.
  if (_cfg->nearestAirport) {
    while (p) {
      i = p->isNight;

      if (strcmp(_cfg->nearestAirport, p->localId) != 0) {
        if (strcmp(_cfg->nearestAirport, p->id) != 0) {
          i = 0;
        }
      }

      if (i == 1) {
        ledstring.channel[0].brightness = _cfg->ledNightBrightness;
        break;
      }

      p = p->next;

      // Circular list.
      if (p == _wx)
        break;
    }

    // Reset the pointer.
    p = _wx;
  }

  // Assign the lights according to the configuration.
  while (p) {
    for (i = 0; i < MAX_LEDS; ++i) {
      if (!_cfg->ledAssignments[i]) {
        continue;
      }

      if (strcmp(p->id, _cfg->ledAssignments[i]) != 0) {
        continue;
      }

      ledstring.channel[0].leds[i] = getColor(_cfg, p);
      break;
    }

    p = p->next;

    // Circular list.
    if (p == _wx)
      break;
  }

  // Commit the color configuration to the string.
  ret = ws2811_render(&ledstring);

  // Cleanup.
  ws2811_fini(&ledstring);

  return (ret == WS2811_SUCCESS ? 0 : -1);
}
