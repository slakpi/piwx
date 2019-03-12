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

typedef enum __DominantWeather
{
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
  char *raw;
  time_t obsTime;
  double lat, lon;
  time_t sunrise, sunset;
  DominantWeather wx;
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
