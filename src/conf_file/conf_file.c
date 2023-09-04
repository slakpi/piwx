/**
 * @file conf_file.c
 */
typedef void *yyscan_t;

// clang-format off
// These includes must be in this order.
#include "conf_file.h"
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

static char *appendFileToPath(const char *prefix, const char *file, char *path, size_t len);

void conf_freePiwxConfig(PiwxConfig *cfg) {
  if (!cfg) {
    return;
  }

  free(cfg->installPrefix);
  free(cfg->imageResources);
  free(cfg->fontResources);
  free(cfg->configFile);
  free(cfg->stationQuery);

  for (int i = 0; i < MAX_LEDS; ++i) {
    free(cfg->ledAssignments[i]);
  }

  free(cfg);
}

PiwxConfig *conf_getPiwxConfig(const char *installPrefix, const char *imageResources,
                               const char *fontResources, const char *configFile) {
  FILE       *cfgFile = NULL;
  yyscan_t    scanner = NULL;
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
  cfg->cycleTime          = 60;
  cfg->highWindSpeed      = 25;
  cfg->highWindBlink      = 0;
  cfg->ledBrightness      = 32;
  cfg->ledNightBrightness = 32;
  cfg->ledDataPin         = 18;
  cfg->ledDMAChannel      = 10;
  cfg->logLevel           = logWarning;
  cfg->daylight           = daylightCivil;

  cfgFile = fopen(configFile, "r");

  if (!cfgFile) {
    return cfg;
  }

  conf_lex_init(&scanner);
  conf_set_in(cfgFile, scanner);
  conf_parse(scanner, cfg);
  conf_lex_destroy(scanner);

  fclose(cfgFile);

  return cfg;
}

char *conf_getPathForFont(const char *fontResources, const char *file, char *path, size_t len) {
  return appendFileToPath(fontResources, file, path, len);
}

char *conf_getPathForIcon(const char *imageResources, const char *file, char *path, size_t len) {
  return appendFileToPath(imageResources, file, path, len);
}

/**
 * @brief   Append a file name to a path prefix.
 * @details A POSIX-only function to append a trailing backslash to @a prefix,
 *          if necessary, followed by @a name.
 * @param[in]  prefix Null-terminated path prefix.
 * @param[in]  file   Null-terminated file name.
 * @param[out] path   Output buffer. May not overlap @a prefix or @a file.
 * @param[in]  len    Length of @a path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
static char *appendFileToPath(const char *prefix, const char *file, char *path, size_t len) {
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
