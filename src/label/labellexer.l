/* outfile is needed, becauseo ylwrap (automake) depends on it. */
%option outfile="lex.yy.c"
%option prefix="label_yy"
%option noyywrap

%{
#include <glib.h>
#include "labelparser.h"

%}

%x str

%%

        GString * buf = NULL;

\"       buf = g_string_new (""); BEGIN(str);

<str>\"	{ /* closing quote */
                        label_yylval.s = g_string_free (buf, FALSE);
buf = NULL;
			BEGIN (INITIAL);
			return (STRING);
		}

<str>\n 	{ /* Unterminated string constant. */
			yyterminate();
		 }

<str>\\(.) { /* Escaped character, e.g. "\\n" */
                        g_string_append_c (buf, label_yytext[1]);
         }

<str>[^\\\n\"]+ { g_string_append_len (buf, label_yytext, label_yyleng); }

[a-z0-9A-Z_-]+ { 
label_yylval.s = g_strdup (label_yytext);
return (STRING); 
}

[ \t\n]+ 	{ /* eat up whitespaces */ }

<<EOF>>	{ return 0; }
.		{ return *label_yytext; }

%%

void label_yyflush ()
{
	YY_FLUSH_BUFFER;
}
