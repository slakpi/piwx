/**
 * @file conf_file.c
 */
#include <config.h>
#include <string.h>

typedef void *yyscan_t;

// clang-format off
// These includes must be in this order.
#include "conf_file.h"
#include "conf_param.h"
#include "conf.parser.h"
// clang-format on

#define YYSTYPE CONF_STYPE

#include "conf.lexer.h"

PiwxConfig *getPiwxConfig() {
  FILE *      cfgFile;
  yyscan_t    scanner;
  PiwxConfig *cfg = (PiwxConfig *)malloc(sizeof(PiwxConfig));

  if (!cfg) {
    return NULL;
  }

  memset(cfg, 0, sizeof(PiwxConfig));
  cfg->installPrefix      = strdup(INSTALL_PREFIX);
  cfg->imageResources     = strdup(IMAGE_RESOURCES);
  cfg->fontResources      = strdup(FONT_RESOURCES);
  cfg->configFile         = strdup(CONFIG_FILE);
  cfg->cycleTime          = 60;
  cfg->highWindSpeed      = 25;
  cfg->highWindBlink      = 0;
  cfg->ledBrightness      = 32;
  cfg->ledNightBrightness = 32;
  cfg->ledDataPin         = 18;
  cfg->ledDMAChannel      = 10;

  cfgFile = fopen(CONFIG_FILE, "r");

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

void freePiwxConfig(PiwxConfig *_cfg) {
  int i;

  if (!_cfg) {
    return;
  }

  free(_cfg->installPrefix);
  free(_cfg->imageResources);
  free(_cfg->fontResources);
  free(_cfg->configFile);
  free(_cfg->stationQuery);
  free(_cfg->nearestAirport);

  for (i = 0; i < MAX_LEDS; ++i) {
    free(_cfg->ledAssignments[i]);
  }

  free(_cfg);
}

/**
 * @brief   Append a file name to a path prefix.
 * @details A POSIX-only function to append a trailing backslash to @a _prefix,
 *          if necessary, followed by @a _name.
 * @param[in] _prefix Null-terminated path prefix.
 * @param[in] _file   Null-terminated file name.
 * @param[in] _path   Output buffer. May not overlap @a _prefix or @a _file.
 * @param[in] _len    Length of @a _path.
 * @returns The buffer pointer or NULL if the output buffer is too small.
 */
static char *appendFileToPath(const char *_prefix, const char *_file,
                              char *_path, size_t _len) {
  size_t pl = strlen(_prefix), fl = strlen(_file);

  // If the prefix does not already have a trailing backslash, add one to the
  // length to account for adding the backslash.
  if (_prefix[pl - 1] != '/') {
    ++pl;
  }

  // If the prefix length + file length + null terminator is larger than the
  // buffer, return NULL.
  if (pl + fl + 1 >= _len) {
    return NULL;
  }

  // Make sure the prefix copies to the beginning of the output buffer.
  _path[0] = 0;

  strcat(_path, _prefix);

  // If the first check above did not find a trailing backslash, it increased
  // the length by one. `pl - 1` would now point to the null terminator. If it
  // did find a trailing backslash, then `pl - 1` still points to the backslash.
  // Overwrite the null terminator with the backslash, and set the new null
  // terminator so the file name copy occurs after the backslash.
  if (_path[pl - 1] != '/') {
    _path[pl - 1] = '/';
    _path[pl]     = 0;
  }

  strcat(_path, _file);

  return _path;
}

char *getPathForBitmap(const char *_file, char *_path, size_t _len) {
  return appendFileToPath(IMAGE_RESOURCES, _file, _path, _len);
}

char *getPathForFont(const char *_file, char *_path, size_t _len) {
  return appendFileToPath(FONT_RESOURCES, _file, _path, _len);
}
