/**
 * @file geo.h
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
 * @enum  DaylightSpan
 * @brief Daylight spans.
 */
typedef enum {
  daylightOfficial,
  daylightCivil,
  daylightNautical,
  daylightAstronomical
} DaylightSpan;

/**
 * @brief   Calculate the daylight span at the given location and date.
 * @param[in]  lat      Latitude of the location (+N/-S) in degrees.
 * @param[in]  lon      Longitude of the location (+E/-W) in degrees.
 * @param[in]  daylight The daylight span to calculate.
 * @param[in]  year     Year of transit.
 * @param[in]  month    Month of transit.
 * @param[in]  day      Day of transit.
 * @param[out] start    UTC time (UNIX time) of transit start.
 * @param[out] end      UTC time (UNIX time) of transit end.
 * @returns True if able to calculate the transit times, false otherwise.
 */
bool geo_calcDaylightSpan(double lat, double lon, DaylightSpan daylight, int year, int month,
                          int day, time_t *start, time_t *end);

/**
 * @brief Calculate the coordinates of the subsolar point at a given time.
 * @param[in]  obsTime The UTC observation date/time.
 * @param[out] lat     The latitude of the subsolar point in degrees.
 * @param[out] lon     The longitude of the subsolar point in degrees.
 */
bool geo_calcSubsolarPoint(time_t obsTime, double *lat, double *lon);

/**
 * @brief   Checks if the given observation time is night at the given location.
 * @details The daylight span is considered a half-open interval that does not
 *          include the end time of the span.
 * @param[in] lat      Observation latitude.
 * @param[in] lon      Observation longitude.
 * @param[in] daylight The daylight span defining "day".
 * @param[in] obsTime  UTC observation time (UNIX time).
 * @returns True if night, false if day or there is an error.
 */
bool geo_isNight(double lat, double lon, DaylightSpan daylight, time_t obsTime);

/**
 * @brief   Converts latitude / longitude coordinates to WGS84 ECEF.
 * @details The X axis runs from 0 to +/-180 degrees longitude at the Equator.
 *          The Y axis runs from 90 to -90 degrees longitude at the Equator. The
 *          Z axis is the Polar axis.
 * @param[in]  lat Input latitude in degrees.
 * @param[in]  lon Input longitude in degrees.
 * @param[out] x   ECEF X coordinate in meters.
 * @param[out] y   ECEF Y coordinate in meters.
 * @param[out] z   ECEF Z coordinate in meters.
 */
void geo_LatLonToECEF(double lat, double lon, float *x, float *y, float *z);

#endif
