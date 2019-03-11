%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge

%{

#include <string.h>
#include "../config_helpers.h"
#include "../conf.h"
#include "conf.parser.h"

%}

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

"stations" {
  yylval->param = confStations;
  return TOKEN_PARAM;
}

"=" { return '='; }

";" { return ';'; }

{COMMENT} { }

{WS} { }

%%
