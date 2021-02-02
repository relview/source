
/******************************************************************************/
/* MULTIOP                                                                    */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : Definitions of several operations needed to create the          */
/*            appropriate dialog window. To add new operations change or add  */
/*            only these variables and use them with the public functions of  */
/*            multiopwindow_gtk.c.                                            */
/*  CREATED : 10-AUG-2000 Werner Lehmann (WL)                                 */
/******************************************************************************/

#include <stdio.h>

#include "multiop.h"
#include "utilities.h"
#include "global.h"
#include "Relation.h"
#include "Domain.h"

/******************************************************************************/
/* BASIC OPERATIONS                                                           */
/******************************************************************************/

static struct OpInfo basicops[] =
   {{2, "|", "(%s)|(%s)", NULL, std_proc},
    {2, "&", "(%s)&(%s)", NULL, std_proc},
    {1, "-", "-(%s)",     NULL, std_proc},
    {2, "*", "(%s)*(%s)", NULL, std_proc},
    {1, "^", "(%s)^",     NULL, std_proc}};

MultiOpInfo basicop_info =
   {"Basic Operations", 5, 40, basicops, NULL, NULL, NULL, NULL, NULL, NULL,
     NULL};

/******************************************************************************/
/* RESIDUALS AND QUOTIENTS                                                    */
/******************************************************************************/

static struct OpInfo resquots[] =
   {{2, "/",   "(%s)/(%s)",      NULL, std_proc},
    {2, "\\",  "(%s)\\(%s)",     NULL, std_proc},
    {2, "syq", "syq((%s),(%s))", NULL, std_proc}};

MultiOpInfo resquot_info =
   {"Residuals and Quotients", 3, 45, resquots, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL};

/******************************************************************************/
/* CLOSURES                                                                   */
/******************************************************************************/

static struct OpInfo closures[] =
   {{1, "trans", "trans((%s))", NULL, std_proc},
    {1, "refl",  "refl((%s))",  NULL, std_proc},
    {1, "symm",  "symm((%s))",  NULL, std_proc}};

MultiOpInfo closure_info =
   {"Closures", 3, 50, closures, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/******************************************************************************/
/* DOMAINS                                                                    */
/******************************************************************************/

static struct OpInfo domains[] =
   {{1, "1-st", "1-st((%s))", NULL, std_proc},
    {1, "2-nd", "2-nd((%s))", NULL, std_proc},
    {1, "ord",  "%s((%s))",   NULL, ord_proc}}; /* sic */

MultiOpInfo domain_info =
   {"Domains", 3, 50, domains, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/******************************************************************************/
/* DIRECT PRODUCT                                                             */
/******************************************************************************/

static struct OpInfo dirprods[] =
   {{1, "p-1", "p-1((%s))",   NULL, std_proc},
    {1, "p-2", "p-2((%s))",   NULL, std_proc},
    {2, "[ ]", "[(%s),(%s)]", NULL, std_proc}};

MultiOpInfo dirprod_info =
   {"Direct Product", 3, 45, dirprods, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL};

/******************************************************************************/
/* DIRECT SUM                                                                 */
/******************************************************************************/

static struct OpInfo dirsums[] =
   {{1, "i-1", "i-1((%s))",  NULL, std_proc},
    {1, "i-2", "i-2((%s))",  NULL, std_proc},
    {2, "+",   "(%s)+(%s)",  NULL, std_proc}};

MultiOpInfo dirsum_info =
   {"Direct Sum", 3, 45, dirsums, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/******************************************************************************/
/* POWERSETS                                                                  */
/******************************************************************************/

static struct OpInfo powersets[] =
   {{1, "epsi",   "epsi((%s))",        NULL, std_proc},
    {2, "part-f", "part-f((%s),(%s))", NULL, std_proc},
    {2, "tot-f",  "tot-f((%s),(%s))",  NULL, std_proc},
    {1, "inj",    "inj((%s))",         NULL, std_proc}};

MultiOpInfo powerset_info =
   {"Powersets", 4, 55, powersets, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/******************************************************************************/
/*       NAME : std_proc                                                      */
/*    PURPOSE : Standard computation for operations. To be used if no extra   */
/*              processing on the params or anything else is needed.          */
/*    CREATED : 11-AUG-2000 WL                                                */
/******************************************************************************/

void std_proc (gchar *name, gchar *param1, gchar *param2, struct OpInfo *opinfo)
{
  GString *term;

  term = g_string_new (NULL);
  g_string_sprintf (term, opinfo->syntax, param1, param2);
  compute_term (term->str, name);

  /* clean up */
  g_string_free (term, TRUE);
}

/******************************************************************************/
/*       NAME : ord_proc                                                      */
/*    PURPOSE : Special processing proc to distinguish between p and s        */
/*              domains.                                                      */
/*    CREATED : 11-AUG-2000 WL                                                */
/******************************************************************************/

void ord_proc (gchar *name, gchar *param1, gchar *param2, struct OpInfo *opinfo)
{
#if 1
	g_warning ("multiop.c: ord_proc: DISABLED.");
#else
  int ord_type;
  Dom *d;
  GString *term;

  g_assert (strcmp (opinfo->name, "ord") == 0);
  d = get_dom (dom_root, param1);
  if (!d) {
    printf ("ERROR: Unknown domain\n");
    return;
  }

  term = g_string_new (NULL);
  ord_type = get_dom_type (d);
  switch (ord_type) {
  case PROD:
    g_string_sprintf (term, opinfo->syntax, "p-ord", param1);
    break;
  case SUM:
    g_string_sprintf (term, opinfo->syntax, "s-ord", param1);
    break;
  default:
    printf("domain-type unknown\n");
    return;
  }
  compute_term (term->str, name);

  /* clean up */
  g_string_free (term, TRUE);
#endif
}
