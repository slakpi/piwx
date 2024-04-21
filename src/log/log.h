/**
 * @file log.h
 */
#if !defined LOG_H
#define LOG_H

#include "util.h"
#include <stdbool.h>

/**
 * @enum  LogLevel
 * @brief Logging levels in order of decreasing priority. A log level of
 *        @a quiet suppresses all messages. A log level of @a warning suppresses
 *        @a info and @a debug messages. A log level of @a info suppresses
 *        @a debug messages.
 *
 * @warning The log is not thread-safe.
 */
typedef enum { logQuiet, logWarning, logInfo, logDebug, logLevelCount } LogLevel;

/**
 * @brief   If @a condition is not true, log the message and assert.
 * @details @a assertLog overrides the max logging level and writes a warning
 *          message if @a condition is false.
 * @param[in] condition The condition to asssert.
 * @param[in] fmt       The @a printf style format string.
 * @param[in] ...       The format parameters.
 */
void assertLog(bool condition, const char *fmt, ...);

/**
 * @brief Close the log file.
 */
void closeLog(void);

/**
 * @brief Open the log.
 * @param[in] logFile  The path to PiWx's log file.
 * @param[in] maxLevel The maximum level to output.
 * @returns True if successful, false otherwise.
 */
bool openLog(const char *logFile, LogLevel maxLevel);

/**
 * @brief Write a message at the specified log level.
 * @param[in] level The log level for the message.
 * @param[in] fmt   The @a printf style format string.
 * @param[in] ...   The format parameters.
 */
void writeLog(LogLevel level, const char *fmt, ...);

#endif /* LOG_H */
