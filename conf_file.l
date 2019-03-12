%option prefix="conf_"
%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge

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
  yylval->param = confStations;
  return TOKEN_PARAM;
}

"cycletime" {
  yylval->param = confCycleTime;
  return TOKEN_PARAM;
}

"=" { return '='; }

";" { return ';'; }

{COMMENT} { }

{WS} { }

%%
