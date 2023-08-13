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
 * @brief   Calculate the sun transit times at the given location and date.
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
bool calcSunTransitTimes(double lat, double lon, double offset, int year, int month, int day,
                         time_t *start, time_t *end);

#endif
