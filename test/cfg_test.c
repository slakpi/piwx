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

typedef bool (*TestFn)();

static char  gTempFilePath[MAX_PATH];
static FILE *gTempFile;

static void closeTempFile();

static bool compareConf(const PiwxConfig *act, const PiwxConfig *exp);

static bool compareStrings(const char *act, const char *exp);

static void openTempFile();

static bool testNormalParse();

static void writeValidConfFile(FILE *cfgFile, const PiwxConfig *cfg);

static const TestFn gTests[] = {testNormalParse};

int main() {
  bool ok = true;

  memset(gTempFilePath, 0, sizeof(gTempFilePath)); // NOLINT -- Size known.
  gTempFile = NULL;

  for (int i = 0; i < COUNTOF(gTests); ++i) {
    ok = gTests[i]() && ok;
  }

  return (ok ? 0 : -1);
}

static bool testNormalParse() {
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

static void openTempFile() {
  char tmpFile[] = {"/tmp/piwxXXXXXX"};
  int  fd        = mkstemp(tmpFile);

  assert(fd >= 0);

  gTempFile = fdopen(fd, "w");
  assert(gTempFile);

  strncpy_safe(gTempFilePath, tmpFile, COUNTOF(gTempFilePath));
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
  if (!compareStrings(act->stationQuery, exp->stationQuery) || act->cycleTime != exp->cycleTime ||
      act->highWindSpeed != exp->highWindSpeed || act->highWindBlink != exp->highWindBlink ||
      act->ledBrightness != exp->ledBrightness ||
      act->ledNightBrightness != exp->ledNightBrightness || act->ledDataPin != exp->ledDataPin ||
      act->ledDMAChannel != exp->ledDMAChannel || act->logLevel != exp->logLevel ||
      act->daylight != exp->daylight) {
    return false;
  }

  for (int i = 0; i < COUNTOF(act->ledAssignments); ++i) {
    if (!compareStrings(act->ledAssignments[i], exp->ledAssignments[i])) {
      return false;
    }
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

static void closeTempFile() {
  if (!gTempFile) {
    return;
  }

  fclose(gTempFile);
  unlink(gTempFilePath);
  memset(gTempFilePath, 0, sizeof(gTempFilePath)); // NOLINT -- Size known.
  gTempFile = NULL;
}
