/**
 * @file led.c
 * @ingroup LedModule
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

/**
 * The library is incorrect for some WS2811 strings. The GBR constant is
 * really BRG ordering on the ALITOVE string.
 */
#define STRIP_TYPE WS2811_STRIP_GBR

#define WS2811_COLOR(c) (((c).b << 16) | ((c).r << 8) | (c).g)

static bool gInitialized;

static ws2811_t gLedString;

#if !defined HAS_LED_SUPPORT

bool led_init(int dataPin, int dmaChannel, size_t maxLeds) {
  return true;
}

bool led_setColors(const LEDColor *colors, size_t count) {
  return true;
}

void led_finalize() {}

#else

bool led_init(int dataPin, int dmaChannel, size_t maxLeds) {
  if (gInitialized) {
    return true;
  }

  memset(&gLedString, 0, sizeof(gLedString));

  if (maxLeds == 0) {
    return false;
  }

  gLedString.freq                  = DEFAULT_TARGET_FREQ;
  gLedString.channel[0].count      = maxLeds;
  gLedString.channel[0].brightness = 255;
  gLedString.channel[0].strip_type = STRIP_TYPE;

  // Sanity check the data pin.
  switch (dataPin) {
  case 12:
  case 18:
    gLedString.channel[0].gpionum = dataPin;
    break;
  default:
    return false;
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
    gLedString.dmanum = dmaChannel;
    break;
  default:
    return false;
  }

  if (ws2811_init(&gLedString) != WS2811_SUCCESS) {
    return false;
  }

  gInitialized = true;

  return true;
}

bool led_setColors(const LEDColor *colors, size_t count) {
  if (!gInitialized) {
    return false;
  }

  memset(gLedString.channel[0].leds, 0,
         sizeof(gLedString.channel[0].leds[0]) * gLedString.channel[0].count);

  // Convert the colors. If colors is NULL, the LEDs will just be zeroed out.
  if (colors) {
    size_t actualCount = umin(gLedString.channel[0].count, count);

    for (size_t i = 0; i < actualCount; ++i) {
      gLedString.channel[0].leds[i] = WS2811_COLOR(colors[i]);
    }
  }

  // Commit the color configuration to the string.
  return ws2811_render(&gLedString) == WS2811_SUCCESS;
}

void led_finalize() {
  if (!gInitialized) {
    return;
  }

  ws2811_fini(&gLedString);
}

#endif
