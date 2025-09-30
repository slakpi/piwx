/**
 * @file log.c
 * @ingroup LogModule
 */
#include "log.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define MAX_LOG 1024

static FILE *gLog      = NULL;
static int   gMaxLevel = logQuiet;

static const char *getLevelText(LogLevel level);

static void writeLogV(LogLevel level, const char *fmt, va_list args);

void assertLog(bool condition, const char *fmt, ...) {
  va_list args;

  if (condition) {
    return;
  }

  va_start(args, fmt);
  writeLogV(logWarning, fmt, args);
  va_end(args);

  assert(0);
}

bool openLog(const char *logFile, LogLevel maxLevel) {
  if (gLog) {
    closeLog();
  }

  if (maxLevel <= logQuiet) {
    return true;
  }

  gLog = fopen(logFile, "a+");

  if (!gLog) {
    return false;
  }

  gMaxLevel = maxLevel;

  return true;
}

void closeLog(void) {
  if (!gLog) {
    return;
  }

  fclose(gLog);
  gLog      = NULL;
  gMaxLevel = logQuiet;
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

/**
 * @brief   Get the text associated with a log level.
 * @param[in] level The log level to retrieve.
 * @returns The log level text if the level is valid, an empty string otherwise.
 */
static const char *getLevelText(LogLevel level) {
  switch (level) {
  case logWarning:
    return "WARNING";
  case logInfo:
    return "INFO";
  case logDebug:
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
  char      logTime[MAX_LOG] = {0};
  time_t    now;
  struct tm nowTime;

  if (!gLog) {
    return;
  }

  now = time(NULL);
  localtime_r(&now, &nowTime);
  strftime(logTime, COUNTOF(logTime), "%a, %d %b %Y %H:%M:%S %z ", &nowTime);
// NOLINTNEXTLINE -- Ignore fprintf_s() warnings.
  fprintf(gLog, "[%s] %s\n   ", getLevelText(level), logTime);

  vfprintf(gLog, fmt, args);

// NOLINTNEXTLINE -- Ignore fprintf_s() warnings.
  fprintf(gLog, "\n");
  fflush(gLog);
}

