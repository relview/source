#include <stdio.h>
#include <gtk/gtk.h>

#include "multiopwindow_gtk.h"
#include "Relview.h"
#include "prefs.h"
//#include "rv_gtk.h"
//#include "utilities.h"
//#include "rvops_intf.h"
#include "rvops_gtk.h"

void rvops_init ()
{
	if (!windowRelviewOps) {
		Workspace * workspace = rv_get_workspace(rv_get_instance());

		windowRelviewOps = create_windowRelviewOps ();

		/* Add the window to the workspace. Also restores position and visibility. */
		workspace_add_window (workspace, GTK_WINDOW(windowRelviewOps),
				"ops-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(windowRelviewOps)))
			show_rvops ();
	}
}

/******************************************************************************/
/*       NAME : show_rvops                                                    */
/*    PURPOSE : Create relview operations window if necessary and show it.    */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void show_rvops ()
{
	g_warning ("rvops_init: NOT IMPLEMENTED!");
}

/******************************************************************************/
/*       NAME : hide_rvops                                                    */
/*    PURPOSE : hide window                                                   */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void hide_rvops ()
{
	g_warning ("rvops_init: NOT IMPLEMENTED!");
}

/******************************************************************************/
/*       NAME : destroy_rvops                                                 */
/*    PURPOSE : Hide the relview operations window and destroy it.            */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void destroy_rvops ()
{
	g_warning ("rvops_init: NOT IMPLEMENTED!");
}

/******************************************************************************/
/*       NAME : rvops_isVisible                                               */
/*    PURPOSE : TRUE if windowRelviewOps is created & visible/shown           */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

int rvops_isVisible (void)
{
	g_warning ("rvops_init: NOT IMPLEMENTED!");
	return FALSE;
}

void buttonMultiOpClicked (GtkButton *button, gpointer user_data)
{
	printf ("buttonMultiOpClicked: NOT IMPLEMENTED!\n");
}

void buttonDomDefClicked (GtkButton *button, gpointer user_data)
{
	printf ("buttonDomDefClicked: NOT IMPLEMENTED!\n");
}

gint rvops_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  hide_rvops ();
  return TRUE;
}

#if 0
/******************************************************************************/
/* RVOPS_GTK                                                                  */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : The relview operations window. Hub for all operations dialogs.  */
/*  CREATED : DEC-2000   Werner Lehmann (WL)                                  */
/******************************************************************************/

#include <stdio.h>
#include <gtk/gtk.h>

#include "multiopwindow_gtk.h"
#include "prefs.h"
#include "rv_gtk.h"
#include "utilities.h"
#include "rvops_intf.h"
#include "rvops_gtk.h"

/******************************************************************************/
/*       NAME : button***Clicked                                              */
/*    PURPOSE : Button *** has been clicked ==> open appropriate window       */
/*    CREATED : MAR-2000 WL                                                   */
/******************************************************************************/

/* MultiOp button */

void buttonMultiOpClicked (GtkButton *button, gpointer user_data)
{
  MultiOpInfo *info = gtk_object_get_data (GTK_OBJECT (button), "data");
  int idx = GPOINTER_TO_INT (user_data);

  show_multiopwindow (info);
  set_current_op (info, idx);
}

/* Domain definition button */

void buttonDomDefClicked (GtkButton *button, gpointer user_data)
{
	//show_defdomwindow ();
	// You could use rv_user_edit_domain here.
	g_warning ("buttonDomDefClicked: Disabled!\n");
}

/******************************************************************************/
/*       NAME : rvops_delete_event                                            */
/*    PURPOSE : This is called if the user closes the window via the window   */
/*              decoration button.                                            */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

gint rvops_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  hide_rvops ();
  return TRUE;
}

/******************************************************************************/
/*       NAME : rvops_init                                                    */
/*    PURPOSE : show window if necessary                                      */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void rvops_init ()
{
	if (!windowRelviewOps)
		windowRelviewOps = create_windowRelviewOps ();

  if (prefs_visible_by_name ("windowRelviewOps"))
    show_rvops ();
}

/******************************************************************************/
/*       NAME : show_rvops                                                    */
/*    PURPOSE : Create relview operations window if necessary and show it.    */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void show_rvops ()
{
  prefs_restore_window_info (GTK_WINDOW (windowRelviewOps));
  gtk_widget_show (windowRelviewOps);
}

/******************************************************************************/
/*       NAME : hide_rvops                                                    */
/*    PURPOSE : hide window                                                   */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void hide_rvops ()
{
  if (windowRelviewOps) {
    gtk_widget_hide (win_on_hide (windowRelviewOps));
  }
}

/******************************************************************************/
/*       NAME : destroy_rvops                                                 */
/*    PURPOSE : Hide the relview operations window and destroy it.            */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

void destroy_rvops ()
{
  if (windowRelviewOps) {
    gtk_widget_hide (win_on_hide (windowRelviewOps));
    gtk_object_destroy (win_unregister_window (windowRelviewOps));
    windowRelviewOps = NULL;
  }
}

/******************************************************************************/
/*       NAME : rvops_isVisible                                               */
/*    PURPOSE : TRUE if windowRelviewOps is created & visible/shown           */
/*    CREATED : 19-DEC-2000 WL                                                */
/******************************************************************************/

int rvops_isVisible (void)
{
  if (!windowRelviewOps)
    return FALSE;

  return GTK_WIDGET_VISIBLE (GTK_WIDGET (windowRelviewOps));
}
#endif
