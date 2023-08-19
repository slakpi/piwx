/**
 * @file geo.h
 */
#if !defined GEO_H
#define GEO_H

#include <stdbool.h>
#include <time.h>

#define DAY_OFFICIAL         90.833
#define CIVIL_TWILIGHT       96.0
#define NAUTICAL_TWILIGHT    102.0
#define ASTONOMICAL_TWILIGHT 108.0

/**
 * @brief   Calculate the daylight span at the given location and date.
 * @param[in]  lat     Latitude of the location (+N/-S) in degrees.
 * @param[in]  lon     Longitude of the location (+E/-W) in degrees.
 * @param[in]  offset  The twilight offset to use.
 * @param[in]  year    Year of transit.
 * @param[in]  month   Month of transit.
 * @param[in]  day     Day of transit.
 * @param[out] start   UTC time (UNIX time) of transit start.
 * @param[out] end     UTC time (UNIX time) of transit end.
 * @returns True if able to calculate the transit times, false otherwise.
 */
bool geo_calcDaylightSpan(double lat, double lon, double offset, int year, int month, int day,
                          time_t *start, time_t *end);

/**
 * @brief   Checks if the given observation time is night at the given location.
 * @param[in] lat     Observation latitude.
 * @param[in] lon     Observation longitude.
 * @param[in] obsTime UTC observation time (UNIX time).
 * @returns True if night, false if day or there is an error.
 */
bool geo_isNight(double lat, double lon, time_t obsTime);

#endif
