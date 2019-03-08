#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
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
  RGB c;
  WxStation *wx;

  wx = queryWx(cfg->stationQuery);
  freeStations(wx);

  c.r = 1.0;
  c.g = 1.0;
  c.b = 1.0;
  c.a = 1.0;
  setTextColor(font, &c);

  drawText(sfc, font, 0, 0, "KHIO", 4);
  drawBitmapInBox(sfc, icon, 236, 0, 320, 82);

  if (!_test)
    writeToFramebuffer(sfc);
  else
    writeToFile(sfc, "test.png");

  freeSurface(sfc);
  freeFont(font);
  freeBitmap(icon);
  freePiwxConfig(cfg);

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
    return go(t);

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
