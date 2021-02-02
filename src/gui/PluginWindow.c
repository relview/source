/*
 * PluginWindow.c
 *
 *  Copyright (C) 2010,2011 Stefan Bolus, University of Kiel, Germany
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

#include "Relview.h"
#include "PluginWindow.h"
#include "pluginImpl.h" /* RvPlugInInfo, Plugin, PluginManager */

#include <gtk/gtk.h>
#include <assert.h>

#define PLUGIN_WINDOW_OBJECT_ATTACH_KEY "PlugInWindowObj"

struct _PlugInWindow
{
	PluginManager * manager;
	GtkWidget * window;

	GtkTreeModel * model;

	PluginManagerObserver pm_observer;
	PluginObserver p_observer;
};


enum {
	PLUGIN_MODEL_COL_OBJECT,
    PLUGIN_MODEL_COL_NAME,
    PLUGIN_MODEL_COL_DESCR,
    PLUGIN_MODEL_COL_ENABLED
};

static void _update_list (PlugInWindow * self)
{
	const GList/*<Plugin*>*/ *plugins = NULL, * iter = NULL;

	gtk_list_store_clear (GTK_LIST_STORE(self->model));
	plugins = plugin_manager_get_plugins (self->manager);

	for ( iter = plugins ; iter ; iter = iter->next ) {
		Plugin * cur = (Plugin*) iter->data;
		GtkTreeIter tree_iter = {0};

		/* Register and observer if not already done. */
		plugin_register_observer (cur, &self->p_observer);

		gtk_list_store_append (GTK_LIST_STORE(self->model), &tree_iter);
		gtk_list_store_set (GTK_LIST_STORE(self->model), &tree_iter,
				PLUGIN_MODEL_COL_OBJECT, (gpointer) cur,
				PLUGIN_MODEL_COL_NAME, plugin_get_name(cur),
				PLUGIN_MODEL_COL_DESCR, plugin_get_descr(cur),
				PLUGIN_MODEL_COL_ENABLED, plugin_is_enabled(cur),
				-1);
	}
}

static void _update_list_elements (PlugInWindow * self)
{
	GtkTreeIter iter = {0};

	if (gtk_tree_model_get_iter_first (self->model, &iter)) {
		do {
			Plugin * plugin = NULL;
			gtk_tree_model_get (self->model, &iter, 0, (gpointer*)&plugin, -1);

			/* Only the enabled/disabled information can change here. */
			gtk_list_store_set (GTK_LIST_STORE(self->model), &iter,
					PLUGIN_MODEL_COL_ENABLED, plugin_is_enabled(plugin),
					-1);

		} while (gtk_tree_model_iter_next (self->model, &iter));
	}
}

static void _on_plugin_manager_changed (gpointer user_data, PluginManager * manager)
{
	PlugInWindow * self = (PlugInWindow*) user_data;
	_update_list (self);
}

static void _on_plugin_changed (gpointer user_data, Plugin * plugin)
{
	PlugInWindow * self = (PlugInWindow*) user_data;
	_update_list_elements (self);
}

static PlugInWindow * plugin_window_new ()
{
	Relview * rv = rv_get_instance ();
	GtkBuilder * builder = rv_get_gtk_builder (rv);
	Workspace * workspace = rv_get_workspace(rv);
	PlugInWindow * self = g_new0 (PlugInWindow,1);
	const GList/*<Plugin*>*/ *plugins = NULL, * iter = NULL;

	self->model = GTK_TREE_MODEL(gtk_builder_get_object(builder, "liststorePlugIns"));

	self->manager = plugin_manager_get_instance ();
	assert (self->manager != NULL);

	plugin_manager_add_dirs(self->manager, rv_get_default_plugin_dirs (rv));

	self->window = GTK_WIDGET (gtk_builder_get_object (builder, "dialogPlugIns"));
	g_object_set_data (G_OBJECT(self->window),
			PLUGIN_WINDOW_OBJECT_ATTACH_KEY, self);

	/* Add the window to the workspace. Also restores position and visibility. */
	workspace_add_window (workspace, GTK_WINDOW(self->window), "plugin-window");

	if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
		plugin_window_show (self);

	/* Observe the plugin manager. */
	self->pm_observer.changed = _on_plugin_manager_changed;
	self->pm_observer.object = self;
	plugin_manager_register_observer (self->manager, &self->pm_observer);

	/* Listen to each plugin individually. */
	self->p_observer.changed = _on_plugin_changed;
	self->p_observer.onDelete = NULL;
	self->p_observer.object = self;
	plugins = plugin_manager_get_plugins (self->manager);
	for (iter = plugins ; iter ; iter = iter->next) {
		plugin_register_observer ((Plugin*)iter->data, &self->p_observer);
	}

	_update_list (self);

	return self;
}

void plugin_window_show (PlugInWindow * self)
{ gtk_widget_show (self->window); }

void plugin_window_hide (PlugInWindow * self)
{ gtk_widget_hide (self->window); }

GtkWidget * plugin_window_get_widget (PlugInWindow * self)
{ return self->window; }

PluginManager * plugin_window_get_manager (PlugInWindow * self)
{ return self->manager; }


/*******************************************************************************
 *                         Plug-In Window - Callbacks                          *
 *                                                                             *
 *                              Thu, 30 Jul 2009                               *
 ******************************************************************************/

/* swapped */
G_MODULE_EXPORT void
on_buttonPlugInWindowRefresh_clicked (GtkWidget * window, GtkButton * button)
{
	PlugInWindow * self
		= g_object_get_data (G_OBJECT(window), PLUGIN_WINDOW_OBJECT_ATTACH_KEY);
	assert (self != NULL);

	plugin_manager_refresh (self->manager);
}

/* swapped */
G_MODULE_EXPORT void
on_buttonPlugInWindowClose_clicked (GtkWidget * window, GtkButton * button)
{
	PlugInWindow * self
		= g_object_get_data (G_OBJECT(window), PLUGIN_WINDOW_OBJECT_ATTACH_KEY);
	assert (self != NULL);

	plugin_window_hide (self);
}

/* swapped */
G_MODULE_EXPORT void
on_buttonPlugInWindowAdd_clicked (GtkWidget * window, GtkButton * button)
{
	GtkWidget * chooser = gtk_file_chooser_dialog_new (
			"Load a plug-in (shared object)", NULL /*parent*/,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gint resp;

	gtk_window_set_position(GTK_WINDOW (chooser), GTK_WIN_POS_CENTER);
	resp = gtk_dialog_run(GTK_DIALOG (chooser));

	if (GTK_RESPONSE_OK == resp) {
		gchar * file =
				gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
		if (file) {
			int ret = FALSE;
			GError * err = NULL;
			PlugInWindow * self
				= g_object_get_data (G_OBJECT(window), PLUGIN_WINDOW_OBJECT_ATTACH_KEY);
			assert (self != NULL);

			ret = plugin_manager_add_file (self->manager, file, &err);
			if (! ret) {
				g_log ("relview", G_LOG_LEVEL_MESSAGE, "Unable to load file "
						"\"%s\". Reason: %s.", file, err->message);
				g_error_free(err);
			}
			g_free(file);
		}
	}

	gtk_widget_destroy(chooser);
}

/* warning. Signals are swapped! */
G_MODULE_EXPORT void
on_cellrendererEnablePlugin_toggled
(GtkTreeModel *model,
        gchar                 *path_str,
        gpointer               user_data)
{
    GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
    GtkTreeIter  iter = {0};

    gtk_tree_model_get_iter (model, &iter, path);

    /* Enable or disable the plugin. */
    {
    	Plugin * plugin = NULL;
    	gtk_tree_model_get (model, &iter, PLUGIN_MODEL_COL_OBJECT, &plugin, -1);
    	if (plugin) {
    		if (plugin_is_enabled(plugin))
    			plugin_disable(plugin);
    		else
    			plugin_enable(plugin);
    	}
        else {
            g_log("relview", G_LOG_LEVEL_WARNING, "Missing Plugin object.\n");
        }
    }

    /* clean up */
    gtk_tree_path_free (path);
}


/*******************************************************************************
 *                         Plug-In Window - Singleton                          *
 *                                                                             *
 *                              Thu, 30 Jul 2009                               *
 ******************************************************************************/

static PlugInWindow * _plugin_window = NULL;

PlugInWindow * plugin_window_get_instance ()
{
	if (! _plugin_window)
		_plugin_window = plugin_window_new ();

	return _plugin_window;
}

