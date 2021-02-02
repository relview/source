
/******************************************************************************/
/* MULTIOPWINDOW                                                              */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : Dialog window for monadic and dyadic relation operations        */
/*  CREATED : AUG-2000 Werner Lehmann (WL)                                    */
/******************************************************************************/

#include <string.h>
#include <gtk/gtk.h>

#include "Relview.h"
#include "prefs.h"
#include "utilities.h"
#include "multiopwindow_intf.h"
#include "multiopwindow_gtk.h"

/******************************************************************************/
/*       NAME : get_current_op                                                */
/*    PURPOSE : Determines the currently active toggle button.                */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

int get_current_op (MultiOpInfo *info)
{
  int i;

  for (i = 0; i < info->opcount; i++)
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info->opinfos[i].toggle)))
      return i;

  return -1;
}

/******************************************************************************/
/*       NAME : set_current_op                                                */
/*    PURPOSE : Activates the desired togglebutton, enables/disables needed/  */
/*              unneeded entries.                                             */
/* PARAMETERS : operation number (0 <= op < opcount)                          */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void set_current_op (MultiOpInfo *info, int op)
{
  int i = 0;

  for (i = 0; i < info->opcount; i++)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->opinfos[i].toggle), (i == op));

  if (info->entryParam2MO) {
    gtk_widget_set_sensitive (info->entryParam2MO, (info->opinfos[op].paramcount > 1));
    gtk_widget_set_sensitive (info->labelParam2MO, (info->opinfos[op].paramcount > 1));
  }
}

/******************************************************************************/
/*       NAME : on_buttonApplyMO_clicked                                      */
/*    PURPOSE : Callback; Apply button has been clicked. Execute desired      */
/*              operation.                                                    */
/* PARAMETERS : standard GTK+                                                 */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void on_buttonApplyMO_clicked (GtkButton *button, gpointer user_data)
{
  gchar *name, *param1, *param2 = NULL;
  int op;
  GdkCursor *cursor;
  MultiOpInfo *info = (MultiOpInfo *) user_data;

  g_assert (info != NULL);

  /* change mouse cursor to indicate being busy */
  cursor = gdk_cursor_new (GDK_WATCH);
  gdk_window_set_cursor (info->window->window, cursor);
  gdk_cursor_destroy (cursor);

  /* result */
  name = gtk_editable_get_chars (GTK_EDITABLE (info->entryResultMO), 0, -1);
  if (strlen (name) == 0)
    name = g_strdup ("$");

  /* params */
  op = get_current_op (info);
  g_assert ((info->opinfos[op].paramcount == 1) || info->entryParam2MO);

  param1 = gtk_editable_get_chars (GTK_EDITABLE (info->entryParam1MO), 0, -1);
  if (info->entryParam2MO)
    param2 = gtk_editable_get_chars (GTK_EDITABLE (info->entryParam2MO), 0, -1);

  /* computation */
  if (!info->opinfos[op].process_op) /* proccesing proc undefined? => use std */
    std_proc (name, param1, param2, &info->opinfos[op]);
  else
    info->opinfos[op].process_op (name, param1, param2, &info->opinfos[op]);

  /* clean up */
  g_free (name);
  g_free (param1);
  g_free (param2);

  /* reset mouse cursor */
  gdk_window_set_cursor (info->window->window, NULL);
}

/******************************************************************************/
/*       NAME : on_buttonCloseMO_pressed                                      */
/*    PURPOSE : Callback: the close button has been clicked => dismiss window */
/*    CREATED : 11-AUG-2000 WL                                                */
/******************************************************************************/

void on_buttonCloseMO_pressed (GtkButton *button, gpointer user_data)
{
  hide_multiopwindow (user_data);
}

/******************************************************************************/
/*       NAME : find_togglebutton                                             */
/*    PURPOSE : Find togglebutton in info struct. Returns idx or -1 if not    */
/*              found.                                                        */
/* PARAMETERS : The info struct and the toggle to be searched for.            */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

int find_togglebutton (MultiOpInfo *info, GtkToggleButton *togglebutton)
{
  int i;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (togglebutton != NULL, 0);

  for (i = 0; i < info->opcount; i++)
    if (GTK_TOGGLE_BUTTON (info->opinfos[i].toggle) == togglebutton)
      return i;

  return -1;
}

/******************************************************************************/
/*       NAME : button_toggled                                                */
/*    PURPOSE : Mimic radio button behaviour with toggle buttons              */
/* PARAMETERS : standard GTK+                                                 */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void button_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  int idx, is_active;
  MultiOpInfo *info = (MultiOpInfo *) user_data;

  g_assert (info != NULL);
  idx = find_togglebutton (info, togglebutton);
  g_assert (idx >= 0);
  is_active = gtk_toggle_button_get_active (togglebutton);

  if (is_active || (0 > get_current_op (info)))
    set_current_op (info, idx);
}

/******************************************************************************/
/*       NAME : multiop_window_init                                           */
/*    PURPOSE : create multiopwindow, initialize local module variables       */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void multiop_window_init (MultiOpInfo *info)
{
	/* TODO: The "multiop_window" is some sort of template window and
	 * can be instantiated for multiple purposes. Thus, different names
	 * have to be used to workspace_add_window instead of just one name. */

	if (!info->window) {
		Workspace * workspace = rv_get_workspace(rv_get_instance());

		create_windowMultiOp (info);

		// Will fail. See above.

		/* Add the window to the workspace. Also restores position and
		 * visibility. */
		workspace_add_window (workspace, GTK_WINDOW(info->window), "multiop-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(info->window)))
			gtk_widget_show (info->window);
	}
}

/******************************************************************************/
/*       NAME : multiop_window_destroy                                        */
/*    PURPOSE : frees all allocated resources                                 */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void multiop_window_destroy (MultiOpInfo *info)
{
  /* destroy window */
  if (info->window) {
    gtk_object_destroy (info->window);
    info->window = NULL;
  }
}

/******************************************************************************/
/*       NAME : show_multiopwindow                                            */
/*    PURPOSE : create multiopwindow if necessary and show it                 */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void show_multiopwindow (MultiOpInfo *info)
{


  //prefs_restore_window_info (GTK_WINDOW (info->window));
  //gtk_widget_show (info->window);

  /* bring window to top and set the cursor in the first param entry */
  //gdk_window_show (info->window->window);
  //gtk_widget_grab_focus (info->entryParam1MO);
}

/******************************************************************************/
/*       NAME : hide_multiopwindow                                            */
/*    PURPOSE : hide multiopwindow                                            */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

void hide_multiopwindow (MultiOpInfo *info)
{
  if (info->window) {
    gtk_widget_hide (info->window);
  }
}

/******************************************************************************/
/*       NAME : multiopwindow_isVisible                                       */
/*    PURPOSE : TRUE if window is created & visible/shown                     */
/*    CREATED : 09-AUG-2000 WL                                                */
/******************************************************************************/

int multiopwindow_isVisible (MultiOpInfo *info)
{
  if (!info->window)
    return FALSE;

  return GTK_WIDGET_VISIBLE (GTK_WIDGET (info->window));
}
