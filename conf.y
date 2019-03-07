%{

typedef void* yyscan_t;

#include "conf.parser.h"
#include "conf.lexer.h"

static void yyerror(yyscan_t _scanner, char *_error)
{

}

%}

%union{
  int dummy;
}

%define api.pure
%parse-param {yyscan_t _scanner}
%lex-param {yyscan_t _scanner}
%start confFile

%%

confFile
:
;

%%
