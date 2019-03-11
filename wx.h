#ifndef WX_H
#define WX_H

#include <time.h>

typedef enum __CloudCover
{
  skyInvalid,
  skyScattered,
  skyFew,
  skyBroken,
  skyOvercast
} CloudCover;

typedef struct __SkyCondition
{
  CloudCover coverage;
  int height;
  struct __SkyCondition *next;
} SkyCondition;

typedef enum __FlightCategory
{
  catInvalid,
  catVFR,
  catMVFR,
  catIFR,
  catLIFR
} FlightCategory;

typedef struct __WxStation
{
  char *id;
  char *raw;
  time_t obsTime;
  double lat, lon;
  time_t sunrise, sunset;
  char *wxString;
  SkyCondition *layers;
  int windDir, windSpeed;
  int visibility;
  double temp, dewPoint;
  double alt;
  FlightCategory cat;
  struct __WxStation *next;
} WxStation;

WxStation* queryWx(const char *_stations);
void freeStations(WxStation *_stations);

#endif
