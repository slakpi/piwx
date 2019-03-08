%{

#include <sys/types.h>

typedef void* yyscan_t;

#include "../config_helpers.h"
#include "../conf.h"
#include "conf.parser.h"
#include "conf.lexer.h"

static void yyerror(yyscan_t _scanner, PiwxConfig *_cfg, char *_error)
{

}

%}

%union{
  ConfParam param;
  char *str;
}

%define api.pure
%parse-param {yyscan_t _scanner}
%lex-param {yyscan_t _scanner}
%parse-param {PiwxConfig *_cfg}

%token<param> TOKEN_PARAM
%token<str> TOKEN_STRING
%start confFile

%%

confFile
: assignment
| confFile assignment
;

assignment
: TOKEN_PARAM '=' TOKEN_STRING ';' {
    switch($1)
    {
    case confStations:
      _cfg->stationQuery = $3;
      break;
    }
  }
| error ';'
;

%%
