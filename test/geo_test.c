#include "geo.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef struct {
  int y;
  int m;
  int d;
  double lat;
  double lon;
} DateLocation;

static const DateLocation testCases[] = {
  {2023, 7, 15, 45, -123},
  {2019, 8, 11, 50.88232013304212, 4.365450383031231},
  {2016, 10, 8, 35.768812179426085, 139.78138877967353},
  {1994, 8, 1, 39.77249602297143, -105.00703387541155},
  {1979, 4, 22, 27.966092957654435, -82.80653406522597},
};

int main() {
  for (int i = 0; i < COUNTOF(testCases); ++i) {
    const DateLocation *testCase = &testCases[i];
    time_t sr, ss;
    struct tm tmp;

    printf(">>> (%.2f, %.2f) on %4.4d-%2.2d-%2.2d\n", testCase->lat, testCase->lon, testCase->y,
           testCase->m, testCase->d);

    if (!calcSunTransitTimes(testCase->lat, testCase->lon, testCase->y, testCase->m, testCase->d,
                             &sr, &ss)) {
      printf("  Failed to calculate.\n");
    }

    gmtime_r(&sr, &tmp);
    printf("  %2.2d:%2.2d:%2.2d\n", tmp.tm_hour, tmp.tm_min, tmp.tm_sec);

    gmtime_r(&ss, &tmp);
    printf("  %2.2d:%2.2d:%2.2d\n", tmp.tm_hour, tmp.tm_min, tmp.tm_sec);
  }

  return 0;
}