%{

#include <sys/types.h>

typedef void* yyscan_t;

#include "../config_helpers.h"
#include "../conf_file.h"
#include "../util.h"
#include "conf_file.parser.h"
#include "conf_file.lexer.h"

static void conf_error(yyscan_t _scanner, PiwxConfig *_cfg, char *_error)
{

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
%parse-param {yyscan_t _scanner}
%lex-param {yyscan_t _scanner}
%parse-param {PiwxConfig *_cfg}

%token<p> TOKEN_PARAM
%token<str> TOKEN_STRING
%token<val> TOKEN_VALUE
%start confFile

%%

confFile
: assignment
| confFile assignment
;

assignment
: TOKEN_PARAM '=' TOKEN_STRING ';' {
    switch($1.param)
    {
    case confStations:
      _cfg->stationQuery = $3;
      break;
    case confNearestAirport:
      _cfg->nearestAirport = $3;
      break;
    case confLED:
      if ($1.n < 1 || $1.n > MAX_LEDS)
        break;
      if (_cfg->ledAssignments[$1.n - 1])
        free(_cfg->ledAssignments[$1.n - 1]);

      _cfg->ledAssignments[$1.n - 1] = $3;

      break;
    default:
      YYERROR;
    }
  }
| TOKEN_PARAM '=' TOKEN_VALUE ';' {
    switch($1.param)
    {
    case confCycleTime:
      _cfg->cycleTime = $3;
      break;
    case confHighWindSpeed:
      _cfg->highWindSpeed = $3;
      break;
    case confLEDBrightness:
      _cfg->ledBrightness = min(max($3, 0), 255);
      break;
    case confLEDNightBrightness:
      _cfg->ledNightBrightness = min(max($3, 0), 255);
      break;
    case confLEDDataPin:
      _cfg->ledDataPin = $3;
      break;
    case confLEDDMAChannel:
      _cfg->ledDMAChannel = $3;
      break;
    default:
      YYERROR;
    }
  }
| error ';'
;

%%
