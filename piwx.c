#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <wiringPi.h>
#include <config.h>
#include "config_helpers.h"
#include "gfx.h"
#include "wx.h"
#ifdef WITH_LED_SUPPORT
#include "led.h"
#endif

#define BUTTONS  4
#define BUTTON_1 0x1
#define BUTTON_2 0x2
#define BUTTON_3 0x4
#define BUTTON_4 0x8

static const int buttonPins[] = {17, 22, 23, 27};
static const char *shortArgs = "stVv";
static const struct option longArgs[] = {
  { "stand-alone", no_argument,       0, 's' },
  { "test",        no_argument,       0, 't' },
  { "verbose",     no_argument,       0, 'V' },
  { "version",     no_argument,       0, 'v' },
  { 0,             0,                 0,  0  }
};

static int run = 1;

static void signalHandler(int _signo)
{
  switch (_signo)
  {
  case SIGINT:
  case SIGTERM:
  case SIGHUP:
    run = 0;
    break;
  }
}

static unsigned int scanButtons()
{
  int i;
  unsigned int buttons = 0;

  for (i = 0; i < BUTTONS; ++i)
  {
    if(digitalRead(buttonPins[i]) == LOW)
      buttons |= (1 << i);
  }

  return buttons;
}

static void layerToString(const SkyCondition *_sky, char *_buf, size_t _len)
{
  const char *cover = NULL;

  switch (_sky->coverage)
  {
  case skyScattered:
    cover = "SCT";
    break;
  case skyFew:
    cover = "FEW";
    break;
  case skyBroken:
    cover = "BKN";
    break;
  case skyOvercast:
    cover = "OVC";
    break;
  default:
    break;
  }

  if (cover)
    snprintf(_buf, _len, "%s %d", cover, _sky->height);
  else
    snprintf(_buf, _len, "--- %d", _sky->height);
}

static int go(int _test, int _verbose)
{
  PiwxConfig *cfg = getPiwxConfig();
  Surface sfc = allocateSurface(320, 240);
  Font font16 = allocateFont(font_16pt);
  Font font8 = allocateFont(font_8pt);
  Font font6 = allocateFont(font_6pt);
  Bitmap dlIcon = allocateBitmap("downloading.png");
  Bitmap dlErr = allocateBitmap("download_err.png");
  Bitmap icon = NULL;
  WxStation *wx = NULL, *ptr = NULL;
  SkyCondition *sky;
  time_t nextUpdate = 0, nextBlink = 0, nextWx = 0, now;
  int first = 1, draw, i, x, w;
  unsigned int b, bl = 0, bc, r = 0;
  char buf[33], *str;

  if (_verbose)
  {
    printf("Image Resources: %s\n", cfg->imageResources);
    printf("Font Resources: %s\n", cfg->fontResources);
    printf("Station Query: %s\n", cfg->stationQuery);
    printf("Nearest Airport: %s\n", cfg->nearestAirport);
    printf("Cycle Time: %d\n", cfg->cycleTime);
    printf("High-Wind Speed: %d\n", cfg->highWindSpeed);
    printf("LED Brightness: %d\n", cfg->ledBrightness);
    printf("LED Night Brightness: %d\n", cfg->ledNightBrightness);
    printf("LED Data Pin: %d\n", cfg->ledDataPin);
    printf("LED DMA Channel: %d\n", cfg->ledDMAChannel);

    for (i = 0; i < MAX_LEDS; ++i)
    {
      if (cfg->ledAssignments[i])
        printf("LED %d = %s\n", i + 1, cfg->ledAssignments[i]);
    }
  }

  if (!cfg->stationQuery)
    return 0;

  wiringPiSetupGpio();

  for (i = 0; i < BUTTONS; ++i)
  {
    pinMode(buttonPins[i], INPUT);
    pullUpDnControl(buttonPins[i], PUD_UP);
  }

  do
  {
    draw = 0;

    /**
     * Scan the buttons. Mask off any buttons that were pressed on the last
     * scan and are either still pressed or were released.
     */
    b = scanButtons();
    bc = (~bl) & b;
    bl = b;

    now = time(0);

    if (first || now >= nextUpdate || (bc & BUTTON_1))
    {
      if (wx)
        freeStations(wx);

      clearSurface(sfc);
      drawBitmapInBox(sfc, dlIcon, 0, 0, 320, 240);
      writeToFramebuffer(sfc);

      wx = queryWx(cfg->stationQuery, &i);
      ptr = wx;
      first = 0;
      draw = (wx != NULL);
      nextUpdate = ((now / 1200) + 1) * 1200;
      nextWx = now + 1;
      nextBlink = 10;

      if (wx)
      {
        r = 0;
#ifdef WITH_LED_SUPPORT
        updateLEDs(cfg, wx);
#endif
      }
      else
      {
        clearSurface(sfc);

#ifdef WITH_LED_SUPPORT
        updateLEDs(cfg, NULL);
#endif

        setTextColor(font6, &rgbWhite);
        i = snprintf(buf, 33, "Error %d, Retry %d", i, r++);
        drawText(sfc, font6, 0, 0, buf, i);

        drawBitmapInBox(sfc, dlErr, 0, 0, 320, 240);
        writeToFramebuffer(sfc);
        nextUpdate = now + 300;
      }
    }

    if (wx)
    {
      if (now >= nextWx || (bc & BUTTON_3))
      {
        ptr = ptr->next;
        draw = 1;
        nextWx = now + cfg->cycleTime;
      }
      else if (bc & BUTTON_2)
      {
        ptr = ptr->prev;
        draw = 1;
        nextWx = now + cfg->cycleTime;
      }

      if (cfg->highWindSpeed > 0 && now > nextBlink)
      {
        updateLEDs(cfg, wx);
        nextBlink = now + 1;
      }
    }

    if (!draw)
    {
      usleep(50000);
      continue;
    }

    clearSurface(sfc);
    setTextColor(font16, &rgbWhite);
    setTextColor(font8, &rgbWhite);
    setTextColor(font6, &rgbWhite);

    icon = allocateBitmap("separators.png");

    if (icon)
    {
      drawBitmapInBox(sfc, icon, 0, 0, 320, 240);
      freeBitmap(icon);
      icon = NULL;
    }

    str = ptr->localId;

    if (!str)
      str = ptr->id;

    drawText(sfc, font16, 0, 0, str, strlen(str));

    switch (ptr->wx)
    {
    case wxClearDay:
      icon = allocateBitmap("wx_clear_day.png");
      break;
    case wxClearNight:
      icon = allocateBitmap("wx_clear_night.png");
      break;
    case wxScatteredOrFewDay:
      icon = allocateBitmap("wx_few_day.png");
      break;
    case wxScatteredOrFewNight:
      icon = allocateBitmap("wx_few_night.png");
      break;
    case wxBrokenDay:
      icon = allocateBitmap("wx_broken_day.png");
      break;
    case wxBrokenNight:
      icon = allocateBitmap("wx_broken_night.png");
      break;
    case wxOvercast:
      icon = allocateBitmap("wx_overcast.png");
      break;
    case wxLightMistHaze:
    case wxObscuration:
      icon = allocateBitmap("wx_fog_haze.png");
      break;
    case wxLightDrizzleRain:
      icon = allocateBitmap("wx_chance_rain.png");
      break;
    case wxRain:
      icon = allocateBitmap("wx_rain.png");
      break;
    case wxFlurries:
      icon = allocateBitmap("wx_flurries.png");
      break;
    case wxLightSnow:
      icon = allocateBitmap("wx_chance_snow.png");
      break;
    case wxSnow:
      icon = allocateBitmap("wx_snow.png");
      break;
    case wxLightFreezingRain:
      icon = allocateBitmap("wx_chance_fzra.png");
      break;
    case wxFreezingRain:
      icon = allocateBitmap("wx_fzra.png");
      break;
    case wxVolcanicAsh:
      icon = allocateBitmap("wx_volcanic_ash.png");
      break;
    case wxLightTstormsSqualls:
      icon = allocateBitmap("wx_chance_ts.png");
      break;
    case wxTstormsSqualls:
      icon = allocateBitmap("wx_thunderstorms.png");
      break;
    case wxFunnelCloud:
      icon = allocateBitmap("wx_funnel_cloud.png");
      break;
    default:
      break;
    }

    if (icon)
    {
      drawBitmapInBox(sfc, icon, 236, 0, 320, 81);
      freeBitmap(icon);
      icon = NULL;
    }

    switch (ptr->cat)
    {
    case catLIFR:
      icon = allocateBitmap("cat_lifr.png");
      break;
    case catIFR:
      icon = allocateBitmap("cat_ifr.png");
      break;
    case catMVFR:
      icon = allocateBitmap("cat_mvfr.png");
      break;
    case catVFR:
      icon = allocateBitmap("cat_vfr.png");
      break;
    default:
      break;
    }

    if (icon)
    {
      drawBitmapInBox(sfc, icon, 174, 0, 236, 81);
      freeBitmap(icon);
      icon = NULL;
    }

    if (ptr->wxString)
    {
      w = getFontCharWidth(font8);
      x = strlen(ptr->wxString) * w;
      x = (320 - x) / 2;
      drawText(sfc, font8, x < 0 ? 0 : x, 81, ptr->wxString,
        strlen(ptr->wxString));
    }

    switch ((ptr->windDir + 15) / 30 * 30)
    {
    case 0:
      if (ptr->windDir == 0)
        icon = allocateBitmap("wind_calm.png");
      else
        icon = allocateBitmap("wind_360.png");

      break;
    case 30:
      icon = allocateBitmap("wind_30.png");
      break;
    case 60:
      icon = allocateBitmap("wind_60.png");
      break;
    case 90:
      icon = allocateBitmap("wind_90.png");
      break;
    case 120:
      icon = allocateBitmap("wind_120.png");
      break;
    case 150:
      icon = allocateBitmap("wind_150.png");
      break;
    case 180:
      icon = allocateBitmap("wind_180.png");
      break;
    case 210:
      icon = allocateBitmap("wind_210.png");
      break;
    case 240:
      icon = allocateBitmap("wind_240.png");
      break;
    case 270:
      icon = allocateBitmap("wind_270.png");
      break;
    case 300:
      icon = allocateBitmap("wind_300.png");
      break;
    case 330:
      icon = allocateBitmap("wind_330.png");
      break;
    case 360:
      icon = allocateBitmap("wind_360.png");
      break;
    default:
      break;
    }

    if (icon)
    {
      drawBitmap(sfc, icon, 10, 132);
      freeBitmap(icon);
      icon = NULL;
    }

    if (ptr->windDir > 0)
      snprintf(buf, 33, "%d\x01", ptr->windDir);
    else
    {
      if (ptr->windSpeed == 0)
        strncpy(buf, "Calm", 33);
      else
        strncpy(buf, "Var", 33);
    }
    drawText(sfc, font6, 84, 126, buf, strlen(buf));

    if (ptr->windSpeed == 0)
      strncpy(buf, "---", 33);
    else
      snprintf(buf, 33, "%dkt", ptr->windSpeed);

    drawText(sfc, font6, 84, 149, buf, strlen(buf));

    setTextColor(font6, &rgbRed);

    if (ptr->windGust == 0)
      strncpy(buf, "---", 33);
    else
      snprintf(buf, 33, "%dkt", ptr->windGust);

    drawText(sfc, font6, 84, 172, buf, strlen(buf));

    setTextColor(font6, &rgbWhite);

    sky = ptr->layers;

    if (sky)
    {
      switch (sky->coverage)
      {
      case skyClear:
        strncpy(buf, "Clear", 33);
        drawText(sfc, font6, 172, 126, buf, strlen(buf));
        break;
      case skyOvercastSurface:
        snprintf(buf, 33, "VV %d", ptr->vertVis);
        drawText(sfc, font6, 172, 126, buf, strlen(buf));
        break;
      default:
        while (sky)
        {
          if (sky->coverage >= skyBroken)
          {
            if (sky->prev)
              sky = sky->prev;

            break;
          }

          sky = sky->next;
        }

        if (!sky)
          sky = ptr->layers;

        layerToString(sky, buf, 33);
        drawText(sfc, font6, 172, sky->next ? 149 : 126, buf, strlen(buf));

        if (sky->next)
        {
          layerToString(sky->next, buf, 33);
          drawText(sfc, font6, 172, 126, buf, strlen(buf));
        }

        break;
      }
    }

    snprintf(buf, 33, "%dsm vis", ptr->visibility);
    drawText(sfc, font6, 172, 172, buf, strlen(buf));

    snprintf(buf, 33, "%.0f\x01/%.0f\x01\x43", ptr->temp, ptr->dewPoint);
    drawText(sfc, font6, 0, 206, buf, strlen(buf));

    snprintf(buf, 33, "%.2f\"", ptr->alt);
    drawText(sfc, font6, 172, 206, buf, strlen(buf));

    if (_test)
    {
      writeToFile(sfc, "test.png");
      break;
    }

    writeToFramebuffer(sfc);
  } while (run);

#ifdef WITH_LED_SUPPORT
  updateLEDs(cfg, NULL);
#endif

  if (sfc)
  {
    clearSurface(sfc);
    writeToFramebuffer(sfc);
    freeSurface(sfc);
  }
  if (font16)
    freeFont(font16);
  if (font8)
    freeFont(font8);
  if (font6)
    freeFont(font6);
  if (dlIcon)
    freeBitmap(dlIcon);
  if (dlErr)
    freeBitmap(dlErr);
  if (cfg)
    freePiwxConfig(cfg);
  if (wx)
    freeStations(wx);

  return 0;
}

int main(int _argc, char* _argv[])
{
  pid_t pid, sid;
  int c, t = 0, standAlone = 0, verbose = 0;

  while ((c = getopt_long(_argc, _argv, shortArgs, longArgs, 0)) != -1)
  {
    switch (c)
    {
    case 's':
      standAlone = 1;
      break;
    case 't':
      standAlone = 1;
      t = 1;
      break;
    case 'V':
      verbose = 1;
      break;
    case 'v':
      printf("piwx (%s)\n", GIT_COMMIT_HASH);
      return 0;
    }
  }

  if (standAlone)
  {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);
    return go(t, verbose);
  }

  pid = fork();

  if (pid < 0)
    return -1;
  if (pid > 0)
    return 0;

  umask(0);

  sid = setsid();

  if (sid < 0)
    return -1;

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGHUP, signalHandler);

  return go(0, 0);
}
