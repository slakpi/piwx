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
  Bitmap icon = allocateBitmap("wx_thunderstorms.png");
  Bitmap dlIcon = allocateBitmap("downloading.png");
  Bitmap dlErr = allocateBitmap("download_err.png");
  RGB c;
  WxStation *wx = NULL, *ptr = NULL;
  time_t last = 0, now;
  int first = 1;

  do
  {
    if (!cfg->stationQuery)
    {
      drawBitmapInBox(sfc, dlErr, 0, 0, 320, 240);
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
      drawBitmapInBox(sfc, dlErr, 0, 0, 320, 240);
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
  freeBitmap(icon);
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
