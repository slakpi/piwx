/**
 * @file sun.c
 * @see https://github.com/buelowp/sunset/blob/master/src/sunset.cpp
 * @see https://en.wikipedia.org/wiki/Subsolar_point
 * @see https://gml.noaa.gov/grad/solcalc/calcdetails.html
 */
#include "geo.h"
#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <time.h>

#define SEC_PER_HOUR 3600
#define SEC_PER_DAY  86400
#define MIN_PER_DAY  1400
#define HOUR_PER_DAY 24

static bool calcAbsTime(double *absTime, double lat, double lon, double jd, double offset,
                        bool sunrise);

static double calcEccentricityEarthOrbit(double t);

static bool calcEquationOfTime(double *Etime, double t);

static double calcGeomMeanAnomalySun(double t);

static bool calcGeomMeanLonSun(double *lon, double t);

static double calcHourAngleSunrise(double lat, double solarDec, double offset);

static double calcHourAngleSunset(double lat, double solarDec, double offset);

static double calcJD(int y, int m, int d);

static double calcJDFromJulianCentury(double t);

static double calcMeanObliquityOfEcliptic(double t);

static double calcObliquityCorrection(double t);

static bool calcSunApparentLon(double *lon, double t);

static bool calcSunDeclination(double *decl, double t);

static double calcSunEqOfCenter(double t);

static bool calcSunTrueLon(double *lon, double t);

static time_t calcTime(int year, int month, int day, double minutes);

static double calcTimeJulianCentury(double jd);

static double getTwilightAngularValue(DaylightSpan daylight);

bool geo_calcDaylightSpan(double lat, double lon, DaylightSpan daylight, int year, int month,
                          int day, time_t *start, time_t *end) {
  double angle = getTwilightAngularValue(daylight);
  double jd    = calcJD(year, month, day);
  double absTime;

  // NOTE: The start and end times seem to be within a minute of online sources.
  //       This may be due to the Equation of Time which seems to be slightly
  //       off compared to online sources.

  if (!calcAbsTime(&absTime, lat, lon, jd, angle, true)) {
    return false;
  }

  *start = calcTime(year, month, day, absTime);

  if (!calcAbsTime(&absTime, lat, lon, jd, angle, false)) {
    return false;
  }

  *end = calcTime(year, month, day, absTime);

  return true;
}

bool geo_calcSubsolarPoint(time_t obsTime, double *lat, double *lon) {
  struct tm date;
  double    jd, t, eqTime;
  double    hrs = ((double)(obsTime % SEC_PER_DAY)) / SEC_PER_HOUR;

  gmtime_r(&obsTime, &date);

  jd = calcJD(date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
  jd += ((double)date.tm_hour / HOUR_PER_DAY) + ((double)date.tm_min / MIN_PER_DAY) +
        ((double)date.tm_sec / SEC_PER_DAY);
  t = calcTimeJulianCentury(jd);

  if (!calcSunDeclination(lat, t)) {
    return false;
  }

  if (!calcEquationOfTime(&eqTime, t)) {
    return false;
  }

  *lon = -15 * (hrs - 12 + (eqTime / 60.0));

  return true;
}

bool geo_isNight(double lat, double lon, DaylightSpan daylight, time_t obsTime) {
  time_t    t = obsTime;
  time_t    start, end;
  struct tm date;

  // Consider a report issued at 1753L on July 31 in the US Pacific Daylight
  // time zone. The UTC date/time is 0053Z on Aug 1, so `geo_calcDaylightSpan()`
  // will calculate the daylight span for Aug 1, not July 31. Using the Aug 1
  // data, 0053Z will be less than the sunrise time and `isNight()` would
  // indicate night time despite it being day time PDT.
  //
  // `isNight()` checks the previous or next day as necessary to make a
  // determination without knowing the station's local time zone.

  gmtime_r(&t, &date);

  if (!geo_calcDaylightSpan(lat, lon, daylight, date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                            &start, &end)) {
    return false;
  }

  // If the observation time is less than the daylight span start, check the
  // prior day. Check the next day if the observation time is greater than or
  // equal to the daylight span end. Otherwise, it is daylight.
  if (obsTime < start) {
    if (t < SEC_PER_DAY) {
      return false; // Underflow
    }

    t -= SEC_PER_DAY;
  } else if (obsTime >= end) {
    if (t + SEC_PER_DAY < t) {
      return false; // Overflow
    }

    t += SEC_PER_DAY;
  } else {
    return false; // Between sunrise and sunset; it's day time.
  }

  gmtime_r(&t, &date);

  if (!geo_calcDaylightSpan(lat, lon, daylight, date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                            &start, &end)) {
    return false;
  }

  // It's night time if greater than sunset on the previous day or less than
  // sunrise on the next day.
  return (obsTime >= end || obsTime < start);
}

/**
 * @brief   Get the twilight angular offset for a daylight span.
 * @param[in] daylight The daylight span of interest.
 * @returns The twlight angular offset in degrees.
 */
static double getTwilightAngularValue(DaylightSpan daylight) {
  switch (daylight) {
  default:
  case daylightOfficial:
    return GEO_DAY_OFFICIAL;
  case daylightCivil:
    return GEO_CIVIL_TWILIGHT;
  case daylightNautical:
    return GEO_NAUTICAL_TWILIGHT;
  case daylightAstronomical:
    return GEO_ASTONOMICAL_TWILIGHT;
  }
}

/**
 * @brief   Calculate the Julian day of the given date.
 * @param[in] y The year.
 * @param[in] m The month.
 * @param[in] d The day.
 * @returns The Julian day.
 */
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

/**
 * @brief   Calculates the UTC sunset or sunrise time at a given location.
 * @param[out] absTime UTC time (UNIX time).
 * @param[in]  lat     Latitude of the location (+N/-S) in degrees.
 * @param[in]  lon     Longitude of the location (+E/-W) in degrees.
 * @param[in]  jd      Julian day of interest.
 * @param[in]  offset  Agnular offset (official, civil, nautical, astronomical)
 *                     in degrees.
 * @param[in]  sunrise True if sunrise, false if sunset.
 * @returns True if able to calculate the sunset/sunrise time.
 */
static bool calcAbsTime(double *absTime, double lat, double lon, double jd, double offset,
                        bool sunrise) {
  double t = calcTimeJulianCentury(jd);
  double eqTime;
  double solarDec;
  double hourAngle;
  double delta;
  double timeDiff;
  double newt;

  if (!calcEquationOfTime(&eqTime, t)) {
    return false;
  }

  if (!calcSunDeclination(&solarDec, t)) {
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

  if (!calcEquationOfTime(&eqTime, newt)) {
    return false;
  }

  if (!calcSunDeclination(&solarDec, newt)) {
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

/**
 * @brief   Calculate the Julian century from a Julian day.
 * @param[in] jd The Julian day.
 * @returns The Julian century.
 */
static double calcTimeJulianCentury(double jd) {
  return (jd - 2451545.0) / 36525.0;
}

/**
 * @brief   Calculate the Julian day from a Julian century.
 * @param[in] t The Julian century.
 * @returns The Julian day.
 */
static double calcJDFromJulianCentury(double t) {
  return t * 36525.0 + 2451545.0;
}

/**
 * @brief   Calculate the solar equation of time.
 * @details The solar equation of time is the difference between noon and solar
 *          noon at a given point in time.
 * @param[out] Etime The equation of time.
 * @param[in]  t     The Julian century.
 * @returns True if able to calculate the equation of time, false otherwise.
 */
static bool calcEquationOfTime(double *Etime, double t) {
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

  if (!calcGeomMeanLonSun(&l0, t)) {
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

/**
 * @brief   Calculate the Sun's declination relative to the equator at a given
 *          point in time.
 * @param[out] decl The declination in degrees.
 * @param[in]  t    The Jualian century.
 * @returns True if able to calculate the declination, false otherwise.
 */
static bool calcSunDeclination(double *decl, double t) {
  double e = calcObliquityCorrection(t);
  double lon;
  double sint;

  if (!calcSunApparentLon(&lon, t)) {
    return false;
  }

  sint  = sin(e * DEG_TO_RAD) * sin(lon * DEG_TO_RAD);
  *decl = asin(sint) * RAD_TO_DEG;

  return true;
}

/**
 * @brief   Calculate the corrected obliquity of the ecliptic.
 * @see     https://en.wikipedia.org/wiki/Axial_tilt.
 * @param[in] t The Julian century.
 * @returns The corrected obliquity of the ecliptic.
 */
static double calcObliquityCorrection(double t) {
  double e0    = calcMeanObliquityOfEcliptic(t);
  double omega = 125.04 - t * 1934.136;
  return e0 + 0.00256 * cos(omega * DEG_TO_RAD);
}

/**
 * @brief   Calculate the solar hour angle for sunrise.
 * @see     https://en.wikipedia.org/wiki/Hour_angle.
 * @details Converts the angular offset relative to the location to a solar hour
 *          angle relative to solar noon.
 * @param[in] lat      The latitude of the location in degrees.
 * @param[in] solarDec The solar declination in degrees.
 * @param[in] offset   Angular offset (official, civil, nautical, astronomical)
 *                     in degrees.
 * @returns The sunrise hour angle in radians.
 */
static double calcHourAngleSunrise(double lat, double solarDec, double offset) {
  double latRad = lat * DEG_TO_RAD;
  double sdRad  = solarDec * DEG_TO_RAD;
  return (acos(cos(offset * DEG_TO_RAD) / (cos(latRad) * cos(sdRad)) - tan(latRad) * tan(sdRad)));
}

/**
 * @brief   Calculate the solar hour angle for sunset.
 * @see     @a calcHourAngleSunrise.
 * @param[in] lat      The latitude of the location in degrees.
 * @param[in] solarDec The solar declination in degrees.
 * @param[in] offset   Angular offset (official, civil, nautical, astronomical)
 *                     in degrees.
 * @returns The sunset hour angle in radians.
 */
static double calcHourAngleSunset(double lat, double solarDec, double offset) {
  return -calcHourAngleSunrise(lat, solarDec, offset);
}

/**
 * @brief   Convert the offset from midnight to a date.
 * @param[in] year    Year of interest.
 * @param[in] month   Month of interest.
 * @param[in] day     Day of interest.
 * @param[in] minutes Offset from midnight in minutes.
 * @returns The UTC date/time of interest (UNIX time).
 */
static time_t calcTime(int year, int month, int day, double minutes) {
  struct tm t = {
      .tm_year = year - 1900,
      .tm_mon  = month - 1,
      .tm_mday = day,
      .tm_hour = (int)(minutes / 60.),
      .tm_min  = (int)fmod(minutes, 60.0),
      .tm_sec  = (int)(fmod(minutes, 1.0) * 60),
  };

  return timegm(&t);
}

/**
 * @brief  Calculate the angle between Earth's orbit and its equator.
 * @param[in] t The Julian century.
 * @return The mean obliquity of the ecliptic.
 */
static double calcMeanObliquityOfEcliptic(double t) {
  double seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * (0.001813)));
  return 23.0 + (26.0 + (seconds / 60.0)) / 60.0;
}

/**
 * @brief   Calculate the eccentricity of Earth's orbit at a given point in
 *          time.
 * @param[in] t The Julian century.
 * @returns The eccentricity.
 */
static double calcEccentricityEarthOrbit(double t) {
  return 0.016708634 - t * (0.000042037 + t * 0.0000001267);
}

/**
 * @brief   Calculate the mean anomaly of Earth's orbit around the Sun at a
 *          given point in time.
 * @param[in] t The Julian century.
 * @returns The mean anomaly.
 */
static double calcGeomMeanAnomalySun(double t) {
  return 357.52911 + t * (35999.05029 - t * 0.0001537);
}

/**
 * @brief   Calculate the geometric mean longitude of the Sun.
 * @param[out] lon The geometric mean longitude of the Sun.
 * @param[in]  t   The Julian century.
 * @returns True if able to calculate the geometric mean longitude, false
 *          otherwise.
 */
static bool calcGeomMeanLonSun(double *lon, double t) {
  double l0;

  if (isnan(t)) {
    return false;
  }

  l0   = 280.46646 + t * (36000.76983 + t * 0.0003032);
  *lon = fmod(l0, 360.0);

  return true;
}

/**
 * @brief   Calculate the apparent longitude of the Sun.
 * @see     https://en.wikipedia.org/wiki/Apparent_longitude.
 * @param[out] lon The apparent longitude of the Sun.
 * @param[in]  t   The Julian century.
 * @returns True if able to calculate the apparent longitude, false otherwise.
 */
static bool calcSunApparentLon(double *decl, double t) {
  double omega = 125.04 - 1934.136 * t;
  double lon;

  if (!calcSunTrueLon(&lon, t)) {
    return false;
  }

  *decl = lon - 0.00569 - 0.00478 * sin(omega * DEG_TO_RAD);

  return true;
}

/**
 * @brief   Calculate the true longitude of the Sun.
 * @param[out] lon The true longitude of the Sun.
 * @param[in]  t   The Julian century.
 * @returns True if able to calculate the true longitude, false otherwise.
 */
static bool calcSunTrueLon(double *lon, double t) {
  double c = calcSunEqOfCenter(t);
  double l0;

  if (!calcGeomMeanLonSun(&l0, t)) {
    return false;
  }

  *lon = l0 + c;

  return true;
}

/**
 * @brief   Calculate the equation of center for the Sun.
 * @see     https://en.wikipedia.org/wiki/Equation_of_the_center.
 * @param[in] t The Julian century.
 * @returns The angular difference between the Sun's actual center and the ideal
 *          center with uniform motion in radians.
 */
static double calcSunEqOfCenter(double t) {
  double m     = calcGeomMeanAnomalySun(t);
  double mrad  = m * DEG_TO_RAD;
  double sinm  = sin(mrad);
  double sin2m = sin(mrad + mrad);
  double sin3m = sin(mrad + mrad + mrad);
  return sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) +
         sin3m * 0.000289;
}
