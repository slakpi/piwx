/**
 * @file wx.h
 */
#ifndef WX_H
#define WX_H

#include <time.h>

/**
 * @enum  CloudCover
 * @brief METAR cloud cover levels.
 */
typedef enum {
  skyInvalid,
  skyClear,
  skyScattered,
  skyFew,
  skyBroken,
  skyOvercast,
  skyOvercastSurface
} CloudCover;

/**
 * @struct SkyCondition
 * @brief  METAR cloud layer entry.
 */
typedef struct SkyCondition_ {
  CloudCover            coverage;
  int                   height;
  struct SkyCondition_ *prev, *next;
} SkyCondition;

/**
 * @enum  FlightCategory
 * @brief METAR flight category.
 */
typedef enum { catInvalid, catVFR, catMVFR, catIFR, catLIFR } FlightCategory;

/**
 * @enum  DominantWeather
 * @brief The dominant weather phenomenon a weather station. Only clear,
 *        scattered, and broken need night variants. The items are ordered by
 *        severity priority. E.g. Funnel clouds are a more severe phenomenon
 *        than freezing rain.
 */
typedef enum {
  wxInvalid,
  wxClearDay, // Clear; no other weather.
  wxClearNight,
  wxScatteredOrFewDay, // Scattered or few; no other weather.
  wxScatteredOrFewNight,
  wxBrokenDay, // Broken; no other weather.
  wxBrokenNight,
  wxOvercast,            // Overcast; no other weather.
  wxLightMistHaze,       // BR, HZ
  wxLightDrizzleRain,    // VC/- DZ, RA
  wxRain,                // [+] DZ, RA
  wxFlurries,            // VC SN, SG
  wxLightSnow,           // - SN, SG
  wxSnow,                // [+] SN, SG
  wxLightFreezingRain,   // VC/- FZ + DZ or RA, IC, PL, GR, GS
  wxFreezingRain,        // [+] FZ + DZ or RA, IC, PL, GR, GS
  wxObscuration,         // FG, FU, DU, SS, DS
  wxVolcanicAsh,         // VA
  wxLightTstormsSqualls, // VC/- TS, SQ
  wxTstormsSqualls,      // [+] TS, SQ
  wxFunnelCloud,         // FC
} DominantWeather;

/**
 * @struct WxStation
 * @brief  Weather station data entry.
 */
typedef struct WxStation_ {
  char              *id;
  char              *localId;
  char              *raw;
  time_t             obsTime;
  double             lat, lon;
  int                isNight;
  DominantWeather    wx;
  char              *wxString;
  SkyCondition      *layers;
  int                windDir, windSpeed, windGust;
  double             visibility;
  int                vertVis;
  double             temp, dewPoint;
  double             alt;
  FlightCategory     cat;
  int                blinkState;
  struct WxStation_ *next;
  struct WxStation_ *prev;
} WxStation;

/**
 * @brief   Query the weather source for a comma-separated list of stations.
 * @param[in]  stations The list of stations to query.
 * @param[out] err      Query error code.
 * @returns A pointer to the head of a circular list of weather station entries
 *          or null if there is an error.
 */
WxStation *queryWx(const char *stations, int *err);

/**
 * @brief Frees a list of weather stations.
 * @param[in] stations The list of stations to free.
 */
void freeStations(WxStation *stations);

#endif /* WX_H */