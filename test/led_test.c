#include <stdio.h>
#include <ws2811/ws2811.h>

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB    // WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR    // WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW   // SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT               50

ws2811_t ledstring =
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

int main()
{
  ws2811_return_t ret;
  int i;

  ret = ws2811_init(&ledstring);

  if (ret != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }

  // BRG
  for (i = 0; i < LED_COUNT; ++i) {
    switch (i % 4) {
    case 0:
      ledstring.channel[0].leds[i] = 0x000020; // VFR
      break;
    case 1:
      ledstring.channel[0].leds[i] = 0x200000; // MVFR
      break;
    case 2:
      ledstring.channel[0].leds[i] = 0x002000; // IFR
      break;
    case 3:
      ledstring.channel[0].leds[i] = 0x202000; // LIFR
      break;
    }
  }

  ret = ws2811_render(&ledstring);

  if (ret != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }

  return 0;
}
