/**
 * @file conf_file.h
 */
#if !defined CONF_FILE_H
#define CONF_FILE_H

#include "log.h"
#include <stddef.h>

#define MAX_LEDS 50

/**
 * @struct  PiwxConfig
 * @brief   The configuration structure.
 * @details The @a installPrefix, @a imageResources, @a fontResoures, and
 *          @a configFile members are informational only. The remaining members
 *          contain the values read from @a CONFIG_FILE or the defaults.
 */
typedef struct {
  char    *installPrefix;            // Compile-time install prefix
  char    *imageResources;           // Compile-time image resources path
  char    *fontResources;            // Compile-time font resources path
  char    *configFile;               // Compile-time config file path
  char    *stationQuery;             // List of weather stations to query
  int      cycleTime;                // Airport display cycle time in sec.
  int      highWindSpeed;            // High wind threshold in knots
  int      highWindBlink;            // High wind blink rate in sec.
  char    *ledAssignments[MAX_LEDS]; // Airport LED assignments
  int      ledBrightness;            // Day LED brightness, 0-255
  int      ledNightBrightness;       // Night LED brightness, 0-255
  int      ledDataPin;               // LED Rpi data pin
  int      ledDMAChannel;            // LED Rpi DMA channel
  LogLevel logLevel;                 // Logging output level
} PiwxConfig;

/**
 * @brief Free an allocated configuration struct.
 * @param[in] cfg The configuration struct to free.
 */
void conf_freePiwxConfig(PiwxConfig *cfg);

/**
 * @brief   Load the piwx configuration.
 * @details Loads the piwx configuration from @a CONFIG_FILE or initializes the
 *          configuration structure with the piwx defaults if the file does not
 *          exist or is invalid.
 * @returns An initialized configuration structure or NULL if unable to allocate
 *          memory for the structure.
 */
PiwxConfig *conf_getPiwxConfig();

/**
 * @brief   Load the fully-qualified path to the specified bitmap file.
 * @details Appends @a file to the @a FONT_RESOURCES path using @a path as an
 *          output buffer.
 * @param[in] file The font file to append.
 * @param[in] path The buffer to hold the fully-qualified path.
 * @param[in] len  The length of @a path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
char *conf_getPathForFont(const char *file, char *path, size_t len);

/**
 * @brief   Load the fully-qualified path to the specified bitmap file.
 * @details Appends @a file to the @a IMAGE_RESOURCES path using @a path as
 *          an output buffer.
 * @param[in] file The bitmap file to append.
 * @param[in] path The buffer to hold the fully-qualified path.
 * @param[in] len  The length of @a path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
char *conf_getPathForIcon(const char *file, char *path, size_t len);

#endif /* CONF_FILE_H */
