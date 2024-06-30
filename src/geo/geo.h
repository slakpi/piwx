/**
 * @file geo.h
 * @defgroup GeoModule Geographic Module
 * @{
 */
#if !defined GEO_H
#define GEO_H

#include <stdbool.h>
#include <time.h>

#define GEO_DAY_OFFICIAL         90.833
#define GEO_CIVIL_TWILIGHT       96.0
#define GEO_NAUTICAL_TWILIGHT    102.0
#define GEO_ASTONOMICAL_TWILIGHT 108.0

#define GEO_WGS84_SEMI_MAJOR_M  6378137.0
#define GEO_WGS84_SEMI_MAJOR_M2 (GEO_WGS84_SEMI_MAJOR_M * GEO_WGS84_SEMI_MAJOR_M)
#define GEO_WGS84_SEMI_MINOR_M  6356752.314245
#define GEO_WGS84_SEMI_MINOR_M2 (GEO_WGS84_SEMI_MINOR_M * GEO_WGS84_SEMI_MINOR_M)

/**
 * @struct Position
 * @brief  Position expressed in degrees of latitude and longitude.
 */
typedef struct {
  double lat, lon;
} Position;

/**
 * @enum  DaylightSpan
 * @brief Daylight spans.
 */
typedef enum {
  daylightOfficial,
  daylightCivil,
  daylightNautical,
  daylightAstronomical,
  daylightSpanCount
} DaylightSpan;

/**
 * @brief   Calculate the daylight span at the given location and date.
 * @param[in]  pos      The observation position.
 * @param[in]  daylight The daylight span to calculate.
 * @param[in]  year     Year of transit.
 * @param[in]  month    Month of transit.
 * @param[in]  day      Day of transit.
 * @param[out] start    UTC time (UNIX time) of transit start.
 * @param[out] end      UTC time (UNIX time) of transit end.
 * @returns True if able to calculate the transit times, false otherwise.
 */
bool geo_calcDaylightSpan(Position pos, DaylightSpan daylight, int year, int month, int day,
                          time_t *start, time_t *end);

/**
 * @brief Calculate the coordinates of the subsolar point at a given time.
 * @param[out] pos     The subsolar position.
 * @param[in]  obsTime The UTC observation date/time.
 */
bool geo_calcSubsolarPoint(Position *pos, time_t obsTime);

/**
 * @brief   Checks if the given observation time is night at the given location.
 * @details The daylight span is considered a half-open interval that does not
 *          include the end time of the span.
 * @param[in] pos      The observation position.
 * @param[in] obsTime  UTC observation time (UNIX time).
 * @param[in] daylight The daylight span defining "day".
 * @returns True if night, false if day or there is an error.
 */
bool geo_isNight(Position pos, time_t obsTime, DaylightSpan daylight);

/**
 * @brief   Converts latitude / longitude coordinates to WGS84 ECEF.
 * @details The X axis runs from 0 to +/-180 degrees longitude at the Equator.
 *          The Y axis runs from 90 to -90 degrees longitude at the Equator. The
 *          Z axis is the Polar axis.
 * @param[in]  pos The position.
 * @param[out] x   ECEF X coordinate in meters.
 * @param[out] y   ECEF Y coordinate in meters.
 * @param[out] z   ECEF Z coordinate in meters.
 */
void geo_latLonToECEF(Position pos, float *x, float *y, float *z);

#endif /* GEO_H @} */
