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

static ws2811_led_t getColor(FlightCategory _cat, float _brightness)
{
  char r = 0, g = 0, b = 0;

  switch (_cat)
  {
  case catVFR:
    g = (char)(255.0f * _brightness);
    break;
  case catMVFR:
    b = (char)(255.0f * _brightness);
    break;
  case catIFR:
    r = (char)(255.0f * _brightness);
    break;
  case catLIFR:
    r = (char)(255.0f * _brightness);
    b = r;
    break;
  case catInvalid:
    // Leave the LED off.
    break;
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

  while (p)
  {
    for (i = 0; i < MAX_LEDS; ++i)
    {
      if (!_cfg->ledAssignments[i])
        continue;

      if (strcmp(p->id, _cfg->ledAssignments[i]) != 0)
        continue;

      ledstring.channel[0].leds[i] = getColor(p->cat, _cfg->ledBrightness);
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
