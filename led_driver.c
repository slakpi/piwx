#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "led_driver.h"

/*
###############################################################################
#                                                                             #
# WS2812-RPi                                                                  #
# ==========                                                                  #
# A C++ library for driving WS2812 RGB LED's (known as 'NeoPixels' by         #
#     Adafruit) directly from a Raspberry Pi with accompanying Python wrapper #
# Copyright (C) 2014 Rob Kent                                                 #
#                                                                             #
# This program is free software: you can redistribute it and/or modify        #
# it under the terms of the GNU General Public License as published by        #
# the Free Software Foundation, either version 3 of the License, or           #
# (at your option) any later version.                                         #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU General Public License for more details.                                #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.       #
#                                                                             #
###############################################################################
*/

static volatile unsigned int *pwm_reg  = NULL;
static volatile unsigned int *clk_reg  = NULL;
static volatile unsigned int *dma_reg  = NULL;
static volatile unsigned int *gpio_reg = NULL;
static u_int8_t *virtbase  = NULL;
struct control_data_s *ctl = NULL;

typedef struct {
  unsigned int numLEDs;
  unsigned int PWMWaveform[NUM_DATA_WORDS];
  float brightness;
  Color_t *LEDBuffer;
  page_map_t *page_map;
} __LEDControl_t;

static void NeoPixel_initHardware(__LEDControl_t*);
static void NeoPixel_clearLEDBuffer(__LEDControl_t*);
static void NeoPixel_terminate(__LEDControl_t*, int);
static void NeoPixel_setPWMBit(__LEDControl_t*, unsigned int, unsigned char);
static unsigned char NeoPixel_getPWMBit(__LEDControl_t*, unsigned int);
static void NeoPixel_startTransfer(__LEDControl_t*);
static Color_t NeoPixel_RGB2Color(unsigned char, unsigned char, unsigned char);
static Color_t NeoPixel_Color(unsigned char, unsigned char, unsigned char);

LEDControl_t NeoPixel(unsigned int n)
{
  __LEDControl_t *_led = (__LEDControl_t*)malloc(sizeof(__LEDControl_t));
  _led->numLEDs = n;
  _led->LEDBuffer = (Color_t*)malloc(sizeof(Color_t) * n);
  _led->brightness = DEFAULT_BRIGHTNESS;
  _led->page_map = NULL;

  NeoPixel_initHardware(_led);
  NeoPixel_clearLEDBuffer(_led);

  return _led;
}

void NeoPixel_destroy(LEDControl_t led)
{
  NeoPixel_terminate(led, 0);
}

void NeoPixel_show(LEDControl_t led)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  int i, j;
  unsigned int colorBits = 0;
  unsigned char colorBit = 0;
  unsigned int wireBit = 0;

  for (i = 0; i < _led->numLEDs; ++i) {
    _led->LEDBuffer[i].r *= _led->brightness;
    _led->LEDBuffer[i].g *= _led->brightness;
    _led->LEDBuffer[i].b *= _led->brightness;
    colorBits =
      ((unsigned int)_led->LEDBuffer[i].r << 8) |
      ((unsigned int)_led->LEDBuffer[i].g << 16) |
      _led->LEDBuffer[i].b;

    for (j = 23; j >= 0; --j) {
      colorBit = (colorBits & (1 << j)) ? 1 : 0;
      switch (colorBit) {
      case 1:
        NeoPixel_setPWMBit(_led, wireBit++, 1);
        NeoPixel_setPWMBit(_led, wireBit++, 1);
        NeoPixel_setPWMBit(_led, wireBit++, 0);
        break;
      case 0:
        NeoPixel_setPWMBit(_led, wireBit++, 1);
        NeoPixel_setPWMBit(_led, wireBit++, 0);
        NeoPixel_setPWMBit(_led, wireBit++, 0);
        break;
      }
    }
  }

  ctl = (struct control_data_s *)virtbase;
  dma_cb_t *cbp = ctl->cb;

  for(i = 0; i < (cbp->length / 4); ++i) {
    ctl->sample[i] = _led->PWMWaveform[i];
  }

  NeoPixel_startTransfer(_led);

  float bitTimeUSec = (float)(NUM_DATA_WORDS * 32) * 0.4;
  usleep((int)bitTimeUSec);
}

int NeoPixel_setPixelColor(LEDControl_t led, unsigned int pixel, unsigned char r, unsigned char g, unsigned char b)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  if (pixel < 0) {
    //printf("Unable to set pixel %d (less than zero?)\n", pixel);
    return -1;
  }
  if (pixel > _led->numLEDs - 1) {
    //printf("Unable to set pixel %d (LED buffer is %d pixels long)\n", pixel, numLEDs);
    return -1;
  }

  _led->LEDBuffer[pixel] = NeoPixel_RGB2Color(r, g, b);
  return 0;
}

int NeoPixel_setPixelColor2(LEDControl_t led, unsigned int pixel, Color_t c){
  __LEDControl_t *_led = (__LEDControl_t*)led;

  if (pixel < 0) {
    //printf("Unable to set pixel %d (less than zero?)\n", pixel);
    return -1;
  }
  if (pixel > _led->numLEDs - 1) {
    //printf("Unable to set pixel %d (LED buffer is %d pixels long)\n", pixel, numLEDs);
    return -1;
  }

  _led->LEDBuffer[pixel] = c;
  return 0;
}

int NeoPixel_setBrightness(LEDControl_t led, float b)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  _led->brightness = b < 0.0f ? 0.0f : b;
  _led->brightness = _led->brightness > 1.0f ? 1.0f : _led->brightness;
  return 0;
}

Color_t* NeoPixel_getPixels(LEDControl_t led)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  return _led->LEDBuffer;
}

float NeoPixel_getBrightness(LEDControl_t led)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  return _led->brightness;
}

Color_t NeoPixel_getPixelColor(LEDControl_t led, unsigned int pixel)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  if (pixel < 0) {
    //printf("Unable to get pixel %d (less than zero?)\n", pixel);
    return NeoPixel_RGB2Color(0, 0, 0);
  }
  if (pixel > _led->numLEDs - 1) {
    //printf("Unable to get pixel %d (LED buffer is %d pixels long)\n", pixel, numLEDs);
    return NeoPixel_RGB2Color(0, 0, 0);
  }

  return _led->LEDBuffer[pixel];
}

unsigned int NeoPixel_numPixels(LEDControl_t led)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  return _led->numLEDs;
}

void NeoPixel_clear(LEDControl_t led)
{
  __LEDControl_t *_led = (__LEDControl_t*)led;

  NeoPixel_clearLEDBuffer(_led);
}

/*
void NeoPixel::printBinary(unsigned int i, unsigned int bits){
    int x;
    for(x=bits-1; x>=0; x--) {
        printf("%d", (i & (1 << x)) ? 1 : 0);
        if(x % 16 == 0 && x > 0) {
            printf(" ");
        } else if(x % 4 == 0 && x > 0) {
            printf(":");
        }
    }
}
*/

static unsigned int NeoPixel_reverseWord(unsigned int word)
{
  unsigned int count = sizeof(word) * 8 - 1;
  unsigned int output = word;

  word >>= 1;

  while (word) {
    output <<= 1;
    output |= (word & 1);
    word >>= 1;
    --count;
  }

  output <<= count;

/*unsigned int output = 0;
  unsigned char bit;
  int i;

  for (i = 0; i < 32; ++i) {
    bit = word & (1 << i) ? 1 : 0;
    output |= word & (1 << i) ? 1 : 0;
    if (i<31) {
      output <<= 1;
    }
  }
 */

  return output;
}

static void NeoPixel_terminate(__LEDControl_t *led, int dummy)
{
  if (dma_reg) {
    CLRBIT(dma_reg[DMA_CS], DMA_CS_ACTIVE);
    usleep(100);
    SETBIT(dma_reg[DMA_CS], DMA_CS_RESET);
    usleep(100);
  }

  if (pwm_reg) {
    CLRBIT(pwm_reg[PWM_CTL], PWM_CTL_PWEN1);
    usleep(100);
    pwm_reg[PWM_CTL] = (1 << PWM_CTL_CLRF1);
  }

  if (led->page_map) {
    free(led->page_map);
    led->page_map = NULL;
  }
}

static void NeoPixel_fatal(__LEDControl_t *led, char *fmt, ...)
{
  //va_list ap;
  //va_start(ap, fmt);
  //vfprintf(stderr, fmt, ap);
  //va_end(ap);
  NeoPixel_terminate(led, 0);
}

static unsigned int NeoPixel_mem_virt_to_phys(__LEDControl_t *led, void *virt)
{
  unsigned int offset = (u_int8_t*)virt - virtbase;

  return led->page_map[offset >> PAGE_SHIFT].physaddr + (offset % PAGE_SIZE);
}

static unsigned int NeoPixel_mem_phys_to_virt(__LEDControl_t *led, u_int32_t phys)
{
  unsigned int pg_offset = ((unsigned int)phys) & (PAGE_SIZE - 1);
  unsigned int pg_addr = phys - pg_offset;
  int i;

  for (i = 0; i < NUM_PAGES; ++i) {
    if (led->page_map[i].physaddr == pg_addr) {
      return (u_int32_t)virtbase + i * PAGE_SIZE + pg_offset;
    }
  }

  NeoPixel_fatal(led, "Failed to reverse map phys addr %08x\n", phys);

  return 0;
}

static void* NeoPixel_map_peripheral(__LEDControl_t *led, off_t base, size_t len)
{
  int fd = open("/dev/mem", O_RDWR);
  void *vaddr;

  if (fd < 0) {
    NeoPixel_fatal(led, "Failed to open /dev/mem: %m\n");
    return NULL;
  }

  vaddr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);

  if (vaddr == MAP_FAILED) {
    NeoPixel_fatal(led, "Failed to map peripheral at 0x%08x: %m\n", base);
    return NULL;
  }

  close(fd);

  return vaddr;
}

static void NeoPixel_clearPWMBuffer(__LEDControl_t *led)
{
  memset(led->PWMWaveform, 0, NUM_DATA_WORDS * 4);
}

static void NeoPixel_clearLEDBuffer(__LEDControl_t *led)
{
  int i;

  for (i = 0; i < led->numLEDs; ++i) {
    led->LEDBuffer[i].r = 0;
    led->LEDBuffer[i].g = 0;
    led->LEDBuffer[i].b = 0;
  }
}

static Color_t NeoPixel_RGB2Color(unsigned char r, unsigned char g, unsigned char b)
{
  Color_t color = { r, g, b };
  return color;
}

static Color_t NeoPixel_Color(unsigned char r, unsigned char g, unsigned char b)
{
  return NeoPixel_RGB2Color(r, g, b);
}

static void NeoPixel_setPWMBit(__LEDControl_t *led, unsigned int bitPos, unsigned char bit)
{
  unsigned int wordOffset = (int)(bitPos / 32);
  unsigned int bitIdx = bitPos - (wordOffset * 32);

  switch (bit) {
  case 1:
    led->PWMWaveform[wordOffset] |= (1 << (31 - bitIdx));
    break;
  case 0:
    led->PWMWaveform[wordOffset] &= ~(1 << (31 - bitIdx));
    break;
  }
}

static unsigned char NeoPixel_getPWMBit(__LEDControl_t *led, unsigned int bitPos)
{
  unsigned int wordOffset = (int)(bitPos / 32);
  unsigned int bitIdx = bitPos - (wordOffset * 32);

  if(led->PWMWaveform[wordOffset] & (1 << bitIdx)) {
    return 1;
  }

  return 0;
}

static void NeoPixel_initHardware(__LEDControl_t *led)
{
  int i = 0;
  int pid;
  int fd;
  char pagemap_fn[64];

  NeoPixel_clearPWMBuffer(led);

  // Set up peripheral access
  dma_reg = (unsigned int*)NeoPixel_map_peripheral(led, DMA_BASE, DMA_LEN);
  dma_reg += 0x000;
  pwm_reg = (unsigned int*)NeoPixel_map_peripheral(led, PWM_BASE, PWM_LEN);
  clk_reg = (unsigned int*)NeoPixel_map_peripheral(led, CLK_BASE, CLK_LEN);
  gpio_reg = (unsigned int*)NeoPixel_map_peripheral(led, GPIO_BASE, GPIO_LEN);

  // Set PWM alternate function for GPIO18
  SET_GPIO_ALT(18, 5);

  // Allocate memory for the DMA control block & data to be sent
  virtbase = (u_int8_t*) mmap(
    NULL,
    NUM_PAGES * PAGE_SIZE,
    PROT_READ | PROT_WRITE,
    MAP_SHARED |
    MAP_ANONYMOUS |
    MAP_NORESERVE |
    MAP_LOCKED,
    -1,
    0);

  if (virtbase == MAP_FAILED) {
    NeoPixel_fatal(led, "Failed to mmap physical pages: %m\n");
  }

  if ((unsigned long)virtbase & (PAGE_SIZE-1)) {
    NeoPixel_fatal(led, "Virtual address is not page aligned\n");
  }

  // Allocate page map (pointers to the control block(s) and data for each CB
  led->page_map = (page_map_t*)malloc(NUM_PAGES * sizeof(page_map_t));
  if (!led->page_map) {
    NeoPixel_fatal(led, "Failed to malloc page_map: %m\n");
  }

  pid = getpid();
  sprintf(pagemap_fn, "/proc/%d/pagemap", pid);
  fd = open(pagemap_fn, O_RDONLY);

  if (fd < 0) {
    NeoPixel_fatal(led, "Failed to open %s: %m\n", pagemap_fn);
  }

  if (lseek(fd, (unsigned int)virtbase >> 9, SEEK_SET) != (unsigned int)virtbase >> 9) {
    NeoPixel_fatal(led, "Failed to seek on %s: %m\n", pagemap_fn);
  }

  for (i = 0; i < NUM_PAGES; i++) {
    u_int64_t pfn;
    led->page_map[i].virtaddr = virtbase + i * PAGE_SIZE;
    led->page_map[i].virtaddr[0] = 0;

    if (read(fd, &pfn, sizeof(pfn)) != sizeof(pfn)) {
      NeoPixel_fatal(led, "Failed to read %s: %m\n", pagemap_fn);
    }

    if (((pfn >> 55) & 0xfbf) != 0x10c) {
      NeoPixel_fatal(led, "Page %d not present (pfn 0x%016llx)\n", i, pfn);
    }

    led->page_map[i].physaddr = (unsigned int)pfn << PAGE_SHIFT | 0x40000000;
  }

  // Set up control block
  ctl = (struct control_data_s*)virtbase;
  dma_cb_t *cbp = ctl->cb;
  unsigned int phys_pwm_fifo_addr = 0x7e20c000 + 0x18;

  cbp->info = DMA_TI_CONFIGWORD;
  cbp->src = NeoPixel_mem_virt_to_phys(led, ctl->sample);
  cbp->dst = phys_pwm_fifo_addr;

  cbp->length = ((led->numLEDs * 2.25) + 1) * 4;
  if (cbp->length > NUM_DATA_WORDS * 4) {
    cbp->length = NUM_DATA_WORDS * 4;
  }

  cbp->stride = 0;
  cbp->pad[0] = 0;
  cbp->pad[1] = 0;
  cbp->next = 0;

  dma_reg[DMA_CS] |= (1 << DMA_CS_ABORT);
  usleep(100);
  dma_reg[DMA_CS] = (1 << DMA_CS_RESET);
  usleep(100);

  // PWM Clock
  clk_reg[PWM_CLK_CNTL] = 0x5A000000 | (1 << 5);
  usleep(100);

  CLRBIT(pwm_reg[PWM_DMAC], PWM_DMAC_ENAB);
  usleep(100);

  unsigned int idiv = 400;
  unsigned short fdiv = 0;
  clk_reg[PWM_CLK_DIV] = 0x5A000000 | (idiv << 12) | fdiv;
  usleep(100);

  clk_reg[PWM_CLK_CNTL] = 0x5A000015;
  usleep(100);

  // PWM
  pwm_reg[PWM_CTL] = 0;
  pwm_reg[PWM_RNG1] = 32;
  usleep(100);

  pwm_reg[PWM_DMAC] =
    (1 << PWM_DMAC_ENAB) |
    (8 << PWM_DMAC_PANIC) |
    (8 << PWM_DMAC_DREQ);
  usleep(1000);

  SETBIT(pwm_reg[PWM_CTL], PWM_CTL_CLRF1);
  usleep(100);

  CLRBIT(pwm_reg[PWM_CTL], PWM_CTL_RPTL1);
  usleep(100);

  CLRBIT(pwm_reg[PWM_CTL], PWM_CTL_SBIT1);
  usleep(100);

  CLRBIT(pwm_reg[PWM_CTL], PWM_CTL_POLA1);
  usleep(100);

  SETBIT(pwm_reg[PWM_CTL], PWM_CTL_MODE1);
  usleep(100);

  SETBIT(pwm_reg[PWM_CTL], PWM_CTL_USEF1);
  usleep(100);

  CLRBIT(pwm_reg[PWM_CTL], PWM_CTL_MSEN1);
  usleep(100);

  SETBIT(dma_reg[DMA_CS], DMA_CS_INT);
  usleep(100);

  SETBIT(dma_reg[DMA_CS], DMA_CS_END);
  usleep(100);

  dma_reg[DMA_CONBLK_AD] = (unsigned int)NeoPixel_mem_virt_to_phys(led, ctl->cb);
  usleep(100);

  dma_reg[DMA_DEBUG] = 7;
  usleep(100);
}

static void NeoPixel_startTransfer(__LEDControl_t *led)
{
  dma_reg[DMA_CONBLK_AD] = (unsigned int)NeoPixel_mem_virt_to_phys(led, ctl->cb);
  dma_reg[DMA_CS] = DMA_CS_CONFIGWORD | (1 << DMA_CS_ACTIVE);
  usleep(100);

  SETBIT(pwm_reg[PWM_CTL], PWM_CTL_PWEN1);
}
