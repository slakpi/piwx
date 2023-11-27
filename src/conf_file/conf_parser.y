%{

#include <sys/types.h>

typedef void* yyscan_t;

#include "conf_file.h"
#include "conf_file_prv.h"
#include "conf_param.h"

#include "conf.parser.h"
#include "conf.lexer.h"

static void conf_error(yyscan_t _scanner, PiwxConfig *cfg, char *error) {

}

%}

%union{
  struct {
    ConfParam param;
    int n;
  } p;
  char *str;
  int val;
}

%define api.pure
%define api.prefix {conf_}
%parse-param {yyscan_t scanner}
%lex-param {yyscan_t scanner}
%parse-param {PiwxConfig *cfg}

%token<p> TOKEN_PARAM
%token<str> TOKEN_STRING
%token<val> TOKEN_VALUE TOKEN_ONOFF TOKEN_LOGLEVEL TOKEN_DAYLIGHT_SPAN
%start confFile

%%

confFile
: assignment
| confFile assignment
;

assignment
: TOKEN_PARAM '=' TOKEN_STRING ';' {
  switch($1.param) {
  case confStations:
    cfg->stationQuery = $3;
    break;
  case confLED:
    if ($1.n < 1 || $1.n > MAX_LEDS) {
      break;
    }

    if (cfg->ledAssignments[$1.n - 1]) {
      free(cfg->ledAssignments[$1.n - 1]);
    }

    cfg->ledAssignments[$1.n - 1] = $3;

    break;
  default:
    YYERROR;
  }
  }
| TOKEN_PARAM '=' TOKEN_VALUE ';' {
  switch($1.param) {
  case confCycleTime:
    cfg->cycleTime = max($3, 1);
    break;
  case confHighWindSpeed:
    cfg->highWindSpeed = max($3, 1);
    break;
  case confHighWindBlink:
    cfg->highWindBlink = ($3 != 0);
    break;
  case confLEDBrightness:
    cfg->ledBrightness = min(max($3, 0), 255);
    break;
  case confLEDNightBrightness:
    cfg->ledNightBrightness = min(max($3, 0), 255);
    break;
  case confLEDDataPin:
    cfg->ledDataPin = $3;
    break;
  case confLEDDMAChannel:
    cfg->ledDMAChannel = $3;
    break;
  default:
    YYERROR;
  }
  }
| TOKEN_PARAM '=' TOKEN_ONOFF ';' {
  switch ($1.param) {
  case confHighWindSpeed:
    cfg->highWindSpeed = ($3 == 0 ? 0 : DEFAULT_HIGH_WIND_SPEED);
    break;
  case confHighWindBlink:
    cfg->highWindBlink = ($3 == 0 ? false : true);
    break;
  case confLEDBrightness:
    cfg->ledBrightness = ($3 == 0 ? 0 : DEFAULT_LED_BRIGHTNESS);
    break;
  case confLEDNightBrightness:
    cfg->ledNightBrightness = ($3 == 0 ? 0 : DEFAULT_LED_NIGHT_BRIGHTNESS);
    break;
  case confLogLevel:
    cfg->logLevel = ($3 == 0 ? logQuiet : DEFAULT_LOG_LEVEL);
    break;
  default:
    YYERROR;
  }
  }
| TOKEN_PARAM '=' TOKEN_LOGLEVEL ';' {
  switch ($1.param) {
    case confLogLevel:
      switch ($3) {
      case logWarning:
      case logInfo:
      case logDebug:
        cfg->logLevel = $3;
        break;
      default:
        cfg->logLevel = logQuiet;
        break;
      }
      break;
    default:
      YYERROR;
    }
  }
| TOKEN_PARAM '=' TOKEN_DAYLIGHT_SPAN ';' {
  switch ($1.param) {
  case confDaylight:
    switch ($3) {
    case daylightOfficial:
    case daylightCivil:
    case daylightNautical:
    case daylightAstronomical:
      cfg->daylight = $3;
      break;
    default:
      cfg->daylight = daylightCivil;
      break;
    }
    break;
  default:
    YYERROR;
  }
  }
| error ';'
;

%%
