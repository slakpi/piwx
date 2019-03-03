#ifndef WX_H
#define WX_H

#include <time.h>

int getSunriseSunset(double _lat, double _long, time_t *_sunrise,
  time_t *_sunset);

#endif
