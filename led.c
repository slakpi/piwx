#include <string.h>
#include <ws2811/ws2811.h>
#include "led.h"

#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
/**
 * The library is incorrect for some WS2811 strings. The GBR constant is
 * really BRG ordering on the ALITOVE string.
 */
#define STRIP_TYPE              WS2811_STRIP_GBR
#define LED_COUNT               50

static ws2811_t ledstring =
{
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

static ws2811_led_t getColor(const PiwxConfig *_cfg, const WxStation *_wx)
{
  char r = 0, g = 0, b = 0;

  switch (_wx->cat)
  {
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

  if (_wx->windSpeed >= _cfg->highWindSpeed ||
      _wx->windGust >= _cfg->highWindSpeed)
  {
    r = 247;
    g = 178;
    b = 6;
  }

  return (b << 16) | (r << 8) | g;
}

int updateLEDs(const PiwxConfig *_cfg, const WxStation *_wx)
{
  const WxStation *p = _wx;
  ws2811_return_t ret;
  int i;

  switch (_cfg->ledDataPin)
  {
  case 12:
  case 18:
    ledstring.channel[0].gpionum = _cfg->ledDataPin;
    break;
  default:
    return -1;
  }

  if (_cfg->ledDMAChannel >= 0 || _cfg->ledDMAChannel < 16)
    ledstring.dmanum = _cfg->ledDMAChannel;

  ret = ws2811_init(&ledstring);

  if (ret != WS2811_SUCCESS)
    return -1;

  ledstring.channel[0].brightness = _cfg->ledBrightness;

  if (_cfg->nearestAirport)
  {
    while (p)
    {
      i = p->isNight;

      if (strcmp(_cfg->nearestAirport, p->localId) != 0)
      {
        if (strcmp(_cfg->nearestAirport, p->id) != 0)
          i = 0;
      }

      if (i == 1)
      {
        ledstring.channel[0].brightness = _cfg->ledNightBrightness;
        break;
      }

      p = p->next;

      // Circular list.
      if (p == _wx)
        break;
    }

    p = _wx;
  }

  while (p)
  {
    for (i = 0; i < MAX_LEDS; ++i)
    {
      if (!_cfg->ledAssignments[i])
        continue;

      if (strcmp(p->id, _cfg->ledAssignments[i]) != 0)
        continue;

      ledstring.channel[0].leds[i] = getColor(_cfg, p);
      break;
    }

    p = p->next;

    // Circular list.
    if (p == _wx)
      break;
  }

  ret = ws2811_render(&ledstring);

  ws2811_fini(&ledstring);

  return (ret == WS2811_SUCCESS ? 0 : -1);
}
