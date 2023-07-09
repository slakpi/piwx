/**
 * @file geo.h
 */
#if !defined GEO_H
#define GEO_H

#include <stdbool.h>
#include <time.h>

/**
 * @brief   Calculate the sun transit times at the given location and date.
 * @param[in]  lat     Latitude of the location (+N/-S).
 * @param[in]  lon     Longitude of the location (+E/-W).
 * @param[in]  date    UTC date of transit.
 * @param[out] sunrise UTC sunrise time.
 * @param[out] sunset  UTC sunset time.
 * @returns True if able to calculate the transit times, false otherwise.
 */
bool calcSunTransitTimes(double lat, double lon, const struct tm *date, time_t *sunrise,
                         time_t *sunset);

#endif
