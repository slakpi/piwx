/**
 * @file sun.c
 */
#include "geo.h"
#include <math.h>
#include <stdbool.h>

#define PI                 3.1415926536
#define RAD_TO_DEG         (180.0 / PI)
#define DEG_TO_RAD         (PI / 180.0)
#define SUNSET_OFFICIAL    90.833
#define SUNSET_NAUTICAL    102.0
#define SUNSET_CIVIL       96.0
#define SUNSET_ASTONOMICAL 108.0

static bool calcAbsTime(double lat, double lon, double jd, double offset, bool sunrise,
                        double *absTime);

static double calcEccentricityEarthOrbit(double t);

static bool calcEquationOfTime(double t, double *Etime);

static double calcGeomMeanAnomalySun(double t);

static bool calcGeomMeanLonSun(double t, double *lon);

static double calcHourAngleSunrise(double lat, double solarDec, double offset);

static double calcHourAngleSunset(double lat, double solarDec, double offset);

static double calcJD(int y, int m, int d);

static double calcJDFromJulianCentury(double t);

static double calcMeanObliquityOfEcliptic(double t);

static double calcObliquityCorrection(double t);

static bool calcSunApparentLon(double t, double *lon);

static bool calcSunDeclination(double t, double *decl);

static double calcSunEqOfCenter(double t);

static bool calcSunTrueLon(double t, double *lon);

static time_t calcTime(int year, int month, int day, double minutes);

static double calcTimeJulianCentury(double jd);

/**
 * \see https://github.com/buelowp/sunset/blob/master/src/sunset.cpp
 */
bool calcSunTransitTimes(double lat, double lon, int year, int month, int day, time_t *sunrise,
                         time_t *sunset) {
  double jd = calcJD(year, month, day);
  double absTime;

  if (!calcAbsTime(lat, lon, jd, SUNSET_CIVIL, true, &absTime)) {
    return false;
  }

  *sunrise = calcTime(year, month, day, absTime);

  if (!calcAbsTime(lat, lon, jd, SUNSET_CIVIL, false, &absTime)) {
    return false;
  }

  *sunset = calcTime(year, month, day, absTime);

  return true;
}

static bool calcAbsTime(double lat, double lon, double jd, double offset, bool sunrise,
                        double *absTime) {
  double t = calcTimeJulianCentury(jd);
  double eqTime;
  double solarDec;
  double hourAngle;
  double delta;
  double timeDiff;
  double newt;

  if (!calcEquationOfTime(t, &eqTime)) {
    return false;
  }

  if (!calcSunDeclination(t, &solarDec)) {
    return false;
  }

  if (sunrise) {
    hourAngle = calcHourAngleSunrise(lat, solarDec, offset);
  } else {
    hourAngle = calcHourAngleSunset(lat, solarDec, offset);
  }

  delta    = lon + (hourAngle * RAD_TO_DEG);
  timeDiff = 4 * delta;
  *absTime = 720 - timeDiff - eqTime;
  newt     = calcTimeJulianCentury(calcJDFromJulianCentury(t) + (*absTime / 1440.0));

  if (!calcEquationOfTime(newt, &eqTime)) {
    return false;
  }

  if (!calcSunDeclination(newt, &solarDec)) {
    return false;
  }

  if (sunrise) {
    hourAngle = calcHourAngleSunrise(lat, solarDec, offset);
  } else {
    hourAngle = calcHourAngleSunset(lat, solarDec, offset);
  }

  delta    = lon + (hourAngle * RAD_TO_DEG);
  timeDiff = 4 * delta;
  *absTime = 720 - timeDiff - eqTime;

  return true;
}

static time_t calcTime(int year, int month, int day, double minutes) {
  struct tm t = {
      .tm_year = year - 1900,
      .tm_mon  = month - 1,
      .tm_mday = day,
      .tm_hour = (int)(minutes / 60.),
      .tm_min  = (int)fmod(minutes, 60.0),
      .tm_sec  = (int)(fmod(minutes, 1.0) * 60),
  };

  return mktime(&t);
}

static bool calcEquationOfTime(double t, double *Etime) {
  double epsilon = calcObliquityCorrection(t);
  double e       = calcEccentricityEarthOrbit(t);
  double m       = calcGeomMeanAnomalySun(t);
  double y       = tan((epsilon * DEG_TO_RAD) / 2.0);
  double l0;
  double sin2l0;
  double sinm;
  double cos2l0;
  double sin4l0;
  double sin2m;

  if (!calcGeomMeanLonSun(t, &l0)) {
    return false;
  }

  y *= y;

  sin2l0 = sin(2.0 * (l0 * DEG_TO_RAD));
  sinm   = sin(m * DEG_TO_RAD);
  cos2l0 = cos(2.0 * (l0 * DEG_TO_RAD));
  sin4l0 = sin(4.0 * (l0 * DEG_TO_RAD));
  sin2m  = sin(2.0 * (m * DEG_TO_RAD));
  *Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 -
           1.25 * e * e * sin2m;
  *Etime = (*Etime * RAD_TO_DEG) * 4.0;

  return true;
}

static double calcMeanObliquityOfEcliptic(double t) {
  double seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * (0.001813)));
  return 23.0 + (26.0 + (seconds / 60.0)) / 60.0;
}

static bool calcGeomMeanLonSun(double t, double *lon) {
  double L;

  if (isnan(t)) {
    return false;
  }

  L    = 280.46646 + t * (36000.76983 + t * 0.0003032);
  *lon = fmod(L, 360.0);

  return true;
}

static double calcObliquityCorrection(double t) {
  double e0    = calcMeanObliquityOfEcliptic(t);
  double omega = 125.04 - t * 1934.136;
  return e0 + 0.00256 * cos(omega * DEG_TO_RAD);
}

static double calcEccentricityEarthOrbit(double t) {
  return 0.016708634 - t * (0.000042037 + t * 0.0000001267);
}

static double calcGeomMeanAnomalySun(double t) {
  return 357.52911 + t * (35999.05029 - t * 0.0001537);
}

static double calcTimeJulianCentury(double jd) { return (jd - 2451545.0) / 36525.0; }

static bool calcSunDeclination(double t, double *decl) {
  double e = calcObliquityCorrection(t);
  double lambda;
  double sint;

  if (!calcSunApparentLon(t, &lambda)) {
    return false;
  }

  sint  = sin(e * DEG_TO_RAD) * sin(lambda * DEG_TO_RAD);
  *decl = asin(sint) * RAD_TO_DEG;

  return true;
}

static bool calcSunApparentLon(double t, double *decl) {
  double omega = 125.04 - 1934.136 * t;
  double o;

  if (!calcSunTrueLon(t, &o)) {
    return false;
  }

  *decl = o - 0.00569 - 0.00478 * sin(omega * DEG_TO_RAD);

  return true;
}

static bool calcSunTrueLon(double t, double *lon) {
  double c = calcSunEqOfCenter(t);
  double l0;

  if (!calcGeomMeanLonSun(t, &l0)) {
    return false;
  }

  *lon = l0 + c;

  return true;
}

static double calcSunEqOfCenter(double t) {
  double m     = calcGeomMeanAnomalySun(t);
  double mrad  = m * DEG_TO_RAD;
  double sinm  = sin(mrad);
  double sin2m = sin(mrad + mrad);
  double sin3m = sin(mrad + mrad + mrad);
  double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) +
             sin3m * 0.000289;

  return C;
}

static double calcHourAngleSunrise(double lat, double solarDec, double offset) {
  double latRad = lat * DEG_TO_RAD;
  double sdRad  = solarDec * DEG_TO_RAD;
  double HA =
      (acos(cos(offset * DEG_TO_RAD) / (cos(latRad) * cos(sdRad)) - tan(latRad) * tan(sdRad)));
  return HA;
}

static double calcHourAngleSunset(double lat, double solarDec, double offset) {
  return -calcHourAngleSunrise(lat, solarDec, offset);
}

static double calcJD(int y, int m, int d) {
  double A;
  double B;

  if (m <= 2) {
    y -= 1;
    m += 12;
  }

  A = floor(y / 100);
  B = 2.0 - A + floor(A / 4);
  return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

static double calcJDFromJulianCentury(double t) { return t * 36525.0 + 2451545.0; }
