%option prefix="conf_"
%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge
%option case-insensitive

%{

#include "conf_file.h"
#include "conf_param.h"
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
  yylval->val = LOG_QUIET;
  return TOKEN_LOGLEVEL;
}

"warning" {
  yylval->val = LOG_WARNING;
  return TOKEN_LOGLEVEL;
}

"info" {
  yylval->val = LOG_INFO;
  return TOKEN_LOGLEVEL;
}

"debug" {
  yylval->val = LOG_DEBUG;
  return TOKEN_LOGLEVEL;
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

"=" { return '='; }

";" { return ';'; }

{COMMENT} { }

{WS} { }

%%
