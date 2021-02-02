#include <gtk/gtk.h>
#include "Relview.h"
#include "Relation.h"
#include "utilities.h"
#include "testwindow_gtk.h"

void test_window_init ()
{
	if (!windowTests) {
		Workspace * workspace = rv_get_workspace(rv_get_instance());

		create_windowTests ();

		/* Add the window to the workspace. Also restores position and visibility. */
		workspace_add_window (workspace, GTK_WINDOW(windowTests), "test-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(windowTests)))
			show_testwindow ();
	}
}

void test_window_destroy ()
{

}

/******************************************************************************/
/*       NAME : show_testwindow                                               */
/*    PURPOSE : Create if necessary and show the window                       */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void show_testwindow ()
{

}

/******************************************************************************/
/*       NAME : hide_testwindow                                               */
/*    PURPOSE : Dismiss window. The window is not destroyed.                  */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void hide_testwindow ()
{

}

/******************************************************************************/
/*       NAME : testwindow_isVisible                                          */
/*    PURPOSE : Returns TRUE if window is created and visible.                */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

int testwindow_isVisible (void)
{
	return FALSE;
}

void on_buttonTestT_clicked (GtkButton *button, gpointer user_data)
{

}
void on_buttonCloseT_clicked (GtkButton *button, gpointer user_data)
{

}
void on_radiobutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{

}

#if 0

/******************************************************************************/
/* TESTWINDOW                                                                 */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : Dialog window; testing one or two relations on certain          */
/*            properties.                                                     */
/*  CREATED : 1995 Peter Schneider (PS)                                       */
/* MODIFIED : 20-JUL-2000 Werner Lehmann (WL)                                 */
/*                        - GTK+ port                                         */
/*                        - moved all relevant parts from xv_x.c into this    */
/*                          module)                                           */
/*                        - completely new dialog design                      */
/*                        - easy extension of the property list is now        */
/*                          possible                                          */
/*                        - result pixmaps come in two flavours: see #define  */
/*                          in testwindow_intf.c                              */
/******************************************************************************/

#include <gtk/gtk.h>

#include "global.h"
#include "relation.h"
#include "all_in_one.bison.tab.h"
#include "tree.h"
#include "prefs.h"
#include "utilities.h"
#include "testwindow_gtk.h"
#include "testwindow_intf.h"

/* static constants (result strings) */

enum MONAD_PROPERTIES {
  mpEmpty, mpEind, mpTotal, mpInjec, mpSurj, mpSymm, mpAntisymm, mpReflexiv,
  mpTransitiv, mpErrorMonad};

static char *res_monad[] =
  {"Empty (empty(R))", "Unival (unival(R))", "Total", "Injective",
   "Surjective", "Symmetric", "Antisymmetric", "Reflexive", "Transitive",
   "Error"};

enum DYAD_PROPERTIES {
  dpEqual, dpInclusion, dpErrorDyad};

static char *res_dyad[] =
  {"Equal (eq(R,S))", "Included (incl(R,S))", "Error"};

/* forward declarations */

static void test_on_one_relation (char *term);
static void test_on_two_relation (char *term1, char *term2);

/* external vars */

extern Rel *rel; /* from main.c */

/* local vars */

static char *tmprel1 = "tmptmptmp1"; /* This has been "tmp1" which might have */
static char *tmprel2 = "tmptmptmp2"; /* been in conflict with existing rels. */

/******************************************************************************/
/*       NAME : on_buttonTestT_clicked                                        */
/*    PURPOSE : Callback: the test button has been clicked => test relations  */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void on_buttonTestT_clicked (GtkButton *button, gpointer user_data)
{
  GString *term1, *term2;
  gchar *tmp;

  /* get entry values */

  tmp = gtk_editable_get_chars (GTK_EDITABLE (entryRel1T), 0, -1);
  if (strlen (tmp) == 0)
    tmp = g_strdup ("$");
  term1 = g_string_new (NULL);
  g_string_sprintf (term1, "%s=%s", tmprel1, tmp);
  g_free (tmp);

  tmp = gtk_editable_get_chars (GTK_EDITABLE (entryRel2T), 0, -1);
  if (strlen (tmp) == 0)
    tmp = g_strdup ("$");
  term2 = g_string_new (NULL);
  g_string_sprintf (term2, "%s=%s", tmprel2, tmp);
  g_free (tmp);

  /* clear list and do the tests */

  gtk_clist_clear (GTK_CLIST (clistResultsT));
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radiobutton1DT)))
    test_on_one_relation (term1->str);
  else
    test_on_two_relation (term1->str, term2->str);

  /* clean up */

  g_string_free (term1, TRUE);
  g_string_free (term2, TRUE);
}

/******************************************************************************/
/*       NAME : on_buttonCloseT_clicked                                       */
/*    PURPOSE : Callback: the close button has been clicked => dismiss window */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void on_buttonCloseT_clicked (GtkButton *button, gpointer user_data)
{
  hide_testwindow ();
}

/******************************************************************************/
/*       NAME : update_entry_sensitivity                                      */
/*    PURPOSE : Enable/Disable second relation entry                          */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void update_entry_sensitivity ()
{
  int is_2d = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radiobutton2DT ));

  gtk_widget_set_sensitive (entryRel2T, is_2d);
  gtk_widget_set_sensitive (labelRel2T, is_2d);
}

/******************************************************************************/
/*       NAME : on_radiobutton_toggled                                        */
/*    PURPOSE : Callback: a radiobutton has been toggled => update sens.      */
/* PARAMETERS : standard GTK+                                                 */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void on_radiobutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  update_entry_sensitivity ();
}

/******************************************************************************/
/*       NAME : test_window_init                                              */
/*    PURPOSE : Initialize the testwindow (includes creation of widgets)      */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void test_window_init ()
{
	if (!windowTests)
		create_windowTests ();

  if (prefs_visible_by_name ("windowTests"))
    show_testwindow ();
}

/******************************************************************************/
/*       NAME : test_window_destroy                                           */
/*    PURPOSE : frees all allocated resources                                 */
/*    CREATED : 21-JUL-2000 WL                                                */
/******************************************************************************/

void test_window_destroy ()
{
  /* destroy window */
  if (windowTests) {
    gtk_object_destroy (win_unregister_window (windowTests));
    windowTests = NULL;
    gdk_pixmap_unref (res_checked);
    gdk_pixmap_unref (res_clear);
  }
}

/******************************************************************************/
/*       NAME : show_testwindow                                               */
/*    PURPOSE : Create if necessary and show the window                       */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void show_testwindow ()
{
  prefs_restore_window_info (GTK_WINDOW (windowTests));
  gtk_widget_show (windowTests);

  /* bring window to top */
  gdk_window_show (windowTests->window);
  update_entry_sensitivity ();
  gtk_widget_grab_focus (entryRel1T);
}

/******************************************************************************/
/*       NAME : hide_testwindow                                               */
/*    PURPOSE : Dismiss window. The window is not destroyed.                  */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void hide_testwindow ()
{
  if (windowTests) {
    gtk_widget_hide (win_on_hide (windowTests));
  }
}

/******************************************************************************/
/*       NAME : testwindow_isVisible                                          */
/*    PURPOSE : Returns TRUE if window is created and visible.                */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

int testwindow_isVisible (void)
{
  if (!windowTests)
    return FALSE;

  return GTK_WIDGET_VISIBLE (GTK_WIDGET (windowTests));
}

/******************************************************************************/
/*       NAME : add_to_clist                                                  */
/*    PURPOSE : Adds a row to the result list                                 */
/*    CREATED : 20-JUL-2000 WL                                                */
/******************************************************************************/

void add_to_clist (char *properties[], int property, int checked)
{
  gchar *text[1];
  int row;

  text[0] = properties[property];
  row = gtk_clist_append (GTK_CLIST (clistResultsT), &properties[property]);

  if (!checked)
    gtk_clist_set_pixtext (GTK_CLIST (clistResultsT), row, 0, properties[property], 10,
                           res_clear, NULL);
  else
    gtk_clist_set_pixtext (GTK_CLIST (clistResultsT), row, 0, properties[property], 10,
                           res_checked, NULL);
}

/******************************************************************************/
/*       NAME : test_on_one_relation                                          */
/*    PURPOSE : Tests the entered relation and fills the result list.         */
/*    CREATED : 29-MAR-1995 PS                                                */
/*   MODIFIED : 20-JUL-2000 WL: GTK+ port                                     */
/******************************************************************************/

void test_on_one_relation (char *term)
{
#if 1
	g_warning ("test_on_one_relation: DISABLED.");
#else
  Rel *test_rel;
  nodeptr p = (nodeptr) NULL;
  //int err_flag, typ;
  int typ;
  RvEvalError * error = NULL;

  if ((p = create_tree (term, &typ))) {
    //err_flag = FALSE;
    //eval_tree (p, rel_root, &err_flag);    /* eval with err_flag == FALSE */
    rv_eval_tree (p, &rel_root, dom_root, &error);

    if (! error && (typ == EVALDEF)) {   /* no processing error */
      test_rel = get_rel (rel_root, tmprel1);

      add_to_clist (res_monad, mpEmpty, test_on_empty (test_rel));
      add_to_clist (res_monad, mpEind, test_on_eind (test_rel));
      add_to_clist (res_monad, mpTotal, test_on_total (test_rel));
      add_to_clist (res_monad, mpInjec, test_on_inj (test_rel));
      add_to_clist (res_monad, mpSurj, test_on_surj (test_rel));

      /* from this point on only with square relations */

      if (test_rel->breite == test_rel->hoehe) {
        add_to_clist (res_monad, mpSymm, test_on_symm (test_rel));
        add_to_clist (res_monad, mpAntisymm, test_on_antisymm (test_rel));
        add_to_clist (res_monad, mpReflexiv, test_on_refl (test_rel));
        add_to_clist (res_monad, mpTransitiv, test_on_trans (test_rel));
      }
      rel_root = del_rel (rel_root, tmprel1);
    } else {
      add_to_clist (res_monad, mpErrorMonad, TRUE);
    }
  } else
    add_to_clist (res_monad, mpErrorMonad, TRUE);

  rv_eval_error_destroy(error);
#endif
}

/******************************************************************************/
/*       NAME : test_on_two_relation                                          */
/*    PURPOSE : Tests the entered relations and fills the result list.        */
/*    CREATED : 29-MAR-1995 PS                                                */
/*   MODIFIED : 20-JUL-2000 WL: GTK+ port                                     */
/******************************************************************************/

void test_on_two_relation (char *term1, char *term2)
{
#if 1
	g_warning ("test_on_two_relation: DISABLED.");
#else
  Rel *rel1, *rel2;
  nodeptr p1,p2;
  //int err_flag, typ;
  int typ;
  RvEvalError * error = NULL;

  p1 = p2 = NULL;
  if ((p1 = create_tree (term1, &typ))) {
    //err_flag = FALSE;
    //eval_tree (p1, rel_root, &err_flag);    /* eval with err_flag == FALSE */
    rv_eval_tree (p2, &rel_root, dom_root, &error);
    if (!error && (typ == EVALDEF)) {
      rel1 = get_rel (rel_root, tmprel1);
      if ((p2 = create_tree (term2, &typ))) {
        //err_flag = FALSE;
        //eval_tree (p2, rel_root, &err_flag);  /* eval with err_flag == FALSE */
    	  rv_eval_tree (p2, &rel_root, dom_root, &error);

        if (! error && (typ == EVALDEF)) {
          rel2 = get_rel (rel_root, tmprel2);
          add_to_clist (res_dyad, dpEqual, test_on_equal(rel1,rel2));
          add_to_clist (res_dyad, dpInclusion, test_on_inclusion(rel1,rel2));
          rel_root = del_rel (rel_root, tmprel2);
        } else
          add_to_clist (res_dyad, dpErrorDyad, TRUE);
      } else
        add_to_clist (res_dyad, dpErrorDyad, TRUE);
        rel_root = del_rel (rel_root, tmprel1);
    } else
      add_to_clist (res_dyad, dpErrorDyad, TRUE);
  } else
    add_to_clist (res_dyad, dpErrorDyad, TRUE);

  rv_eval_error_destroy(error);
#endif
}

#endif
