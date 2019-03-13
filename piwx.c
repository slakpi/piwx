#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
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
  Font font = allocateFont(font_16pt);
  Bitmap dlIcon = allocateBitmap("downloading.png");
  Bitmap dlErr = allocateBitmap("download_err.png");
  Bitmap icon = NULL;
  RGB c;
  WxStation *wx = NULL, *ptr = NULL;
  time_t last = 0, now;
  int first = 1;

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
    setTextColor(font, &c);

    drawText(sfc, font, 0, 0, ptr->id, strlen(ptr->id));

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

    if (_test)
    {
      writeToFile(sfc, "test.png");
      break;
    }

    writeToFramebuffer(sfc);
    ptr = ptr->next;
    sleep(5);
  } while (run);

  freeSurface(sfc);
  freeFont(font);
  freeBitmap(dlIcon);
  freeBitmap(dlErr);
  freePiwxConfig(cfg);
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
