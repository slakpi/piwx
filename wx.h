#ifndef WX_H
#define WX_H

#include <time.h>

typedef enum __CloudCover
{
  skyInvalid,
  skyClear,
  skyScattered,
  skyFew,
  skyBroken,
  skyOvercast,
  skyOvercastSurface
} CloudCover;

typedef struct __SkyCondition
{
  CloudCover coverage;
  int height;
  struct __SkyCondition *prev, *next;
} SkyCondition;

typedef enum __FlightCategory
{
  catInvalid,
  catVFR,
  catMVFR,
  catIFR,
  catLIFR
} FlightCategory;

typedef enum __DominantWeather
{
  wxInvalid,
  wxClearDay,             /* Clear; no other weather. */
  wxClearNight,
  wxScatteredOrFewDay,    /* Scattered or few; no other weather. */
  wxScatteredOrFewNight,
  wxBrokenDay,            /* Broken; no other weather. */
  wxBrokenNight,
  wxOvercast,             /* Overcast; no other weather. */
  wxLightMistHaze,        /* BR, HZ */
  wxLightDrizzleRain,     /* VC/- DZ, RA */
  wxRain,                 /* [+] DZ, RA */
  wxFlurries,             /* VC SN, SG */
  wxLightSnow,            /* - SN, SG */
  wxSnow,                 /* [+] SN, SG */
  wxLightFreezingRain,    /* VC/- FZ + DZ or RA, IC, PL, GR, GS */
  wxFreezingRain,         /* [+] FZ + DZ or RA, IC, PL, GR, GS */
  wxObscuration,          /* FG, FU, DU, SS, DS */
  wxVolcanicAsh,          /* VA */
  wxLightTstormsSqualls,  /* VC/- TS, SQ */
  wxTstormsSqualls,       /* [+] TS, SQ */
  wxFunnelCloud,          /* FC */
} DominantWeather;

typedef struct __WxStation
{
  char *id;
  char *localId;
  char *raw;
  time_t obsTime;
  double lat, lon;
  int isNight;
  DominantWeather wx;
  char *wxString;
  SkyCondition *layers;
  int windDir, windSpeed, windGust;
  int visibility, vertVis;
  double temp, dewPoint;
  double alt;
  FlightCategory cat;
  int blinkState;
  struct __WxStation *next;
  struct __WxStation *prev;
} WxStation;

WxStation* queryWx(const char *_stations, int *err);
void freeStations(WxStation *_stations);

#endif
