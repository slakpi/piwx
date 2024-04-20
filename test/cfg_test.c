#include "conf_file.h"
#include "conf_file_prv.h"
#include "geo.h"
#include "log.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK_STRING(a, e) {                                                                       \
  if (!compareStrings((a), (e))) {                                                                 \
    fprintf(stderr, "%s %d -- \"%s\" != \"%s\"\n", __FUNCTION__, __LINE__, a, e);                  \
    return false;                                                                                  \
  }                                                                                                \
}

#define CHECK_SIGNED_INTEGER(a, e) {                                                               \
  if (a != e) {                                                                                    \
    fprintf(stderr, "%s %d -- %d != %d\n", __FUNCTION__, __LINE__, a, e);                          \
    return false;                                                                                  \
  }                                                                                                \
}

#define CHECK_UNSIGNED_INTEGER(a, e) {                                                             \
  if (a != e) {                                                                                    \
    fprintf(stderr, "%s %d -- %u != %u\n", __FUNCTION__, __LINE__, a, e);                          \
    return false;                                                                                  \
  }                                                                                                \
}

typedef bool (*TestFn)(void);

static char  gTempFilePath[MAX_PATH];
static FILE *gTempFile;

static void closeTempFile(void);

static bool compareConf(const PiwxConfig *act, const PiwxConfig *exp);

static bool compareStrings(const char *act, const char *exp);

static void openTempFile(void);

static bool testNormalParse(void);

static void writeValidConfFile(FILE *cfgFile, const PiwxConfig *cfg);

static const TestFn gTests[] = {testNormalParse};

int main() {
  bool ok = true;

  memset(gTempFilePath, 0, sizeof(gTempFilePath)); // NOLINT -- Size known.
  gTempFile = NULL;

  for (int i = 0; i < COUNTOF(gTests); ++i) {
    // Don't short circuit by placing `ok &&` at the beginning, run the test
    // even if previous tests failed.
    ok = gTests[i]() && ok;
  }

  return (ok ? 0 : -1);
}

static bool testNormalParse(void) {
  bool       ok  = true;
  PiwxConfig cfg = {
      .stationQuery       = "KHIO;K7S3;KTPA;KGNV;KDEN;KSEA",
      .cycleTime          = 10,
      .highWindSpeed      = 30,
      .highWindBlink      = 3,
      .ledAssignments     = {"KSEA", "KDEN", "KGNV", "KTPA", "K7S3", "KHIO"},
      .ledBrightness      = 127,
      .ledNightBrightness = 63,
      .ledDataPin         = 12,
      .ledDMAChannel      = 11,
      .logLevel           = logDebug,
      .daylight           = daylightAstronomical,
  };
  PiwxConfig out = {0};

  openTempFile();
  writeValidConfFile(gTempFile, &cfg);

  if (!conf_parseStream(&out, gTempFile)) {
    fprintf(stderr, "Failed to parse normal configuration file.\n");
    ok = false;
  }

  closeTempFile();

  if (ok) {
    if (!compareConf(&out, &cfg)) {
      fprintf(stderr, "Configuration mismatch with normal configuration file.\n");
      ok = false;
    }
  }

  return ok;
}

static void openTempFile(void) {
  char tmpFile[] = {"/tmp/piwxXXXXXX"};
  int  fd        = mkstemp(tmpFile);

  assert(fd >= 0);

  gTempFile = fdopen(fd, "w");
  assert(gTempFile);

  strncpy_safe(gTempFilePath, COUNTOF(gTempFilePath), tmpFile);
}

static void writeValidConfFile(FILE *cfgFile, const PiwxConfig *cfg) {
  if (cfg->stationQuery) {
    fprintf(cfgFile, "stations = \"%s\";\n", cfg->stationQuery);
  }

  fprintf(cfgFile, "cycletime = %d;\n", cfg->cycleTime);
  fprintf(cfgFile, "highwindspeed = %d;\n", cfg->highWindSpeed);
  fprintf(cfgFile, "highwindblink = %d;\n", cfg->highWindBlink);
  fprintf(cfgFile, "brightness = %d;\n", cfg->ledBrightness);
  fprintf(cfgFile, "nightbrightness = %d;\n", cfg->ledNightBrightness);
  fprintf(cfgFile, "ledpin = %d;\n", cfg->ledDataPin);
  fprintf(cfgFile, "leddma = %d;\n", cfg->ledDMAChannel);

  fprintf(cfgFile, "loglevel = ");
  switch (cfg->logLevel) {
  case logQuiet:
    fprintf(cfgFile, "quiet;\n");
    break;
  case logWarning:
    fprintf(cfgFile, "warning;\n");
    break;
  case logInfo:
    fprintf(cfgFile, "info;\n");
    break;
  case logDebug:
    fprintf(cfgFile, "debug;\n");
    break;
  default:
    assert(false);
    break;
  }

  fprintf(cfgFile, "daylight = ");
  switch (cfg->daylight) {
  case daylightOfficial:
    fprintf(cfgFile, "official;\n");
    break;
  case daylightCivil:
    fprintf(cfgFile, "civil;\n");
    break;
  case daylightNautical:
    fprintf(cfgFile, "nautical;\n");
    break;
  case daylightAstronomical:
    fprintf(cfgFile, "astronomical;\n");
    break;
  default:
    assert(false);
    break;
  }

  for (int i = 0; i < COUNTOF(cfg->ledAssignments); ++i) {
    if (cfg->ledAssignments[i]) {
      fprintf(cfgFile, "led%d = \"%s\";\n", i + 1, cfg->ledAssignments[i]);
    }
  }

  fseeko(gTempFile, 0, SEEK_SET);
}

static bool compareConf(const PiwxConfig *act, const PiwxConfig *exp) {
  CHECK_STRING(act->stationQuery, exp->stationQuery);
  CHECK_SIGNED_INTEGER(act->cycleTime, exp->cycleTime);
  CHECK_SIGNED_INTEGER(act->highWindSpeed, exp->highWindSpeed);
  CHECK_SIGNED_INTEGER(!!act->highWindBlink, !!exp->highWindBlink);
  CHECK_SIGNED_INTEGER(act->ledBrightness, exp->ledBrightness);
  CHECK_SIGNED_INTEGER(act->ledNightBrightness, exp->ledNightBrightness);
  CHECK_SIGNED_INTEGER(act->ledDataPin, exp->ledDataPin);
  CHECK_SIGNED_INTEGER(act->ledDMAChannel, exp->ledDMAChannel);
  CHECK_SIGNED_INTEGER(act->logLevel, exp->logLevel);
  CHECK_SIGNED_INTEGER(act->daylight, exp->daylight);

  for (int i = 0; i < COUNTOF(act->ledAssignments); ++i) {
    CHECK_STRING(act->ledAssignments[i], exp->ledAssignments[i]);
  }

  return true;
}

static bool compareStrings(const char *act, const char *exp) {
  if (act != NULL && exp != NULL) {
    if (strcmp(act, exp) != 0) {
      return false;
    }
  } else if (!(act == NULL && exp == NULL)) {
    return false;
  }

  return true;
}

static void closeTempFile(void) {
  if (!gTempFile) {
    return;
  }

  fclose(gTempFile);
  unlink(gTempFilePath);
  memset(gTempFilePath, 0, sizeof(gTempFilePath)); // NOLINT -- Size known.
  gTempFile = NULL;
}
