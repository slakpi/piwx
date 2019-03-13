#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "config_helpers.h"
#include "gfx.h"
#include "wx.h"

static const char *shortArgs = "st";
static const struct option longArgs[] = {
  { "stand-alone", no_argument,       0, 's' },
  { "test",        no_argument,       0, 't' },
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

static int go(int _test)
{
  PiwxConfig *cfg = getPiwxConfig();
  Surface sfc = allocateSurface(320, 240);
  Font font16 = allocateFont(font_16pt);
  Font font8 = allocateFont(font_8pt);
  Font font6 = allocateFont(font_6pt);
  Bitmap dlIcon = allocateBitmap("downloading.png");
  Bitmap dlErr = allocateBitmap("download_err.png");
  Bitmap icon = NULL;
  RGB c;
  WxStation *wx = NULL, *ptr = NULL;
  time_t last = 0, now;
  int first = 1;
  char buf[33];

  do
  {
    if (!cfg->stationQuery)
    {
      clearSurface(sfc);
      drawBitmapInBox(sfc, dlErr, 0, 0, 320, 240);
      writeToFramebuffer(sfc);
      break;
    }

    now = time(0);
    if (!wx || first || (now - last > 1200))
    {
      if (wx)
        freeStations(wx);

      clearSurface(sfc);
      drawBitmapInBox(sfc, dlIcon, 0, 0, 320, 240);
      writeToFramebuffer(sfc);

      wx = queryWx(cfg->stationQuery);
      ptr = wx;
      first = 0;
      last = now;
    }

    if (!wx)
    {
      clearSurface(sfc);
      drawBitmapInBox(sfc, dlErr, 0, 0, 320, 240);
      writeToFramebuffer(sfc);
      sleep(60);
      continue;
    }

    if (!ptr)
      ptr = wx;

    clearSurface(sfc);

    c.r = 1.0;
    c.g = 1.0;
    c.b = 1.0;
    c.a = 1.0;
    setTextColor(font16, &c);
    setTextColor(font8, &c);
    setTextColor(font6, &c);

    icon = allocateBitmap("separators.png");

    if (icon)
    {
      drawBitmapInBox(sfc, icon, 0, 0, 320, 240);
      freeBitmap(icon);
      icon = NULL;
    }

    drawText(sfc, font16, 0, 0, ptr->id, strlen(ptr->id));

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
      drawText(sfc, font8, 5, 81, ptr->wxString, strlen(ptr->wxString));
    else
      drawText(sfc, font8, 5, 81, "No Present Wx", 13);

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
      snprintf(buf, 33, "%d", ptr->windDir);
    else
    {
      if (ptr->windSpeed == 0)
        strncpy(buf, "Calm", 33);
      else
        strncpy(buf, "Var.", 33);
    }
    drawText(sfc, font6, 84, 126, buf, strlen(buf));

    if (ptr->windSpeed == 0)
      strncpy(buf, "---", 33);
    else
      snprintf(buf, 33, "%d", ptr->windSpeed);

    drawText(sfc, font6, 84, 149, buf, strlen(buf));

    c.r = 1.0;
    c.g = 0.0;
    c.b = 0.0;
    c.a = 1.0;
    setTextColor(font6, &c);

    if (ptr->windGust == 0)
      strncpy(buf, "---", 33);
    else
      snprintf(buf, 33, "%d", ptr->windGust);

    drawText(sfc, font6, 84, 172, buf, strlen(buf));

    if (_test)
    {
      writeToFile(sfc, "test.png");
      break;
    }

    writeToFramebuffer(sfc);
    ptr = ptr->next;
    sleep(5);
  } while (run);

  if (sfc)
    freeSurface(sfc);
  if (font16)
    freeFont(font16);
  if (font8)
    freeFont(font8);
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
  int c, t = 0, standAlone = 0;

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
    }
  }

  if (standAlone)
  {
    signal(SIGINT, signalHandler);
    return go(t);
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

  return go(0);
}
