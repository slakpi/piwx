#include "log.h"
#include <assert.h>
#include <config.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *gLog      = NULL;
static int   gMaxLevel = LOG_QUIET;

/**
 * @brief   Get the text associated with a log level.
 * @param[in] level The log level to retrieve.
 * @returns The log level text if the level is valid, an empty string otherwise.
 */
static const char *getLevelText(LogLevel level) {
  switch (level) {
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

/**
 * @brief Write a message at the specified log level. 
 * @param[in] level The log level for the message.
 * @param[in] fmt   The @a printf style format string.
 * @param[in] ...   The format parameters.
 */
static void writeLogV(LogLevel level, const char *fmt, va_list args) {
  char      logTime[256];
  time_t    now;
  struct tm nowTime;

  if (!gLog) {
    return;
  }

  now = time(0);
  localtime_r(&now, &nowTime);
  strftime(logTime, 256, "%a, %d %b %Y %H:%M:%S %z ", &nowTime);
  fprintf(gLog, "[%s] %s\n   ", getLevelText(level), logTime);

  vfprintf(gLog, fmt, args);

  fprintf(gLog, "\n");
  fflush(gLog);
}

boolean openLog(LogLevel maxLevel) {
  if (gLog) {
    closeLog();
  }

  if (maxLevel <= LOG_QUIET) {
    return TRUE;
  }

  gLog = fopen(LOG_FILE, "a+");

  if (!gLog) {
    return FALSE;
  }

  gMaxLevel = maxLevel;

  return TRUE;
}

void assertLog(boolean condition, const char *fmt, ...) {
  va_list args;

  if (condition) {
    return;
  }

  va_start(args, fmt);
  writeLogV(LOG_WARNING, fmt, args);
  va_end(args);

  assert(0);
}

void writeLog(LogLevel level, const char *fmt, ...) {
  va_list args;

  if (level > gMaxLevel) {
    return;
  }

  va_start(args, fmt);
  writeLogV(level, fmt, args);
  va_end(args);
}

void closeLog() {
  if (!gLog) {
    return;
  }

  fclose(gLog);
  gLog      = NULL;
  gMaxLevel = LOG_QUIET;
}
