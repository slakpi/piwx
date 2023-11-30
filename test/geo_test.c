#include "geo.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

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

typedef bool (*TestFn)();

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

static bool testDaylightSpan();

static bool testIsNight();

static const TestFn gTests[] = {testDaylightSpan, testIsNight};

int main() {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gTests); ++i) {
    ok = gTests[i]() && ok;
  }

  return (ok ? 0 : -1);
}

static bool testDaylightSpan() {
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

      if (ok) {
        fprintf(stderr, "Daylight test case %d, span %d passed.\n", i, j);
      }
    }
  }

  return ok;
}

static bool testIsNight() {
  bool ok = true;

  for (int i = 0; i < COUNTOF(gNightTestCases); ++i) {
    const NightTestCase *testCase = &gNightTestCases[i];

    for (int j = 0; j < COUNTOF(gSpanOrder); ++j) {
      bool isNight = geo_isNight(testCase->lat, testCase->lon, gSpanOrder[j], testCase->obsTime);

      if (isNight != testCase->exp[j]) {
        fprintf(stderr, "Night test case %d, span %d, %d != %d\n", i, j, isNight, testCase->exp[j]);
        ok = false;
      } else {
        fprintf(stderr, "Night test case %d, span %d passed\n", i, j);
      }
    }
  }

  return ok;
}
