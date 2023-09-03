%option prefix="wxtype_"
%option noyywrap
%option never-interactive
%option reentrant

%{

#include "wx_type.h"
#include <string.h>

%}

%%

" "  { return ' '; }
"+"  { return '+'; }
"-"  { return '-'; }
"VC" { return wxVC; }

"MI" { return wxMI; }
"PR" { return wxPR; }
"BC" { return wxBC; }
"DR" { return wxDR; }
"BL" { return wxBL; }
"SH" { return wxSH; }
"TS" { return wxTS; }
"FZ" { return wxFZ; }

"DZ" { return wxDZ; }
"RA" { return wxRA; }
"SN" { return wxSN; }
"SG" { return wxSG; }
"IC" { return wxIC; }
"PL" { return wxPL; }
"GR" { return wxGR; }
"GS" { return wxGS; }
"UP" { return wxUP; }

"BR" { return wxBR; }
"FG" { return wxFG; }
"FU" { return wxFU; }
"VA" { return wxVA; }
"DU" { return wxDU; }
"SA" { return wxSA; }
"HZ" { return wxHZ; }
"PY" { return wxPY; }
"PO" { return wxPO; }
"SQ" { return wxSQ; }
"FC" { return wxFC; }
"SS" { return wxSS; }
"DS" { return wxDS; }

%%
