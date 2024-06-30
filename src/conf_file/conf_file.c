/**
 * @file conf_file.c
 * @ingroup ConfFileModule
 */
typedef void *yyscan_t;

// clang-format off
// These includes must be in this order.
#include "conf_file.h"
#include "conf_file_prv.h"
#include "conf_param.h"
#include "conf.parser.h"
// clang-format on

#define YYSTYPE CONF_STYPE

#include "conf.lexer.h"
#include "geo.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *appendFileToPath(char *path, size_t len, const char *prefix, const char *file);

static void validateConfig(PiwxConfig *cfg);

void conf_freePiwxConfig(PiwxConfig *cfg) {
  if (!cfg) {
    return;
  }

  free(cfg->installPrefix);
  free(cfg->imageResources);
  free(cfg->fontResources);
  free(cfg->configFile);
  free(cfg->stationQuery);

  for (int i = 0; i < CONF_MAX_LEDS; ++i) {
    free(cfg->ledAssignments[i]);
  }

  free(cfg);
}

PiwxConfig *conf_getPiwxConfig(const char *installPrefix, const char *imageResources,
                               const char *fontResources, const char *configFile) {
  FILE       *cfgFile = NULL;
  PiwxConfig *cfg     = NULL;

  if (!installPrefix || !imageResources || !fontResources || !configFile) {
    return NULL;
  }

  cfg = malloc(sizeof(PiwxConfig));

  if (!cfg) {
    return NULL;
  }

  memset(cfg, 0, sizeof(PiwxConfig)); // NOLINT -- Size known.
  cfg->installPrefix      = strdup(installPrefix);
  cfg->imageResources     = strdup(imageResources);
  cfg->fontResources      = strdup(fontResources);
  cfg->configFile         = strdup(configFile);
  cfg->cycleTime          = DEFAULT_CYCLE_TIME;
  cfg->highWindSpeed      = DEFAULT_HIGH_WIND_SPEED;
  cfg->highWindBlink      = DEFAULT_HIGH_WIND_BLINK;
  cfg->ledBrightness      = DEFAULT_LED_BRIGHTNESS;
  cfg->ledNightBrightness = DEFAULT_LED_NIGHT_BRIGHTNESS;
  cfg->ledDataPin         = DEFAULT_LED_DATA_PIN;
  cfg->ledDMAChannel      = DEFAULT_LED_DMA_CHANNEL;
  cfg->logLevel           = DEFAULT_LOG_LEVEL;
  cfg->daylight           = DEFAULT_DAYLIGHT;
  cfg->drawGlobe          = DEFAULT_DRAW_GLOBE;
  cfg->stationSort        = DEFAULT_SORT_TYPE;
  cfg->hasPiTFT           = DEFAULT_HAS_PITFT;

  cfgFile = fopen(configFile, "r");

  if (!cfgFile) {
    return cfg;
  }

  (void)conf_parseStream(cfg, cfgFile);

  fclose(cfgFile);

  validateConfig(cfg);

  return cfg;
}

bool conf_parseStream(PiwxConfig *cfg, FILE *cfgFile) {
  yyscan_t scanner = NULL;
  int      ret;

  conf_lex_init(&scanner);
  conf_set_in(cfgFile, scanner);
  ret = conf_parse(scanner, cfg);
  conf_lex_destroy(scanner);

  return (ret == 0);
}

char *conf_getPathForFont(char *path, size_t len, const char *fontResources, const char *file) {
  return appendFileToPath(path, len, fontResources, file);
}

char *conf_getPathForImage(char *path, size_t len, const char *imageResources, const char *file) {
  return appendFileToPath(path, len, imageResources, file);
}

/**
 * @brief   Append a file name to a path prefix.
 * @details A POSIX-only function to append a trailing backslash to @a prefix,
 *          if necessary, followed by @a name.
 * @param[out] path   Output buffer. May not overlap @a prefix or @a file.
 * @param[in]  len    Length of @a path.
 * @param[in]  prefix Null-terminated path prefix.
 * @param[in]  file   Null-terminated file name.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
static char *appendFileToPath(char *path, size_t len, const char *prefix, const char *file) {
  size_t pl = strlen(prefix);
  size_t fl = strlen(file);

  // If the prefix does not already have a trailing backslash, add one to the
  // length to account for adding the backslash.
  if (prefix[pl - 1] != '/') {
    ++pl;
  }

  // If the prefix length + file length + null terminator is larger than the
  // buffer, return NULL.
  if (pl + fl + 1 >= len) {
    return NULL;
  }

  // Make sure the prefix copies to the beginning of the output buffer.
  path[0] = 0;

  strcat(path, prefix); // NOLINT -- Size checked above.

  // If the first check above did not find a trailing backslash, it increased
  // the length by one. `pl - 1` would now point to the null terminator. If it
  // did find a trailing backslash, then `pl - 1` still points to the backslash.
  // Overwrite the null terminator with the backslash, and set the new null
  // terminator so the file name copy occurs after the backslash.
  if (path[pl - 1] != '/') {
    path[pl - 1] = '/';
    path[pl]     = 0;
  }

  strcat(path, file); // NOLINT -- Size checked above.

  return path;
}

/**
 * @brief   Validate the configuration.
 * @details The LED library only supports pins 12 and 18, so ensure those are
 *          set to valid values. Deconflict the PiTFT from the LED string if
 *          both are present.
 * @param[in,out] cfg The configuration.
 */
static void validateConfig(PiwxConfig *cfg) {
  // Only GPIO12 and GPIO18 are support data pins for the LEDs.
  if (cfg->ledDataPin != 12 && cfg->ledDataPin != 18) {
    cfg->ledDataPin = DEFAULT_LED_DATA_PIN;
  }

  // If the PiTFT is connected, the LED data pin cannot be 18.
  if (cfg->hasPiTFT) {
    cfg->ledDataPin = 12;
  }
}
