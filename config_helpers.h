#ifndef CONFIG_HELPERS_H
#define CONFIG_HELPERS_H

#define MAX_LEDS 50

typedef struct __PiwxConfig
{
  char *installPrefix;
  char *imageResources;
  char *fontResources;
  char *configFile;
  char *stationQuery;
  char *nearestAirport;
  int cycleTime;
  char* ledAssignments[MAX_LEDS];
  float ledBrightness;
  float ledNightBrightness;
  int ledDataPin;
  int ledDMAChannel;
} PiwxConfig;

PiwxConfig* getPiwxConfig();
void freePiwxConfig(PiwxConfig *_cfg);
char* getPathForBitmap(const char *_file, char *_path, size_t _len);
char* getPathForFont(const char *_file, char *_path, size_t _len);

#endif
