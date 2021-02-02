/*
 * FileWindow.c
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
#include "FileWindow.h"
#include "FileUtils.h"
#include <assert.h>

#define FILE_WINDOW_OBJECT_ATTACH_KEY "FileWindowObj"

struct _FileWindow
{
	Relview * rv;

	GtkWidget * window; /*!< The window. */
	GtkWidget * chooser; /*!< The GtkFileChooserWidget */
	GtkWidget * extensions; /*!< GtkTreeView for the extensions. */
	GtkWidget * file_type_label; /*!< "Select File Type (...)" */
	GtkWidget * add_ext; /*!< Checkbutton "Add Ext..." */

	/*! NULL always points to  */
	GHashTable/*<IOHandler*,GtkFileFilter*>*/ * filter_map;

	RelviewObserver relview_observer;
};

static FileWindow * _file_window_instance = NULL;		/*!< Singleton */


void 	on_buttonOpenFiles_clicked (GtkButton *widget, gpointer user_data);
void 	on_buttonSaveFiles_clicked (GtkButton * widget, gpointer user_data);
void 	on_buttonCloseFiles_clicked (GtkButton *widget, gpointer user_data);

static FileWindow * 	_file_window_ctor (FileWindow * self, GtkWidget * window);
static FileWindow * 	_file_window_new (GtkWidget * window);
static void				_file_window_dtor (FileWindow * self);
static void				_file_window_destroy (FileWindow * self);
static FileWindow *		_file_window_get (GtkWidget * widget);

static void 			_on_io_handlers_changed (gpointer object, Relview * rv);
static GtkFileFilter * 	_create_file_filter_for_io_handler (IOHandler * handler);
static void 			_update_extensions (FileWindow * self);
static void 			_update_filters (FileWindow * self);
static void 			_on_sel_ext_changed (GtkTreeSelection * sel, FileWindow * self);
static IOHandler * 		_get_current_io_handler (FileWindow * self);
static gchar * 			_add_ext_if_necessary (const gchar * filename, IOHandler * handler);
static void				_on_file_activated (GtkFileChooser *chooser, gpointer user_data);
static void				_open_files (FileWindow * self);


FileWindow * file_window_get_instance ()
{
	if (! _file_window_instance) {
		GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
		GtkWidget * window = GTK_WIDGET (gtk_builder_get_object(builder, "windowFiles"));
		_file_window_instance = _file_window_new (window);
	}
	return _file_window_instance;
}


/*!
 * Create a filter for the given \ref IOHandler object. The name has the format
 * "<name> (*.abc, *def, <etc>)". If handler is NULL, NULL is returned.
 */
GtkFileFilter * _create_file_filter_for_io_handler (IOHandler * handler)
{
	if (! handler) return NULL;
	else {
		GtkFileFilter * filter = gtk_file_filter_new ();
		GString * filter_name = g_string_new ("");
		GList/*<gchar*>*/ * exts = io_handler_get_extensions_as_list (handler), *jter;

		g_string_append_printf(filter_name, "%s (", io_handler_get_name(handler));

		for (jter = exts ; jter ; jter = jter->next) {
			gchar * cur_ext = (gchar*) jter->data;
			gchar * pat = g_strdup_printf ("*.%s", cur_ext);

			g_string_append (filter_name, pat);
			gtk_file_filter_add_pattern (filter, pat);

			g_free (cur_ext);
			g_free (pat);

			if (jter->next) g_string_append (filter_name, ", ");
		}
		g_string_append_c (filter_name, ')');
		g_list_free (exts);
		gtk_file_filter_set_name (filter, filter_name->str);
		g_string_free (filter_name, TRUE);

		return filter;
	}
}

/*!
 * For the extensions at the bottom we have to consider both types of handlers.
 * The user might want to load files with non-compliant suffices.
 */
void _update_extensions (FileWindow * self)
{
	GList/*<IOHandler*>*/ * handlers
		= rv_get_io_handler_for_any_action (self->rv, IO_HANDLER_ACTION_SAVE
				| IO_HANDLER_ACTION_LOAD);
	GtkTreeView * treeview = GTK_TREE_VIEW(self->extensions);
	GtkTreeSelection * sel
				= gtk_tree_view_get_selection (treeview);

	/* Selections are not be affected, if the selected row doesn't change. If
	 * there was a selection and the selected row was deleted the
	 * "By Extension" row is selected afterwards. */
	gboolean had_sel = (gtk_tree_selection_count_selected_rows(sel) > 0);

	GtkTreeModel * model = gtk_tree_view_get_model (treeview);
	GtkListStore * liststore = GTK_LIST_STORE (model);
	GList/*<IOHandler*>*/ * handlers_in_list = NULL;

	/* First remove extensions which because unavailable. */
	{
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				IOHandler * handler = NULL;
				gtk_tree_model_get (model, &iter, 2, &handler, -1);
				/* Don't remove the default extension item. */
				if (handler != NULL && ! g_list_find(handlers, handler)) {
					gtk_list_store_remove (liststore, &iter);
				}
				else {
					handlers_in_list
						= g_list_prepend (handlers_in_list, handler);
				}
			} while (gtk_tree_model_iter_next(model, &iter));
		}
	}

	/* Add the default item if necessary. */
	if ( !g_list_find (handlers_in_list, NULL)) {
		GtkTreeIter tree_iter;

		gtk_list_store_prepend (liststore, &tree_iter);
		gtk_list_store_set(liststore, &tree_iter,
				0, "By Extension",
				1, "",
				2, NULL, -1);
	}

	/* Add new extensions to the tree view */
	{
		GList/*<IOHandler*>*/ * iter = handlers;
		GtkTreeIter tree_iter;

		for ( ; iter ; iter = iter->next) {
			if ( !g_list_find (handlers_in_list, iter->data)) {
				IOHandler * handler = iter->data;
				gtk_list_store_append (liststore, &tree_iter);
				gtk_list_store_set (liststore, &tree_iter,
						0, io_handler_get_name(handler),
						1, io_handler_get_extensions(handler),
						2, handler, -1);
			}
		}
	}

	g_list_free (handlers_in_list);
	g_list_free (handlers);

	if (had_sel) {
		GtkTreeIter tree_iter;
		g_assert (gtk_tree_model_get_iter_first (model, &tree_iter));
		gtk_tree_selection_select_iter (sel, &tree_iter);
	}
}


void _update_filters (FileWindow * self)
{
	GtkFileChooser * chooser = GTK_FILE_CHOOSER(self->chooser);
	GtkFileFilter * cur_filter = gtk_file_chooser_get_filter (chooser);
	GList/*<IOHandler*>*/ * handlers
		= rv_get_io_handler_for_any_action (self->rv, IO_HANDLER_ACTION_LOAD
				| IO_HANDLER_ACTION_SAVE);
	int removed_filters = 0, new_filters = 0;

	/* First remove handlers which became unavailable. */
	{
		GHashTableIter iter;
		IOHandler * key = NULL;
		GtkFileFilter * filter = NULL;

		g_hash_table_iter_init (&iter, self->filter_map);
		while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&filter)) {
			/* Handler no longer available: NOTE: O(n^2). But never remove the
			 * default filter (0x0, 0x1). */
			if ( (gpointer)key > (gpointer)0x10 && !g_list_find (handlers, key)) {
				verbose_printf (VERBOSE_DEBUG, __FILE__
						": Removing new file filter \"%s\".\n", gtk_file_filter_get_name (filter));

				gtk_file_chooser_remove_filter (chooser, filter);
				g_hash_table_iter_remove (&iter);
				removed_filters ++;

				/* If the currently selected filter is no longer available,
				 * we choose another one afterwards. */
				if (cur_filter == filter) cur_filter = NULL;
			}
		}
	}

	/* Now add the new filters and update the filter for all known files if
	 * necessary. */
	{
		GList/*<IOHandler*>*/ * iter = NULL;
#define FILTER_KNOWN_KEY 	((gpointer)0x0)
#define FILTER_ALL_KEY		((gpointer)0x1)

		// Add the filter for all files if necessary.
		if (NULL == g_hash_table_lookup (self->filter_map, FILTER_ALL_KEY)) {
			GtkFileFilter * filter = gtk_file_filter_new();
			gtk_file_filter_add_pattern(filter, "*");
			gtk_file_filter_set_name(filter, "All files (*)");
			g_hash_table_insert (self->filter_map, FILTER_ALL_KEY, filter);
			gtk_file_chooser_add_filter (chooser, filter);
		}

		for ( iter = handlers ; iter ; iter = iter->next) {
			if (NULL == g_hash_table_lookup (self->filter_map, iter->data)) {
				/* Add this new filter. */
				IOHandler * handler = (IOHandler*) iter->data;
				GtkFileFilter * filter
					= _create_file_filter_for_io_handler (handler);
				gtk_file_chooser_add_filter (chooser, filter);
				g_hash_table_insert (self->filter_map, handler, filter);

				verbose_printf (VERBOSE_DEBUG, __FILE__
						": Adding new file filter \"%s\".\n", gtk_file_filter_get_name (filter));

				new_filters ++;
			}
		}

		/* Update the filter for all known file types if necessary. */
		if (removed_filters > 0 || new_filters > 0) {
			GtkFileFilter * filter = g_hash_table_lookup (self->filter_map, FILTER_KNOWN_KEY);

			verbose_printf (VERBOSE_DEBUG, __FILE__": Updating file filter for known file types.\n");

			// Add the default filter if necessary.
			if (NULL != filter) {
				gtk_file_chooser_remove_filter (chooser, filter);
				g_hash_table_remove (self->filter_map, FILTER_KNOWN_KEY);
			}

			filter = gtk_file_filter_new();
			gtk_file_filter_set_name (filter, "Known files");

			for ( iter = handlers ; iter ; iter = iter->next) {
				GList/*<gchar*>*/ * exts = io_handler_get_extensions_as_list ((IOHandler*)iter->data), *jter;
				for ( jter = exts ; jter ; jter = jter->next) {
					gchar * pat = g_strdup_printf ("*.%s", (const gchar*)jter->data);
					gtk_file_filter_add_pattern (filter, pat);
					g_free (pat);
				}
				g_list_free (exts);
			}
			gtk_file_chooser_add_filter (chooser, filter);
			g_hash_table_insert (self->filter_map, FILTER_KNOWN_KEY, filter);

		}
	}

	/* Reset the current filter or use the default filter for all known files
	 * if necessary. */
	if (!cur_filter) cur_filter = g_hash_table_lookup (self->filter_map, FILTER_KNOWN_KEY);
	g_assert (cur_filter != NULL);
	gtk_file_chooser_set_filter (chooser, cur_filter);
}


void _on_io_handlers_changed (gpointer object, Relview * rv)
{
	_update_filters ((FileWindow*) object);
	_update_extensions ((FileWindow*) object);
}


void _on_sel_ext_changed (GtkTreeSelection * sel, FileWindow * self)
{
	GtkTreeIter iter = {0};
	GtkTreeModel * model = NULL;

	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		const gchar * ext_name;
		gchar * s;

		gtk_tree_model_get (model,&iter,0,&ext_name,-1);
		s = g_strdup_printf ("Select File Type (%s)", ext_name);

		gtk_label_set_text (GTK_LABEL (self->file_type_label), s);
		g_free (s);
	}
}


/*!
 * Returns NULL, if the user hasn't specified a handler. This means that
 * the file extension is used to determine the file type.
 */
IOHandler * _get_current_io_handler (FileWindow * self)
{
	GtkTreeView * treeview = GTK_TREE_VIEW (self->extensions);
	GtkTreeSelection * sel = gtk_tree_view_get_selection (treeview);
	GtkTreeIter iter = {0};
	GtkTreeModel * model = NULL;

	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		IOHandler * handler = NULL;
		gtk_tree_model_get (model, &iter, 2, &handler, -1);

		// handler may be the default handler, i.e. it may be NULL.
		return handler;
	}
	else return NULL;
}


void _open_files (FileWindow * self)
{
	IOHandler * handler = _get_current_io_handler (self);
	gchar * filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->chooser));

	if(!filename) {
		rv_user_error ("No file selected", "Please choose a file.");
		return;
	}

	if (! handler) /* Use file extension */ {
		handler = rv_get_io_handler_for_file_and_action (self->rv, filename,
				IO_HANDLER_ACTION_LOAD);
	}

	if (handler) {
		gboolean success;
		GError * err = NULL;

		verbose_printf (VERBOSE_DEBUG, "Loading file \"%s\" with handler "
				"\"%s\"\n", filename, io_handler_get_name (handler));

		#warning TODO: Missing Replace-Callback
		success = io_handler_load (handler, filename, NULL, NULL, &err);
		if (! success) {
			rv_user_error_with_descr ("Unable to load file", err->message,
					"The IO handler \"%s\" was incapable to load the file "
					"\"%s\". The following error was returned.",
					io_handler_get_name(handler), filename);
			g_error_free(err);
		}
	}
	else {
		rv_user_error ("Unable to Load File", "Don't know how to handle a "
					   "file with the that extension. If you know it, "
					   "you can choose the type from the list at the bottom.");
	}

	g_free (filename);
}

void on_buttonOpenFiles_clicked (GtkButton *widget, gpointer user_data)
{
	_open_files (_file_window_get(GTK_WIDGET(widget)));
}


/*!
 * The returned string must be freed with g_free. The extension is added, if
 * the given file doesn't have any of the extensions of the given handler.
 * Extensions are compared case-insensitively. If the handler covert more
 * than one extension, any is chosen.
 */
gchar * _add_ext_if_necessary (const gchar * filename, IOHandler * handler)
{
	if (! handler) return g_strdup (filename);
	else  {
		gchar * cur_ext = file_utils_get_extension (filename);
		if (! cur_ext) return g_strdup (filename);
		else {
			GList/*<gchar*>*/ * exts = io_handler_get_extensions_as_list (handler);
			GList * iter;
			gchar * ret = NULL, *tmp = NULL;

			for ( iter = exts ; iter ; iter = iter->next) {
				/* If the extension is found, we have finished. */
				if (g_ascii_strcasecmp (cur_ext, (gchar*) iter->data) == 0)
					return g_strdup (filename);
			}

			tmp = file_utils_strip_extension (filename);
			ret = g_strdup_printf ("%s.%s", tmp, (gchar*)exts->data);
			g_free (tmp);

			g_list_foreach(exts,(GFunc) g_free, NULL);
			g_list_free (exts);

			g_free (cur_ext);
			return ret;
		}
	}
}


void on_buttonSaveFiles_clicked (GtkButton * widget, gpointer user_data)
{
	FileWindow * self = _file_window_get (GTK_WIDGET(widget));
	IOHandler * handler = _get_current_io_handler (self);
	gchar * raw_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->chooser));
	gchar * filename = NULL;

	if(!raw_filename) {
		rv_user_error ("No file name", "Please specify file name.");
		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->add_ext)))
		filename = _add_ext_if_necessary (raw_filename, handler);
	else filename = g_strdup (raw_filename);

	if ( !handler) {
		handler = rv_get_io_handler_for_file_and_action (self->rv, filename,
				IO_HANDLER_ACTION_SAVE);
	}

	if (! handler) {
		rv_user_error ("Unable to Save File", "Don't know how to handle a "
				       "file with the that extension. If you know it, "
					   "you can choose the type from the list at the bottom.");
	}
	else {
		gboolean really_write = TRUE;

		if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
			gint respId;
			GtkWidget * dialog = gtk_message_dialog_new(NULL, 0 /*flags*/,
					GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					"File exists.");
			gtk_dialog_add_buttons(GTK_DIALOG (dialog), "Overwrite", 1, NULL);
			gtk_message_dialog_format_secondary_text(
					GTK_MESSAGE_DIALOG (dialog),
					"File \"%s\" already exists. What shall I do?", filename);

			gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
			respId = gtk_dialog_run(GTK_DIALOG (dialog));
			gtk_widget_destroy(dialog);

			if (respId == GTK_RESPONSE_CANCEL)
				really_write = FALSE;
		}

		if (really_write) {
			gboolean success;
			GError * err = NULL;

			verbose_printf (VERBOSE_DEBUG, "Saving file \"%s\" with handler "
					"\"%s\". Raw filename was \"%s\".\n",
					filename, io_handler_get_name (handler), raw_filename);

			success = io_handler_save (handler, filename, &err);
			if (! success) {
				rv_user_error ("Unable to Save File", "Reason: %s", err->message);
				g_error_free(err);
			}
		}
	}

	g_free (raw_filename);
	g_free (filename);
}


/*!
 * Just hide the file window.
 */
void on_buttonCloseFiles_clicked (GtkButton *widget, gpointer user_data)
{
	FileWindow * self = _file_window_get (GTK_WIDGET(widget));
	file_window_hide (self);
}


FileWindow * _file_window_get (GtkWidget * widget)
{
  while (widget) {
    if (GTK_IS_MENU (widget)) {
      widget = gtk_menu_get_attach_widget (GTK_MENU(widget));
      if (! widget) {
        fprintf (stderr,
                 "The clicked menu doesn't have an attached widget.\n");
        assert (widget != NULL);
      }
    }
    else {
      gpointer * pdata
        = g_object_get_data (G_OBJECT(widget), FILE_WINDOW_OBJECT_ATTACH_KEY);

      if (pdata) {
        return (FileWindow*) pdata;
      }

      widget = gtk_widget_get_parent (widget);
    }
  }

  assert (FALSE);
  return NULL;
}


/*!
 * Double-click or enter in the file chooser.
 */
void _on_file_activated (GtkFileChooser *chooser, gpointer user_data)
{
	FileWindow * self = (FileWindow*) user_data;
	_open_files(self);
}

FileWindow * _file_window_ctor (FileWindow * self, GtkWidget * window)
{
	GtkBuilder * builder;
	GtkWidget * extra_widget;

	self->rv = rv_get_instance ();
	builder = rv_get_gtk_builder (self->rv);

	/* Attach the window widget to the window. You can call _file_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(window), FILE_WINDOW_OBJECT_ATTACH_KEY,
	                   (gpointer) self);

	self->window = window;
	self->chooser = GTK_WIDGET (gtk_builder_get_object (builder, "filechooserwidgetFiles"));
	extra_widget = GTK_WIDGET (gtk_builder_get_object (builder, "expanderFileExtensionsStatic"));
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(self->chooser), extra_widget);

	/* Allows the user to open a file using a double-click or by pressing
	 * enter. */
	g_signal_connect (G_OBJECT(self->chooser), "file-activated",
			G_CALLBACK(_on_file_activated), self);

	self->extensions = GTK_WIDGET (gtk_builder_get_object (builder, "treeviewFileExtensionsStatic"));
	self->file_type_label = GTK_WIDGET (gtk_builder_get_object (builder, "labelSelectedFileTypeFiles"));
	self->add_ext = GTK_WIDGET (gtk_builder_get_object (builder, "checkbuttonAddExtFiles"));

	// Connect the 'changed' signal of the selection
	{
		GtkTreeSelection * sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->extensions));
		g_signal_connect (G_OBJECT(sel), "changed", G_CALLBACK(_on_sel_ext_changed), self);
	}

	/* Get informed on changes with the IO handlers. */
	self->relview_observer.IOHandlersChanged = _on_io_handlers_changed;
	self->relview_observer.object = self;
	rv_register_observer (self->rv, &self->relview_observer);

	self->filter_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	_update_filters (self);
	_update_extensions (self);

#if 1
	{
		Workspace * workspace = rv_get_workspace(self->rv);

		/* Add the window to the workspace. Also restores position and
		 * visibility. */
		workspace_add_window (workspace, GTK_WINDOW(self->window), "file-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
			file_window_show (self);
	}
#else
	/* Load the set preferences for this window. */
	if (prefs_visible_by_name ("windowFilechooser"))
		file_window_show (self);
#endif

	return self;
}

FileWindow * _file_window_new (GtkWidget * window) { return _file_window_ctor (g_new0 (FileWindow,1), window); }

void _file_window_dtor (FileWindow * self)
{
	rv_unregister_observer (self->rv, &self->relview_observer);
	g_hash_table_destroy (self->filter_map);
}



G_GNUC_UNUSED void _file_window_destroy (FileWindow * self)
{
	_file_window_dtor (self);
	g_free (self);
}

GtkWidget * file_window_get_widget (FileWindow * self) { return self->window; }

void		 file_window_show (FileWindow * self) { gtk_widget_show (self->window); }
void		 file_window_hide (FileWindow * self) { gtk_widget_hide (self->window); }
gboolean	 file_window_is_visible (FileWindow * self) {
#if GTK_MINOR_VERSION >= 18
	return gtk_widget_get_visible (self->window);
#else
	return GTK_WIDGET_VISIBLE(self->window);
#endif

}
