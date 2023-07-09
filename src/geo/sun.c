/**
 * @file sun.c
 */
#include "geo.h"
#include <math.h>
#include <stdbool.h>

#define RAD_TO_DEG (180.0 / M_PI)
#define DEG_TO_RAD (M_PI / 180.0)

/**
 * \see https://github.com/buelowp/sunset/blob/master/src/sunset.cpp
 */
bool calcSunTransitTimes(double lat, double lon, const struct tm *date, time_t *sunrise,
                         time_t *sunset) {
  return false;
}
