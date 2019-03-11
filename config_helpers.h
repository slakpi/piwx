#ifndef CONFIG_HELPERS_H
#define CONFIG_HELPERS_H

typedef struct __PiwxConfig
{
  char *installPrefix;
  char *imageResources;
  char *fontResources;
  char *configFile;
  char *stationQuery;
  int cycleTime;
} PiwxConfig;

PiwxConfig* getPiwxConfig();
void freePiwxConfig(PiwxConfig *_cfg);
char* getPathForBitmap(const char *_file, char *_path, size_t _len);
char* getPathForFont(const char *_file, char *_path, size_t _len);

#endif
