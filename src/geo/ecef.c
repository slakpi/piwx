/**
 * @file ecef.c
 * @see https://en.wikipedia.org/wiki/Geographic_coordinate_conversion
 */
#include "geo.h"
#include "geo_prv.h"
#include <math.h>

#define WGS84_SEMI_MAJOR_M  6378137.0
#define WGS84_SEMI_MAJOR_M2 (WGS84_SEMI_MAJOR_M * WGS84_SEMI_MAJOR_M)
#define WGS84_SEMI_MINOR_M  6356752.314245
#define WGS84_SEMI_MINOR_M2 (WGS84_SEMI_MINOR_M * WGS84_SEMI_MINOR_M)

void geo_LatLonToECEF(double lat, double lon, float *x, float *y, float *z) {
  double lat_rad = lat * DEG_TO_RAD;
  double lon_rad = lon * DEG_TO_RAD;
  double cos_phi = cos(lat_rad);
  double sin_phi = sin(lat_rad);
  double e2      = 1.0 - (WGS84_SEMI_MINOR_M2 / WGS84_SEMI_MAJOR_M2);
  double N       = WGS84_SEMI_MAJOR_M / sqrt(1.0 - (e2 * sin_phi * sin_phi));

  *x = (float)(N * cos_phi * cos(lon_rad));
  *y = (float)(N * cos_phi * sin(lon_rad));
  *z = (float)((1.0 - e2) * N * sin_phi);
}
