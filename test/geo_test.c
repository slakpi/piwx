#include "geo.h"
#include "util.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

// Allows up to a tenth of a millimeter of error in ECEF calculations.
#define ECEF_EPSILON 1e-4

// Allows up to a minute of error in subsolar calculations.
#define SUBSOLAR_EPSILON (1.0 / 60.0)

typedef struct {
  time_t start;
  time_t end;
} Span;

typedef struct {
  int    y;
  int    m;
  int    d;
  double lat;
  double lon;
  Span   exp[4];
} SpanTestCase;

typedef struct {
  double lat;
  double lon;
  time_t obsTime;
  bool   exp[4];
} NightTestCase;

typedef struct {
  double lat;
  double lon;
  float x;
  float y;
  float z;
} LatLonToECEFCase;

typedef struct {
  time_t obsTime;
  double lat;
  double lon;
} SubsolarTestCase;

typedef bool (*TestFn)(void);

// clang-format off
static const DaylightSpan gSpanOrder[] = {
  daylightOfficial,
  daylightCivil,
  daylightNautical,
  daylightAstronomical
};
// clang-format on

// Verified against https://www.timeanddate.com/sun/. Values are within
// +/- 1 minute of the values calculated by timeanddate.com.
// clang-format off
static const SpanTestCase gSpanTestCases[] = {
  // Aurora, OR; 2023-07-15
  {2023, 7, 15, 45.2471264, -122.7700469, {
    {1689424666, 1689479752},
    {1689422495, 1689481916},
    {1689419664, 1689484733},
    {1689416120, 1689488243}
  }},
  // Brussels, Belgium; 2019-08-11
  {2019, 8, 11, 50.88232013304212, 4.365450383031231, {
    {1565497381, 1565550691},
    {1565495114, 1565552945},
    {1565492189, 1565555845},
    {1565488591, 1565559385}
  }},
  // Tokyo, Japan; 2016-10-08
  {2016, 10, 8, 35.768812179426085, 139.78138877967353, {
    {1475872887, 1475914489},
    {1475871352, 1475916022},
    {1475869576, 1475917796},
    {1475867796, 1475919573}
  }},
  // Denver, CO; 1994-08-01
  {1994, 8, 1, 39.77249602297143, -105.00703387541155, {
    {775742328, 775793594},
    {775740517, 775795400},
    {775738287, 775797619},
    {775735840, 775800051}
  }},
  // Clearwater, FL; 1979-04-22
  {1979, 4, 22, 27.95429332816202, -82.80341673562455, {
    {293626791, 293673605},
    {293625335, 293675065},
    {293623612, 293676791},
    {293621845, 293678565}
  }}
};
// clang-format on

// clang-format off
static const NightTestCase gNightTestCases[] = {
  // Aurora, OR; The start of official daylight.
  {45.2471264, -122.7700469, 1689424666, {
    false, false, false, false
  }},
  // Aurora, OR; 1 second before the start of official daylight.
  {45.2471264, -122.7700469, 1689424665, {
    true, false, false, false
  }},
  // Aurora, OR; The start of civil twilight.
  {45.2471264, -122.7700469, 1689422495, {
    true, false, false, false
  }},
  // Aurora, OR; 1 second before the start of civil twilight.
  {45.2471264, -122.7700469, 1689422494, {
    true, true, false, false
  }},
  // Aurora, OR; The start of nautical twilight.
  {45.2471264, -122.7700469, 1689419664, {
    true, true, false, false
  }},
  // Aurora, OR; 1 second before the start of nautical twilight.
  {45.2471264, -122.7700469, 1689419663, {
    true, true, true, false
  }},
  // Aurora, OR; The start of astronomical twilight.
  {45.2471264, -122.7700469, 1689416120, {
    true, true, true, false
  }},
  // Aurora, OR; 1 second before the start of astronomical twilight.
  {45.2471264, -122.7700469, 1689416119, {
    true, true, true, true
  }},
  // Tokyo, Japan; 1 second before the end of official daylight.
  {35.768812179426085, 139.78138877967353, 1475914488, {
    false, false, false, false
  }},
  // Tokyo, Japan; The end of official daylight.
  {35.768812179426085, 139.78138877967353, 1475914489, {
    true, false, false, false
  }},
  // Tokyo, Japan; 1 second before the end of civil twilight.
  {35.768812179426085, 139.78138877967353, 1475916021, {
    true, false, false, false
  }},
  // Tokyo, Japan; The end of civil twilight.
  {35.768812179426085, 139.78138877967353, 1475916022, {
    true, true, false, false
  }},
  // Tokyo, Japan; 1 second before the end of nautical twilight.
  {35.768812179426085, 139.78138877967353, 1475917795, {
    true, true, false, false
  }},
  // Tokyo, Japan; The end of nautical twilight.
  {35.768812179426085, 139.78138877967353, 1475917796, {
    true, true, true, false
  }},
  // Tokyo, Japan; 1 second before the end of astronomical twilight.
  {35.768812179426085, 139.78138877967353, 1475919572, {
    true, true, true, false
  }},
  // Tokyo, Japan; The end of astronomical twilight.
  {35.768812179426085, 139.78138877967353, 1475919573, {
    true, true, true, true
  }}
};
// clang-format on

// Any coordinate less than ECEF_EPSILON is zeroed out.
// clang-format off
static const LatLonToECEFCase gECEFCases[] = {
  {90, 0, 0.0f, 0.0f, 6356752.314245f},
  {45, 0, 4517590.878837506f, 0.0f, 4487348.408877217f},
  {45, 45, 3194419.1450443445f, 3194419.1450606463f, 4487348.408877217f},
  {45, 90, 0.0f, 4517590.878837506f, 4487348.408877217f},
  {45, 135, -3194419.145076949f, 3194419.145028042f, 4487348.408877217f},
  {45, 180, -4517590.878837506f, 0.0f, 4487348.408877217f},
  {45, -135, -3194419.145076949f, -3194419.145028042f, 4487348.408877217f},
  {45, -90, 0.0f, -4517590.878837506f, 4487348.408877217f},
  {45, -45, 3194419.1450443445f, -3194419.1450606463f, 4487348.408877217f},
  {90, 0, 0.0f, 0.0f, 6356752.314245f},
  {90, 45, 0.0f, 0.0f, 6356752.314245f},
  {90, 135, 0.0f, 0.0f, 6356752.314245f},
  {90, 180, 0.0f, 0.0f, 6356752.314245f},
  {90, 225, 0.0f, 0.0f, 6356752.314245f},
  {90, 270, 0.0f, 0.0f, 6356752.314245f},
  {90, 315, 0.0f, 0.0f, 6356752.314245f},
  {-45, 0, 4517590.878837506f, 0.0f, -4487348.408877217f},
  {-45, 45, 3194419.1450443445f, 3194419.1450606463f, -4487348.408877217f},
  {-45, 90, 0.0f, 4517590.878837506f, -4487348.408877217f},
  {-45, 135, -3194419.145076949f, 3194419.145028042f, -4487348.408877217f},
  {-45, 180, -4517590.878837506f, 0.0f, -4487348.408877217f},
  {-45, -135, -3194419.145076949f, -3194419.145028042f, -4487348.408877217f},
  {-45, -90, 0.0f, -4517590.878837506f, -4487348.408877217f},
  {-45, -45, 3194419.1450443445f, -3194419.1450606463f, -4487348.408877217f},
  {-90, 0, 0.0f, 0.0f, -6356752.314245f},
};
// clang-format on

// Subsolar coodinates are to the nearest minute.
// clang-format off
static const SubsolarTestCase gSubsolarCases[] = {
  // 2024 Jan. 18 at 14:40:00 UTC
  {1705588800, -20.5666666667, -37.4166666667},
  // 1979 Apr. 22 at 09:00:00 UTC
  {293619600, 12.05, 44.6666666667},
  // 2019 Oct. 10 at 19:00:00 UTC
  {1570734000, -6.75, -108.25},
};
// clang-format on

static bool floatsEqual(double a, double b, double eps);

static bool testDaylightSpan(void);

static bool testIsNight(void);

static bool testLatLonToECEF(void);

static bool testSubsolarPoint(void);

static const TestFn gTests[] = {testDaylightSpan, testIsNight, testLatLonToECEF, testSubsolarPoint};

int main() {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gTests); ++i) {
    // Don't short circuit by placing `ok &&` at the beginning, run the test
    // even if previous tests failed.
    ok = gTests[i]() && ok;
  }

  return (ok ? 0 : -1);
}

static bool floatsEqual(double a, double b, double eps) { return fabs(a - b) < eps; }

static bool testDaylightSpan(void) {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gSpanTestCases); ++i) {
    const SpanTestCase *testCase = &gSpanTestCases[i];

    for (int j = 0; j < COUNTOF(gSpanOrder); ++j) {
      time_t s = 0, e = 0;

      if (!geo_calcDaylightSpan(testCase->lat, testCase->lon, gSpanOrder[j], testCase->y,
                                testCase->m, testCase->d, &s, &e)) {
        fprintf(stderr, "geo_calcDaylightSpan() failed.");
        ok = false;
      }

      if (ok && s != testCase->exp[j].start) {
        fprintf(stderr, "Daylight test case %d, span %d, %lu != %lu\n", i, j, s,
                testCase->exp[j].start);
        ok = false;
      }

      if (ok && e != testCase->exp[j].end) {
        fprintf(stderr, "Daylight test case %d, span %d, %lu != %lu\n", i, j, e,
                testCase->exp[j].end);
        ok = false;
      }
    }
  }

  return ok;
}

static bool testIsNight(void) {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gNightTestCases); ++i) {
    const NightTestCase *testCase = &gNightTestCases[i];

    for (int j = 0; j < COUNTOF(gSpanOrder); ++j) {
      bool isNight = geo_isNight(testCase->lat, testCase->lon, gSpanOrder[j], testCase->obsTime);

      if (isNight != testCase->exp[j]) {
        fprintf(stderr, "Night test case %d, span %d, %d != %d\n", i, j, isNight, testCase->exp[j]);
        ok = false;
      }
    }
  }

  return ok;
}

static bool testLatLonToECEF(void) {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gECEFCases); ++i) {
    const LatLonToECEFCase *testCase = &gECEFCases[i];
    float                   x, y, z;

    geo_LatLonToECEF(testCase->lat, testCase->lon, &x, &y, &z);

    if (ok && !floatsEqual(x, testCase->x, ECEF_EPSILON)) {
      fprintf(stderr, "ECEF test case %d, X %f != %f\n", i, x, testCase->x);
      ok = false;
    }

    if (ok && !floatsEqual(y, testCase->y, ECEF_EPSILON)) {
      fprintf(stderr, "ECEF test case %d, Y %f != %f\n", i, y, testCase->y);
      ok = false;
    }

    if (ok && !floatsEqual(z, testCase->z, ECEF_EPSILON)) {
      fprintf(stderr, "ECEF test case %d, Z %f != %f\n", i, z, testCase->z);
      ok = false;
    }
  }

  return ok;
}

bool testSubsolarPoint(void) {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gSubsolarCases); ++i) {
    const SubsolarTestCase *testCase = &gSubsolarCases[i];
    double                  lat, lon;

    geo_calcSubsolarPoint(testCase->obsTime, &lat, &lon);

    if (ok && !floatsEqual(lat, testCase->lat, SUBSOLAR_EPSILON)) {
      fprintf(stderr, "Subsolar test case %d, Lat %f != %f\n", i, lat, testCase->lat);
      ok = false;
    }

    if (ok && !floatsEqual(lon, testCase->lon, SUBSOLAR_EPSILON)) {
      fprintf(stderr, "Subsolar test case %d, Lon %f != %f\n", i, lon, testCase->lon);
      ok = false;
    }
  }

  return ok;
}
