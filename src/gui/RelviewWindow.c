/*
 * RelviewWindow.c
 *
 *  Copyright (C) 2000-2010 Werner Lehmann, Stefan Bolus,
 *  University of Kiel, Germany
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

#include "Relation.h"
#include "Graph.h"
#include "msg_boxes.h"
#include "RelationWindow.h"
#include "GraphWindow.h"
#include "LabelWindow.h"
#include "testwindow_gtk.h"
#include "EvalWindow.h"
#include "OptionsDialog.h"
#include "prefs.h"
#include "utilities.h"
#include "rvops_gtk.h"
#include "RelviewWindow.h"
#include "Relview.h"
#include "DirWindow.h"
#include "FileWindow.h"
#include "Function.h"
#include "RelviewTypes.h"
#include "IterWindow.h"

//#include "iterationwindow_intf.h"
//#include "multiopwindow_intf.h"
#include "LabelWindow.h"
#include "PluginWindow.h"

#include <assert.h>
#include <stdio.h>
#include <gtk/gtk.h>

#define OBJECT_ATTACH_KEY "RvWindowObj"


struct _RvWindow
{
	Relview * rv;
	GtkWidget * window;
};

static RvWindow * _window = NULL;


static RvWindow * 		_rv_window_new ();
static void 			_rv_window_destroy (RvWindow * self);


RvWindow * rv_window_get_instance()
{
	if (! _window)
		_window = _rv_window_new ();
	return _window;
}

void rv_window_destroy_instance()
{
	if (_window) {
		_rv_window_destroy (_window);
		_window = NULL;
	}
}


GtkWidget *	rv_window_get_widget(RvWindow * self)
{
	return self->window;
}

/*!
 * Update the toggle button's state if the a window is closed by the user.
 */
static void _on_window_hide (GtkToggleButton * button, GtkWidget * window)
{
	gtk_toggle_button_set_active (button, GTK_WIDGET_VISIBLE(window));
}

RvWindow * _rv_window_new ()
{
	RvWindow * self = g_new0 (RvWindow,1);
	GtkBuilder * builder = NULL;
	Workspace * workspace = NULL;

	self->rv = rv_get_instance ();
	builder = rv_get_gtk_builder (self->rv);

	self->window = GTK_WIDGET(gtk_builder_get_object(builder, "windowRelview"));

	/* Attach the window widget to the window. You can call _*_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(self->window), OBJECT_ATTACH_KEY, (gpointer) self);

	workspace = rv_get_workspace(self->rv);

	/* Add the window to the workspace. Also restores position and
	 * visibility. */
	workspace_add_window (workspace, GTK_WINDOW(self->window), "main-window");

	  /* Install observers for hide/show events on the windows, associated
	   * to the main window toggle-buttons.*/
#define TOGGLE_ON(buttonName,windowObj) \
  { GtkToggleButton * buttonObj \
	  = GTK_TOGGLE_BUTTON (gtk_builder_get_object(builder,buttonName)); \
	  assert (windowObj != NULL); \
	  g_signal_connect_swapped (G_OBJECT(windowObj),\
			"hide", (GCallback)_on_window_hide, buttonObj); \
	  g_signal_connect_swapped (G_OBJECT(windowObj),\
	  			"show", (GCallback)_on_window_hide, \
	  			buttonObj);\
	  gtk_toggle_button_set_active (buttonObj, GTK_WIDGET_VISIBLE(windowObj));\
  }

	 TOGGLE_ON("togglebuttonFiles", file_window_get_widget(file_window_get_instance()));
	 TOGGLE_ON("togglebuttonRelation", relation_window_get_window(relation_window_get_instance()));
	 TOGGLE_ON("togglebuttonGraph", graph_window_get_widget(graph_window_get_instance()));
	 TOGGLE_ON("togglebuttonEval", eval_window_get_widget(eval_window_get_instance()));
	 TOGGLE_ON("togglebuttonIter", iter_window_get_widget(iter_window_get_instance()));
	 //TOGGLE_ON("togglebuttonOps", windowRelviewOps);
	 //TOGGLE_ON("togglebuttonTests", windowTests);
	 TOGGLE_ON("togglebuttonObjs", dir_window_get_widget (dir_window_get_instance()));
	 TOGGLE_ON("togglebuttonLabels", label_window_get_widget(label_window_get_instance()));
	 TOGGLE_ON("togglebuttonPlugIns",
			 plugin_window_get_widget(plugin_window_get_instance()));

	/* The main window is always shown on startup. So visibility can be
	 * ignored here. */
	 gtk_widget_show (self->window);

	return self;
}


/*!
 * Destroys the window.
 *
 * \author stb
 */
void _rv_window_destroy (RvWindow * self)
{
  /* TODO: unref the entry widget which is currently not visible. */
	g_free (self);
}


/* File button */
G_MODULE_EXPORT void buttonFileClicked (GtkButton * button, gpointer user_data)
{
	FileWindow * fw = file_window_get_instance ();
	if (! file_window_is_visible (fw)) file_window_show (fw);
	else file_window_hide (fw);
}

/* Layout button */
G_MODULE_EXPORT void buttonOptionsClicked (GtkButton * button, gpointer user_data)
{
  show_options (rv_get_instance());
}

/* Info button */
G_MODULE_EXPORT void buttonInfoClicked (GtkButton * button, gpointer user_data)
{
  GtkBuilder * builder =  rv_get_gtk_builder(rv_get_instance());
  GtkDialog * about = GTK_DIALOG(gtk_builder_get_object (builder, "aboutdialog"));
  gtk_dialog_run (about);
  gtk_widget_hide (GTK_WIDGET (about));
}

/* Quit button */
G_MODULE_EXPORT void buttonQuitClicked (GtkButton * button, gpointer user_data)
{
  if ((error_and_wait (error_msg [really_quit])) == NOTICE_YES)
    gtk_main_quit ();

}

/* Graph button */
G_MODULE_EXPORT void buttonGraphClicked (GtkButton * button, gpointer user_data)
{
  GraphWindow * gw = graph_window_get_instance ();

  if (graph_window_is_visible (gw))
    graph_window_hide (gw);
  else
    graph_window_show (gw);
}

/* Relation button */
G_MODULE_EXPORT void buttonRelationClicked (GtkButton * button, gpointer user_data)
{
  if (relationwindow_isVisible ())
    hide_relationwindow ();
  else
    show_relationwindow ();
}

/* XRV/Prog (Directory) button */
G_MODULE_EXPORT void buttonXRVPClicked (GtkButton * button, gpointer user_data)
{
	DirWindow * window = dir_window_get_instance ();
	if (! dir_window_is_visible (window)) dir_window_show (window);
	else dir_window_hide (window);
}

/* Label (Directory) button */
G_MODULE_EXPORT void buttonLabClicked (GtkButton * button, gpointer user_data)
{
	LabelWindow * window = label_window_get_instance ();
	if ( !label_window_is_visible(window)) label_window_show(window);
	else label_window_hide (window);
}

/* Defi(ne) button */
void buttonDefClicked (GtkButton * button, gpointer user_data)
{
	Relview * rv = rv_get_instance ();
	gboolean success;
	GString * def = g_string_new ("");

	success = rv_user_edit_function (rv, def);
	if (success) {
		GError * err = NULL;
		Fun * f = fun_new_from_def(def->str, &err);
		if (! f) {
			rv_user_error ("Invalid Function", "The definition \"%s\" is not "
					"a valid function. Reason: %s", err->message);
			g_error_free (err);
		}
		else {
			FunManager * manager = fun_manager_get_instance ();
			gboolean really_insert = TRUE;
			const gchar * name = fun_get_name (f);

			Namespace * ns = rv_get_namespace(rv);
			RvObject * conflict = namespace_get_by_name(ns, name);

			if (conflict) {
				RvReplaceAction action = rv_user_ask_replace_object2
						(rv, conflict, NULL, FALSE);
				if (action != RV_REPLACE_ACTION_REPLACE) {
					fun_destroy (f);
					really_insert = FALSE;
				}
				else rv_object_destroy (conflict);
			}

			if (really_insert)
				fun_manager_insert (manager, f);
		}
	}

	g_string_free(def, TRUE);
}

/* Eval(uate) button */
G_MODULE_EXPORT void buttonEvalClicked (GtkButton * button, gpointer user_data)
{
	EvalWindow * window = eval_window_get_instance ();
	if ( !eval_window_is_visible(window)) eval_window_show(window);
	else eval_window_hide (window);
}

/* Iter(ation) button */
G_MODULE_EXPORT void
buttonIterClicked (GtkButton * button, gpointer user_data)
{
	IterWindow * window = iter_window_get_instance ();
	if (!iter_window_is_visible(window)) iter_window_show (window);
	else iter_window_hide (window);
}

#if 0
/* Test button */
G_MODULE_EXPORT void
buttonTestClicked (GtkButton * button, gpointer user_data)
{
  if (testwindow_isVisible ())
    hide_testwindow ();
  else
    show_testwindow ();
}

/* Ops button */
G_MODULE_EXPORT void
buttonOpsClicked (GtkButton * button, gpointer user_data)
{
  if (rvops_isVisible ())
    hide_rvops ();
  else
    show_rvops ();
}
#endif

G_MODULE_EXPORT void
on_togglebuttonPlugIns_released(GtkButton * button, gpointer user_data)
{
	PlugInWindow * window = plugin_window_get_instance();
	GtkWidget * widget = plugin_window_get_widget(window);

  if (GTK_WIDGET_VISIBLE(widget))
	  plugin_window_hide (window);
  else
	  plugin_window_show (window);
}



