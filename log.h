#ifndef LOG_H
#define LOG_H

#include "util.h"

/**
 * @enum  LogLevel
 * @brief Logging levels in order of decreasing priority. A log level of
 *        @a quiet suppresses all messages. A log level of @a warning suppresses
 *        @a info and @a debug messages. A log level of @a info suppresses
 *        @a debug messages.
 *
 * @warning The log is not thread-safe.
 */
typedef enum {
  LOG_QUIET,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG,
} LogLevel;

/**
 * @brief Open the log.
 * @param[in] _level The maximum level to output.
 * @returns TRUE if successful, FALSE otherwise.
 */
boolean openLog(LogLevel _maxLevel);

/**
 * @brief Write a message at the specified log level.
 * @param[in] _level The log level for the message.
 * @param[in] _fmt   The @a printf style format string.
 * @param[in] ...    The format parameters.
 */
void writeLog(LogLevel _level, const char *_fmt, ...);

/**
 * @brief Close the log file.
 */
void closeLog();

#endif
