%option prefix="conf_"
%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge
%option case-insensitive

%{

#include "conf_file.h"
#include "conf_param.h"
#include "geo.h"
#include "log.h"
#include <string.h>

#include "conf.parser.h"

#define YYSTYPE CONF_STYPE

%}

INT         [0-9]+
COMMENT     #.*$
WS          [\v\r\n\t ]+

%x STR

%%

"\"" { BEGIN(STR); }

<STR>[^\"]+ { 
  yylval->str = strndup(yytext, yyleng);
  return TOKEN_STRING;
}

<STR>"\"" { BEGIN(INITIAL); }

{INT} {
  yylval->val = atoi(yytext);
  return TOKEN_VALUE;
}

"off" {
  yylval->val = 0;
  return TOKEN_ONOFF;
}

"on" {
  yylval->val = 1;
  return TOKEN_ONOFF;
}

"quiet" {
  yylval->val = logQuiet;
  return TOKEN_LOGLEVEL;
}

"warning" {
  yylval->val = logWarning;
  return TOKEN_LOGLEVEL;
}

"info" {
  yylval->val = logInfo;
  return TOKEN_LOGLEVEL;
}

"debug" {
  yylval->val = logDebug;
  return TOKEN_LOGLEVEL;
}

"official" {
  yylval->val = daylightOfficial;
  return TOKEN_DAYLIGHT_SPAN;
}

"civil" {
  yylval->val = daylightCivil;
  return TOKEN_DAYLIGHT_SPAN;
}

"nautical" {
  yylval->val = daylightNautical;
  return TOKEN_DAYLIGHT_SPAN;
}

"astronomical" {
  yylval->val = daylightAstronomical;
  return TOKEN_DAYLIGHT_SPAN;
}

"stations" {
  yylval->p.param = confStations;
  return TOKEN_PARAM;
}

"cycletime" {
  yylval->p.param = confCycleTime;
  return TOKEN_PARAM;
}

"highwindspeed" {
  yylval->p.param = confHighWindSpeed;
  return TOKEN_PARAM;
}

"highwindblink" {
  yylval->p.param = confHighWindBlink;
  return TOKEN_PARAM;
}

"led"{INT} {
  yylval->p.param = confLED;
  yylval->p.n = atoi(yytext + 3);
  return TOKEN_PARAM;  
}

"brightness" {
  yylval->p.param = confLEDBrightness;
  return TOKEN_PARAM;
}

"nightbrightness" {
  yylval->p.param = confLEDNightBrightness;
  return TOKEN_PARAM;
}

"ledpin" {
  yylval->p.param = confLEDDataPin;
  return TOKEN_PARAM;
}

"leddma" {
  yylval->p.param = confLEDDMAChannel;
  return TOKEN_PARAM;
}

"loglevel" {
  yylval->p.param = confLogLevel;
  return TOKEN_PARAM;
}

"daylight" {
  yylval->p.param = confDaylight;
  return TOKEN_PARAM;
}

"drawglobe" {
  yylval->p.param = confDrawGlobe;
  return TOKEN_PARAM;
}

"sort" {
  yylval->p.param = confSortType;
  return TOKEN_PARAM;
}

"alpha" {
  yylval->val = sortAlpha;
  return TOKEN_SORT_TYPE;
}

"position" {
  yylval->val = sortPosition;
  return TOKEN_SORT_TYPE;
}

"query" {
  yylval->val = sortQuery;
  return TOKEN_SORT_TYPE;
}

"=" { return '='; }

";" { return ';'; }

{COMMENT} { }

{WS} { }

%%
