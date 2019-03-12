%{

#include <sys/types.h>

typedef void* yyscan_t;

#include "../config_helpers.h"
#include "../conf_file.h"
#include "conf_file.parser.h"
#include "conf_file.lexer.h"

static void conf_error(yyscan_t _scanner, PiwxConfig *_cfg, char *_error)
{

}

%}

%union{
  ConfParam param;
  char *str;
  int val;
}

%define api.pure
%define api.prefix {conf_}
%parse-param {yyscan_t _scanner}
%lex-param {yyscan_t _scanner}
%parse-param {PiwxConfig *_cfg}

%token<param> TOKEN_PARAM
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
    switch($1)
    {
    case confStations:
      _cfg->stationQuery = $3;
      break;
    default:
      YYERROR;
    }
  }
| TOKEN_PARAM '=' TOKEN_VALUE ';' {
    switch($1)
    {
    case confCycleTime:
      _cfg->cycleTime = $3;
      break;
    default:
      YYERROR;
    }
  }
| error ';'
;

%%
