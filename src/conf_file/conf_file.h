/**
 * @file conf_file.h
 */
#if !defined CONF_FILE_H
#define CONF_FILE_H

#include "geo.h"
#include "log.h"
#include <stddef.h>

#define CONF_MAX_LEDS 50

/**
 * @struct PiwxConfig
 * @brief  The configuration structure.
 */
typedef struct {
  char        *installPrefix;                 // PiWx install prefix
  char        *imageResources;                // Image resources path
  char        *fontResources;                 // Font resources path
  char        *configFile;                    // Config file path
  char        *stationQuery;                  // List of weather stations to query
  int          cycleTime;                     // Airport display cycle time in sec.
  int          highWindSpeed;                 // High wind threshold in knots
  bool         highWindBlink;                 // High wind blink rate in sec.
  char        *ledAssignments[CONF_MAX_LEDS]; // Airport LED assignments
  int          ledBrightness;                 // Day LED brightness, 0-255
  int          ledNightBrightness;            // Night LED brightness, 0-255
  int          ledDataPin;                    // LED Rpi data pin
  int          ledDMAChannel;                 // LED Rpi DMA channel
  LogLevel     logLevel;                      // Logging output level
  DaylightSpan daylight;                      // Daylight span for night dimming
  bool         drawGlobe;                     // Draw day/night globe
} PiwxConfig;

/**
 * @brief Free an allocated configuration struct.
 * @param[in] cfg The configuration struct to free.
 */
void conf_freePiwxConfig(PiwxConfig *cfg);

/**
 * @brief   Load the PiWx configuration.
 * @details Loads the PiWx configuration from @a configFile or initializes the
 *          configuration structure with the PiWx defaults if the file does not
 *          exist or is invalid.
 * @param[in] installPrefix  The PiWx installation prefix path.
 * @param[in] imageResources The path to PiWx's image resources.
 * @param[in] fontResources  The path to PiWx's font resources.
 * @param[in] configFile     The path to PiWx's configuration file.
 * @returns An initialized configuration structure or NULL if unable to allocate
 *          memory for the structure or any of the paths is invalid.
 */
PiwxConfig *conf_getPiwxConfig(const char *installPrefix, const char *imageResources,
                               const char *fontResources, const char *configFile);

/**
 * @brief   Load the fully-qualified path to the specified font file.
 * @details Appends @a file to the @a fontResources path using @a path as an
 *          output buffer.
 * @param[in]  fontResources The path to PiWx's font resources.
 * @param[in]  file          The font file to append.
 * @param[out] path          The buffer to hold the fully-qualified path.
 * @param[in]  len           The length of @a path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
char *conf_getPathForFont(const char *fontResources, const char *file, char *path, size_t len);

/**
 * @brief   Load the fully-qualified path to the specified bitmap file.
 * @details Appends @a file to the @a imageResources path using @a path as an
 *          output buffer.
 * @param[in]  imageResources The path to PiWx's image resources.
 * @param[in]  file           The bitmap file to append.
 * @param[out] path           The buffer to hold the fully-qualified path.
 * @param[in]  len            The length of @a path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
char *conf_getPathForImage(const char *imageResources, const char *file, char *path, size_t len);

#endif /* CONF_FILE_H */
