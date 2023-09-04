/**
 * @file led.c
 */
#include "led.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if defined HAS_LED_SUPPORT
#include <ws2811/ws2811.h>
#endif

#define DEFAULT_TARGET_FREQ WS2811_TARGET_FREQ
#define DEFAULT_GPIO_PIN    18
#define DEFAULT_DMA_CHANNEL 10
#define DEFAULT_LED_COUNT   50

/**
 * The library is incorrect for some WS2811 strings. The GBR constant is
 * really BRG ordering on the ALITOVE string.
 */
#define STRIP_TYPE WS2811_STRIP_GBR

#define WS2811_COLOR(c) (((c).b << 16) | ((c).r << 8) | (c).g)

#if !defined HAS_LED_SUPPORT
int led_setColors(int dataPin, int dmaChannel, const LEDColor *colors, size_t count) { return 0; }
#else
int led_setColors(int dataPin, int dmaChannel, const LEDColor *colors, size_t count) {
  ws2811_return_t ret;
  // clang-format off
  ws2811_t        ledstring = {
    .freq   = DEFAULT_TARGET_FREQ,
    .dmanum = DEFAULT_DMA_CHANNEL,
    .channel =
      {
        [0] =
          {
            .gpionum    = DEFAULT_GPIO_PIN,
            .count      = DEFAULT_LED_COUNT,
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

  // Sanity check the data pin.
  switch (dataPin) {
  case 12:
  case 18:
    ledstring.channel[0].gpionum = dataPin;
    break;
  default:
    return -1;
  }

  // Sanity check the DMA channel.
  switch (dmaChannel) {
  case 4:
  case 5:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
    ledstring.dmanum = dmaChannel;
    break;
  default:
    return -1;
  }

  // Initialize the LED string.
  if (ws2811_init(&ledstring) != WS2811_SUCCESS) {
    return -1;
  }

  // Convert the colors. If colors is NULL, the LEDs will just be zeroed out.
  if (colors) {
    size_t actualCount = umin(DEFAULT_LED_COUNT, count);

    for (size_t i = 0; i < actualCount; ++i) {
      ledstring.channel[0].leds[i] = WS2811_COLOR(colors[i]);
    }
  }

  // Commit the color configuration to the string.
  ret = ws2811_render(&ledstring);

  // Cleanup.
  ws2811_fini(&ledstring);

  return (ret == WS2811_SUCCESS ? 0 : -1);
}
#endif
