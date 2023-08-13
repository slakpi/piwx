/**
 * @file geo.h
 */
#if !defined GEO_H
#define GEO_H

#include <stdbool.h>
#include <time.h>

/**
 * @brief   Calculate the sun transit times at the given location and date.
 * @param[in]  lat     Latitude of the location (+N/-S) in degrees.
 * @param[in]  lon     Longitude of the location (+E/-W) in degrees.
 * @param[in]  year    Year of transit.
 * @param[in]  month   Month of transit.
 * @param[in]  day     Day of transit.
 * @param[out] sunrise UTC sunrise time (UNIX time).
 * @param[out] sunset  UTC sunset time (UNIX time).
 * @returns True if able to calculate the transit times, false otherwise.
 */
bool calcSunTransitTimes(double lat, double lon, int year, int month, int day, time_t *sunrise,
                         time_t *sunset);

#endif
