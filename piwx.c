#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include "wx.h"

static const char *shortArgs = "st:";
static const struct option longArgs[] = {
  { "stand-alone", no_argument,       0, 's' },
  { "test",        required_argument, 0, 't' },
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

static int go()
{
  while (run)
  {
  }

  return 0;
}

static void testSunriseSunset()
{
  struct tm sunrise, sunset;
  getSunriseSunset(&sunrise, &sunset);
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
      t = atoi(optarg);
      break;
    }
  }

  switch (t)
  {
  case 0:
    break;
  case 1:
    testSunriseSunset();
    return 0;
  }

  if (standAlone)
    return go();

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

  return go();
}
