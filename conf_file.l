%option prefix="conf_"
%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge
%option case-insensitive

%{

#include <string.h>
#include "../config_helpers.h"
#include "../conf_file.h"
#include "conf_file.parser.h"

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

"stations" {
  yylval->p.param = confStations;
  return TOKEN_PARAM;
}

"nearestairport" {
  yylval->p.param = confNearestAirport;
  return TOKEN_PARAM;
}

"cycletime" {
  yylval->p.param = confCycleTime;
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

"=" { return '='; }

";" { return ';'; }

{COMMENT} { }

{WS} { }

%%
