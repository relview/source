/*
 * labelparser.y
 *
 *  Copyright (C) 2011 Stefan Bolus, University of Kiel, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

%name-prefix "label_yy"
%debug

%code top {
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h> /* log10 */
#include <string.h> /* strcpy */
#include "label.h"
 }

%code requires {
#include "label.h"

  typedef struct _Assignment
  {
    int number;
    gchar * name;
  } Assignment;
  
  static Assignment * assignment_new (int number, gchar * name) {
    Assignment * self = g_new (Assignment,1);
    self->number = number;
    self->name = name;
    return self;
  }

  /* Input file stream for the lexer. */
  extern FILE * label_yyin;
 }

%union {
  Label * label;
  Assignment * p;
  GList * pairs;
  gchar * s;
}
 
%{

extern void   set_lex_input (const char *);
extern void   label_yyflush ();
extern int    label_yylex();


/* ll_root ist der Anfang der Liste */
/* ll ist das aktuelle Element der List */
/* in welches aktuelle Labels eingefuegt werden */
void          label_yyerror  (char * s);

 static GList/*<Label*>*/ * _labels;

%}

%type <label> label
%type <p> pair
%type <pairs> label_body
%type <s> STRING

%nonassoc STRING

%start start

%%

start: | contents

contents: /* nothing */
| contents label { 
  /* The list is reversed afterwards. */
  _labels = g_list_prepend (_labels, $2);
 }

label: STRING '=' '{' error '}' { g_free($1); label_yyerror("Error while parsing label."); }
| STRING '=' '{' label_body '}' { 
  GList * l = g_list_reverse ($4);
  GList * iter = l;
  $$ = (Label*)label_list_new ($1);
  g_free($1);
  for ( ; iter ; iter = iter->next) {
    Assignment * a = (Assignment*) (iter->data);
    label_list_add ((LabelList*)$$, a->number, a->name);
    g_free (a->name);
    g_free (a);
  }
  g_list_free (l);
 }

label_body: 
label_body ',' pair { 
  $$ = g_list_prepend ($1, (gpointer)$3); }
| pair { 
$$ = g_list_prepend(NULL, (gpointer)$1); }

pair: STRING STRING { 
  $$ = assignment_new (atoi($<s>1), $<s>2);
  g_free($1);
}

%%

void label_yyerror (char * s)
{
  fprintf (stderr, "Parse-Error %s\n", s);
  label_yyflush();
}

/* Used in file_ops.c */
GList/*<Label*>*/ * parse_label_file (const gchar * filename, GError ** perr)
{
  label_yyin = fopen (filename, "r");
  if ( !label_yyin) {
    g_set_error (perr, 0,0, "Unable to open file \"%s\".", filename);
    return NULL;
  } else {
    gboolean failed;

    _labels = NULL;

    failed = label_yyparse();
    fclose (label_yyin);
    if (failed) /* error */ {
      g_set_error (perr, rv_error_domain(), 0, "Unable to parse file \"%s\".", filename);
      if (NULL != _labels) {
	g_list_foreach (_labels, (GFunc) label_destroy, NULL);
	g_list_free (_labels);
      }
      return NULL;
    }
    else {
      _labels = g_list_reverse (_labels);
      return _labels;
    }
  }
}

