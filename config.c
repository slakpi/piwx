#include <string.h>
#include <config.h>

typedef void* yyscan_t;

#include "config_helpers.h"
#include "conf_file.h"
#include "conf_file.parser.h"
#include "conf_file.lexer.h"

PiwxConfig* getPiwxConfig()
{
  FILE *cfgFile;
  yyscan_t scanner;
  PiwxConfig *cfg = (PiwxConfig*)malloc(sizeof(PiwxConfig));

  cfg->installPrefix = strdup(INSTALL_PREFIX);
  cfg->imageResources = strdup(IMAGE_RESOURCES);
  cfg->fontResources = strdup(FONT_RESOURCES);
  cfg->configFile = strdup(CONFIG_FILE);
  cfg->stationQuery = NULL;
  cfg->cycleTime = 60;

  cfgFile = fopen(CONFIG_FILE, "r");
  if (!cfgFile)
    return cfg;

  yylex_init(&scanner);
  yyset_in(cfgFile, scanner);
  yyparse(scanner, cfg);
  yylex_destroy(scanner);

  fclose(cfgFile);

  return cfg;
}

void freePiwxConfig(PiwxConfig *_cfg)
{
  if (_cfg->installPrefix)
    free(_cfg->installPrefix);
  if (_cfg->imageResources)
    free(_cfg->imageResources);
  if (_cfg->fontResources)
    free(_cfg->fontResources);
  if (_cfg->configFile)
    free(_cfg->configFile);
  if (_cfg->stationQuery)
    free(_cfg->stationQuery);

  free(_cfg);
}

static char* appendFileToPath(const char *_prefix, const char *_file, char *_path, size_t _len)
{
  size_t pl = strlen(_prefix), fl = strlen(_file);

  if (_prefix[pl - 1] != '/')
    ++pl;
  if (pl + fl + 1 >= _len)
    return NULL;

  _path[0] = 0;

  strcat(_path, _prefix);

  if (_path[pl - 1] != '/')
  {
    _path[pl - 1] = '/';
    _path[pl] = 0;
  }

  strcat(_path, _file);

  return _path;
}

char* getPathForBitmap(const char *_file, char *_path, size_t _len)
{
  return appendFileToPath(IMAGE_RESOURCES, _file, _path, _len);
}

char* getPathForFont(const char *_file, char *_path, size_t _len)
{
  return appendFileToPath(FONT_RESOURCES, _file, _path, _len);
}
