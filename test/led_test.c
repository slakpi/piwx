#include <stdio.h>
#include <ws2811/ws2811.h>

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB    // WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR    // WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW   // SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT               10

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

  ret = ws2811_init(&ledstring);

  if (ret != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }

  // BRG
  ledstring.channel[0].leds[0] = 0x000020; // VFR
  ledstring.channel[0].leds[1] = 0x200000; // MVFR
  ledstring.channel[0].leds[2] = 0x002000; // IFR
  ledstring.channel[0].leds[3] = 0x202000; // LIFR

  ret = ws2811_render(&ledstring);

  if (ret != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }

  return 0;
}
