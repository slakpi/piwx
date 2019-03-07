%option noyywrap
%option never-interactive
%option reentrant
%option bison-bridge

%{

#include "conf.parser.h"

%}

%%

. { return 0; }

%%
