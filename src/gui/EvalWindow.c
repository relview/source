/*
 * EvalWindow.c
 *
 *  Copyright (C) 2008,2009,2010,2011 Stefan Bolus, University of Kiel, Germany
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

#include "EvalWindow.h"
#include "Relview.h"
#include "prefs.h"
#include "Relation.h" /* REL_MAX_NAME_LEN */
#include "history.h" /* class History */
#include "compute.h" /* compute_term, enum RvComputeFlags */
#include <string.h>
#include <assert.h>

#define OBJECT_ATTACH_KEY "EvalWindowObj"

struct _EvalWindow
{
	Relview * rv;
	GtkWidget * window;
	GtkWidget * comboHistory;
	GtkWidget * entryTerm;
	GtkWidget * entryResult;
	GtkWidget * checkAsserts;
	GtkWidget * checkOverwrite;

	GtkWidget * hboxInput;
	GtkWidget * textviewConsole;
	GtkWidget * scrolledConsole;

	gboolean isConsoleVisible;

	History history;
};

static EvalWindow * _eval_window_get (GtkWidget * widget);
static EvalWindow * _eval_window_new ();
static void _eval_window_destroy (EvalWindow * self);
static void _update_history (EvalWindow * self, const gchar * term,
		const gchar * result);
static void _fillComboBox (GtkComboBox *cb, const GList * items,
                          gint selectedNo);
static void _addToComboBox (gpointer * str, gpointer * cb);
static gchar * _eval_window_get_term (EvalWindow * self, gboolean * pmultiline);
static void _eval_window_show_console (EvalWindow * self, gboolean yesno);


EvalWindow * _eval_window_get (GtkWidget * widget)
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
		= g_object_get_data (G_OBJECT(widget), OBJECT_ATTACH_KEY);

	  if (pdata) {
		return (EvalWindow*) pdata;
	  }

	  widget = gtk_widget_get_parent (widget);
	}
  }

  assert (FALSE);
  return NULL;
}

static EvalWindow * _eval_window = NULL;

EvalWindow * eval_window_get_instance()
{
	if (! _eval_window)
		_eval_window = _eval_window_new ();
	return _eval_window;
}

EvalWindow * _eval_window_new ()
{
	EvalWindow * self = g_new0 (EvalWindow,1);
	GtkBuilder * builder = NULL;

	self->rv = rv_get_instance ();
	builder = rv_get_gtk_builder (self->rv);

#define LOOKUP(name) GTK_WIDGET(gtk_builder_get_object(builder,(name)))
	self->window = LOOKUP("windowEval");
	self->entryResult = LOOKUP("entryEvalResultRel");
	self->entryTerm = LOOKUP("entryEvalTerm");
	self->comboHistory = LOOKUP("comboboxEvalHistory");
	self->checkAsserts = LOOKUP("cbCheckAssertionsEval");
	self->checkOverwrite = LOOKUP("cbForceOverwriteEval");
	self->hboxInput = LOOKUP("hboxEvalInput");
	self->scrolledConsole = LOOKUP("scrolledEvalConsole");
	self->textviewConsole = LOOKUP("textviewEvalConsole");
#undef LOOKUP

	/* The default display for combo boxes with many entries seems to be
	 * broken on my machine. Using a list-like display solves the problem. */
	{
		gtk_rc_parse_string (
				"style \"my-style\" {\n"
				"   GtkComboBox::appears-as-list = 1\n"
				"   GtkComboBox::shadow-type = GTK_SHADOW_NONE\n"
				"}\n"
				"widget \"*.mycombo\" style \"my-style\"");
		gtk_widget_set_name (self->comboHistory, "mycombo");
	}

	/* When RelView starts, the single line term input widget is
	 * visible. */
	self->isConsoleVisible = FALSE;
	g_object_ref (G_OBJECT(self->scrolledConsole));

	/* Attach the window widget to the window. You can call _*_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(self->window), OBJECT_ATTACH_KEY, (gpointer) self);

	/* Load and show the current history in the combo box. */
	history_new_from_glist(&self->history, prefs_get_int("settings",
			"maxhistorylength", 15), prefs_restore_glist("evalhistory"));
	if (!history_is_empty(&self->history)) {
		_fillComboBox(GTK_COMBO_BOX (self->comboHistory), history_get_glist(
				&self->history), -1);
	}

	{
		Workspace * workspace = rv_get_workspace(self->rv);

		/* Add the window to the workspace. Also restores position and
		 * visibility. */
		workspace_add_window (workspace, GTK_WINDOW(self->window), "eval-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
			eval_window_show(self);
	}

	return self;
}

GtkWidget * eval_window_get_widget (EvalWindow * self) { return self->window; }

/*!
 * Callback: a history combobox item has been selected => fill
 * entries accordingly.
 *
 * \author wl, stb
 * \date 20-NOV-2000
 *
 * \param cb The combo box that has been changed.
 * \param user_data
 */
void on_comboboxEvalHistory_changed (GtkComboBox * cb, gpointer user_data)
{
	EvalWindow * self = _eval_window_get(GTK_WIDGET(cb));
	gchar * term = gtk_combo_box_get_active_text(cb);
	gchar * equal = NULL;

	if (term != NULL) {
		equal = strchr(term, '=');
		if (equal == NULL)
			fprintf(stderr, "Invalid item in history list.\n");
		else {
			*equal = '\0'; /* term = "<result>\s*\0\s<term>" */

			gtk_entry_set_text(GTK_ENTRY (self->entryResult), term);
			gtk_entry_set_text(GTK_ENTRY (self->entryTerm), equal + 1/*'='*/);

			/* Hide the console if necessary. */
			_eval_window_show_console (self, FALSE);
		}
		g_free(term);
	}
}

/*!
 * Callback: the clear button has been clicked => clear history
 *
 * \author wl, stb
 * \date 20-NOV-2000
 *
 * \param button
 * \param user_data
 */
void on_buttonClearEvalHistory_clicked (GtkButton * button, gpointer user_data)
{
	EvalWindow * self = _eval_window_get(GTK_WIDGET(button));
	GtkListStore * model =
			GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (self->comboHistory)));

	history_free(&self->history);

	gtk_combo_box_set_active(GTK_COMBO_BOX (self->comboHistory), -1);
	gtk_list_store_clear(model);

	gtk_entry_set_text(GTK_ENTRY (self->entryTerm), "");
	gtk_entry_set_text(GTK_ENTRY (self->entryResult), "");
}

gchar * _eval_window_get_term (EvalWindow * self, gboolean * pmultiline)
{
	if (pmultiline) *pmultiline = self->isConsoleVisible;

	if ( !self->isConsoleVisible) {
		return g_strdup (gtk_entry_get_text(GTK_ENTRY (self->entryTerm)));
	}
	else {
		GtkTextIter start, end;
		GtkTextBuffer * buf = gtk_text_view_get_buffer
				(GTK_TEXT_VIEW(self->textviewConsole));
		gtk_text_buffer_get_start_iter(buf, &start);
		gtk_text_buffer_get_end_iter(buf, &end);
		return gtk_text_buffer_get_text(buf, &start, &end, FALSE);
	}
}

void _eval_window_show_console (EvalWindow * self, gboolean yesno)
{
	if (self->isConsoleVisible == yesno) return;
	else {
		GtkWidget * old_widget, * new_widget;

		if ( !self->isConsoleVisible) {
			/* Replace the single line input with the console. */
			old_widget = self->entryTerm;
			new_widget = self->scrolledConsole;

			gtk_widget_set_sensitive (self->entryResult, FALSE);
		}
		else {
			old_widget = self->scrolledConsole;
			new_widget = self->entryTerm;

			gtk_widget_set_sensitive (self->entryResult, TRUE);
		}

		g_object_ref (G_OBJECT(old_widget));
		gtk_container_remove(GTK_CONTAINER(self->hboxInput), old_widget);
		gtk_box_pack_start(GTK_BOX(self->hboxInput), new_widget, TRUE, TRUE, 0);
		g_object_unref(G_OBJECT(new_widget));

		gtk_box_reorder_child(GTK_BOX(self->hboxInput), new_widget, 0);

		self->isConsoleVisible = !self->isConsoleVisible;
	}
}

/*!
 * Callback: the eval button has been clicked => evaluate term
 *
 * \author wl, stb
 * \date 21-JUL-2000
 *
 * \param button
 * \param user_data
 */
void on_buttonEval_clicked(GtkButton * button, gpointer user_data)
{
	EvalWindow * self = _eval_window_get(GTK_WIDGET(button));
	gchar * result;

	result = g_strdup(gtk_entry_get_text(GTK_ENTRY (self->entryResult)));

	/* remove leading and trailing whitespaces */
	g_strstrip (result);

	if (0 == strcmp(result, "")) {
		g_free(result);
		result = g_strdup("$");
	}

	/* check if the relation name for the result is valid */
	if (!namespace_is_valid_name(rv_get_namespace(self->rv), result)) {
		if (!rv_ask_rel_name_def(&result, TRUE)) {
			g_free(result);
			return;
		} else {
			/* set the entered relation name in the evaluation window */
			gtk_entry_set_text(GTK_ENTRY(self->entryResult), result);
		}
	}

	/* Evaluate */
	{
		RvComputeFlags flags = 0;
		gboolean multiline;
		gchar * term = NULL;

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (self->checkAsserts))) {
			flags |= RV_COMPUTE_FLAGS_CHECK_ASSERTIONS;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (self->checkOverwrite))) {
			flags |= RV_COMPUTE_FLAGS_FORCE_OVERWRITE;
		}

		term = _eval_window_get_term(self, &multiline);
		if (term)
		{
			if (multiline) {
				/* Parse the commands from the input. */
				gchar ** lines = g_strsplit(term, "\n", 0), **p;
				GList * l = NULL, *iter;
				gboolean go_on = FALSE;
				gint i;

				for (p = lines ; *p ; p ++) {
					gboolean go_on_now = go_on;
					gsize len = strlen (*p);

					/* If the last character on the line is a character
					 * and there is another line left concatenate them. */
					if ((*p)[len-1] == '\\' && *(p+1)) {
						(*p)[len-1] = '\n';
						go_on = TRUE;
					}
					else go_on = FALSE;

					if (go_on_now) {
						g_string_append ((GString*)l->data, *p);
					}
					else {
						l = g_list_prepend (l, g_string_new (*p));
					}
				}

				l = g_list_reverse (l);
				g_strfreev(lines);

#if 0
				for (i = 0, iter = l ; iter ; iter = iter->next, ++i) {
					printf ("Got command %d: \"%s\"\n", i, ((GString*)iter->data)->str);
				}
#endif

				/* Now execute the commands in the given order. Commands must
				 * have the form "<rel> = <term>" */
				for (iter = l ; iter ; iter = iter->next) {
					GString * cmd = (GString*)iter->data;
					gchar ** arr = g_strsplit(cmd->str, "=", 3);
					if (g_strv_length(arr) != 2) {
						rv_user_error ("Invalid command", "Missing '=' in "
								"\"%s\". Format is <rel> = <term>. Remaining "
								"commands are ignored!", cmd->str);
						break;
					}
					else /* execute */ {
						compute_term(arr[1], arr[0], flags);
					}

					g_strfreev(arr);
				}

				for (iter = l ; iter ; iter = iter->next)
					g_string_free((GString*)iter->data, TRUE);
				g_list_free(l);
			}
			else
			{
				compute_term(term, result, flags);

				/* Only store single line commands in the history.
				 * TODO: This should be changed in the future. */
				_update_history(self, term, result);
			}

			g_free(term);
		}

	}

	/* clean up */
	g_free(result);
}


void on_buttonCloseEvalWindow_clicked (GtkButton * button,
                                       gpointer user_data)
{
  eval_window_hide (_eval_window_get(GTK_WIDGET(button)));
}



/*! frees all allocated resources
 *
 * \author wl, stb
 * \date 21-JUL-2000 WL
 */
void _eval_window_destroy (EvalWindow * self)
{
  /* store history to prefs and free list */
  prefs_store_glist ("evalhistory", history_get_glist (&self->history));
  history_free (&self->history);

  /* TODO: unref the entry widget which is currently not visible. */
}


/*!
 * Appends the given string in `str` to the given combo box `cb`.
 *
 * \author stb
 * \date 04.08.2008
 *
 * \param str The string to append to the combo box.
 * \param cb The combo box handle.
 */
void _addToComboBox (gpointer * str, gpointer * cb)
{
  gtk_combo_box_append_text ((GtkComboBox*) cb, (const gchar*) str);
}


/*!
 * Fills the combo box `cb` with the elements of the given list `items`.
 *
 * \author stb
 * \date 04.08.2008
 *
 * \param cb The combo box to fill.
 * \param items The item list to use.
 * \param selectedNo Determines which element will be selected after adding
 *                   the items to the combo box. Begins with 0 for the first
 *                   item.
 */
void _fillComboBox (GtkComboBox *cb, const GList * items,
                          gint selectedNo)
{
  g_list_foreach (items, (GFunc) _addToComboBox, (gpointer) cb);
  gtk_combo_box_set_active (cb, selectedNo);
}



/*! Create window if necessary and show it.
 *
 * \author wl, stb
 * \date 21-JUL-2000
 */
void eval_window_show (EvalWindow * self)
{
  //prefs_restore_window_info_with_name (GTK_WINDOW (self->window), "windowEval");
  gtk_widget_show (self->window);
}


/*! Returns TRUE if window is created and visible.
 *
 * \author stb
 * \date 04.08.2008
 * \return Returns TRUE, if the evaluation window is visible.
 */
gboolean eval_window_is_visible (EvalWindow * self)
{
#if GTK_MINOR_VERSION >= 18
	return gtk_widget_get_visible (self->window);
#else
	return GTK_WIDGET_VISIBLE(self->window);
#endif
}


/*! Dismiss window. The window is not destroyed.
 *
 * \author stb
 * \date 04.08.2008
 */
void eval_window_hide (EvalWindow * self)
{
	gtk_widget_hide (self->window);
}


/*!
 * Adds the evaluated input (<result>=<term>) into the history combo box.
 *
 * \author wl, stb
 * \date 01-SEP-2000
 *
 * \param term The evaluated right side of the assignment.
 * \param result The resulting relation identifier.
 */
void _update_history (EvalWindow * self, const gchar * term,
		const gchar * result)
{
  /* assemble new history term */
  GtkComboBox * cb = GTK_COMBO_BOX (self->comboHistory);
  gchar * newitem = g_strdup_printf ("%s=%s", result, term);
  gint pos = -1;
  GtkTreePath * path = NULL;
  GtkTreeIter iter;
  GtkTreeModel * model = gtk_combo_box_get_model (cb);

  pos = history_position (&self->history, newitem);
  if (pos >= 0) {
    path = gtk_tree_path_new_from_indices (pos, -1 /*END*/);

    gtk_tree_model_get_iter (model, &iter, path);
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

    /* The iterator have not be freed using `gtk_tree_iter_free` */
    gtk_tree_path_free (path);
  }

  history_prepend (&self->history, newitem);
  prefs_store_glist ("evalhistory", history_get_glist (&self->history));

  /* Prepend the (new) element to the combo box and set it active. */
  gtk_combo_box_prepend_text (cb, newitem);
  gtk_combo_box_set_active (cb, 0);
}


void on_buttonEvalHelp_clicked (GtkButton * button, gpointer user_data)
{
	/* Show the functions reference. */
	gchar * path = rv_find_data_file ("base_functions.pdf");
	if ( !path) {
		rv_user_error ("Unable to show help", "Sorry, I'm unable to find the "
				"file \"base_functions.pdf\" in RelView's shared data directory.");
	}
	else {
		gchar * uri = g_strconcat ("file://", path, NULL);
		GError * err = NULL;
		gboolean success;

		/* Note: This won't work on Win32. */
		success = gtk_show_uri(NULL, uri, GDK_CURRENT_TIME, &err);
		if ( !success) {
			rv_user_error ("Unable to show help", "Unable to show URI \"%s\". "
					"Reason: %s", uri, err->message);
			g_error_free(err);
		}

		g_free (uri);
		g_free (path);
	}
}


void on_buttonEvalExpand_clicked (GtkButton * button, gpointer user_data)
{

	EvalWindow * self = _eval_window_get(GTK_WIDGET(button));

	_eval_window_show_console (self, !self->isConsoleVisible);
}
