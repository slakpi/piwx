#include "log.h"
#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log      = NULL;
static int   maxLevel = LOG_QUIET;

static const char *getLevelText(LogLevel _level) {
  switch (_level) {
  case LOG_WARNING:
    return "WARNING";
  case LOG_INFO:
    return "INFO";
  case LOG_DEBUG:
    return "DEBUG";
  default:
    return "";
  }
}

boolean openLog(LogLevel _maxLevel) {
  if (log) {
    closeLog();
  }

  if (_maxLevel <= LOG_QUIET) {
    return TRUE;
  }

  log = fopen(LOG_FILE, "a+");

  if (!log) {
    return FALSE;
  }

  maxLevel = _maxLevel;

  return TRUE;
}

void writeLog(LogLevel _level, const char *_fmt, ...) {
  char      logTime[256];
  time_t    now;
  struct tm nowTime;
  va_list   args;

  if (!log || _level > maxLevel) {
    return;
  }

  now = time(0);
  localtime_r(&now, &nowTime);
  strftime(logTime, 256, "%a, %d %b %Y %H:%M:%S %z ", &nowTime);
  fprintf(log, "[%s] %s\n   ", getLevelText(_level), logTime);

  va_start(args, _fmt);
  vfprintf(log, _fmt, args);
  va_end(args);

  fprintf(log, "\n");
  fflush(log);
}

void closeLog() {
  if (!log) {
    return;
  }

  fclose(log);
  log      = NULL;
  maxLevel = LOG_QUIET;
}
