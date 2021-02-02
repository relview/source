/*
 * IterWindow.c
 *
 *  Copyright (C) 2010 Stefan Bolus, University of Kiel, Germany
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

#include "IterWindow.h"
#include "Relview.h"
#include <assert.h>

#define OBJECT_ATTACH_KEY "IterWindowObj"

struct _IterWindow
{
	Relview * rv;
	GtkWidget * window;
	GtkWidget * comboboxFuns;
	GtkWidget * entryInput, *entryOutput;
	GtkWidget * spinbuttonNum;

	FunManagerObserver fm_observer;
	ProgManagerObserver pm_observer;
};


static IterWindow * _iter_window_get (GtkWidget * widget);
static IterWindow * _iter_window_new ();
static void _iter_window_destroy (IterWindow * self);


IterWindow * _iter_window_get (GtkWidget * widget)
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
		return (IterWindow*) pdata;
	  }

	  widget = gtk_widget_get_parent (widget);
	}
  }

  assert (FALSE);
  return NULL;
}

static IterWindow * _window = NULL;

IterWindow * iter_window_get_instance()
{
	if (! _window)
		_window = _iter_window_new ();
	return _window;
}

void iter_window_destroy_instance()
{
	if (_window) {
		_iter_window_destroy (_window);
		_window = NULL;
	}
}

/*!
 * Updates the combobox for the functions and programs after a manager has
 * emitted a 'changed' signal.
 */
static void _update_funs_and_progs (IterWindow * self)
{
	GList * l = NULL, *iter;
	GtkListStore * model = NULL;
	GtkTreeIter tree_iter = {0};

	model = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(self->comboboxFuns)));
	gtk_list_store_clear(model);

	/* Collect the names of all functions and programs and sort them
	 * case-insensitive by their name. */
	l = g_list_concat(fun_manager_get_names(rv_get_fun_manager(self->rv)),
			prog_manager_get_names (rv_get_prog_manager(self->rv)));
	l = g_list_sort (l, (GCompareFunc) g_strcasecmp);

	for (iter = l ; iter ; iter = iter->next) {
		gtk_list_store_append (model, &tree_iter);
		gtk_list_store_set (model, &tree_iter, 0/*name*/, iter->data,
				-1);
	}
}

/*!
 * Callback for a \ref FunManager object.
 */
static void _on_fun_manager_changed (gpointer user_data, FunManager * manager)
{
	IterWindow * self = (IterWindow*) user_data;
	_update_funs_and_progs (self);
}

/*!
 * Callback for a \ref ProgManager object.
 */
static void _on_prog_manager_changed (gpointer user_data, ProgManager * manager)
{
	IterWindow * self = (IterWindow*) user_data;
	_update_funs_and_progs (self);
}

IterWindow * _iter_window_new ()
{
	IterWindow * self = g_new0 (IterWindow,1);
	GtkBuilder * builder = NULL;
	Workspace * workspace = NULL;

	self->rv = rv_get_instance ();
	builder = rv_get_gtk_builder (self->rv);

#define LOOKUP(name) GTK_WIDGET(gtk_builder_get_object(builder,(name)))
	self->window 		= LOOKUP("dialogIteration");
	self->comboboxFuns 	= LOOKUP("comboboxFunsIW");
	self->entryInput 	= LOOKUP("entryInputIW");
	self->entryOutput 	= LOOKUP("entryOutputIW");
	self->spinbuttonNum = LOOKUP("spinbuttonNumIW");
#undef LOOKUP

	/* Attach the window widget to the window. You can call _*_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(self->window), OBJECT_ATTACH_KEY, (gpointer) self);

	workspace = rv_get_workspace(self->rv);

	/* Add the window to the workspace. Also restores position and
	 * visibility. */
	workspace_add_window (workspace, GTK_WINDOW(self->window), "iteration-window");

	if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
		iter_window_show(self);

	/* We use observers to update the function/program list if something
	 * has changed. */
	self->fm_observer.changed  = _on_fun_manager_changed;
	self->fm_observer.object = (gpointer) self;
	fun_manager_register_observer(rv_get_fun_manager(self->rv), &self->fm_observer);

	self->pm_observer.changed  = _on_prog_manager_changed;
	self->pm_observer.object = (gpointer) self;
	prog_manager_register_observer(rv_get_prog_manager(self->rv), &self->pm_observer);

	/* Update the functions and programs in the combobox. */
	_update_funs_and_progs (self);

	return self;
}

GtkWidget * iter_window_get_widget (IterWindow * self) { return self->window; }


void _iter_window_destroy (IterWindow * self)
{
	/* TODO: unref the entry widget which is currently not visible. */
	g_free (self);
}

/*!
 * Create window if necessary and show it.
 */
void iter_window_show (IterWindow * self)
{
  gtk_widget_show (self->window);
}


/*!
 * Returns TRUE if window is created and visible.
 */
gboolean iter_window_is_visible (IterWindow * self)
{
#if GTK_MINOR_VERSION >= 18
	return gtk_widget_get_visible (self->window);
#else
	return GTK_WIDGET_VISIBLE(self->window);
#endif
}


/*!
 * Dismiss window. The window is not destroyed.
 */
void iter_window_hide (IterWindow * self)
{
	gtk_widget_hide (self->window);
}


G_MODULE_EXPORT void on_buttonCloseIW_clicked (GtkButton * button, gpointer user_data)
{
	iter_window_hide (_iter_window_get (GTK_WIDGET(button)));
}


G_MODULE_EXPORT void on_buttonEvalIW_clicked (GtkButton * button, gpointer user_data)
{
	IterWindow * self = _iter_window_get (GTK_WIDGET(button));
	GtkTreeModel * model;
	GtkTreeIter iter = {0};

	model = gtk_combo_box_get_model (GTK_COMBO_BOX(self->comboboxFuns));
	if ( !gtk_combo_box_get_active_iter (GTK_COMBO_BOX(self->comboboxFuns), &iter)) {
		rv_user_error("No function/program selected", "Please select a function or program.");
	}
	else {
		const gchar * input, *fun;
		int n, i;
		gchar * output = NULL;

		RelManager * manager = rv_get_rel_manager(self->rv);
		Rel * R = NULL;

		gtk_tree_model_get (model, &iter, 0, &fun, -1);

		n = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(self->spinbuttonNum));

		input = gtk_entry_get_text (GTK_ENTRY(self->entryInput));
		output = g_strdup (gtk_entry_get_text (GTK_ENTRY(self->entryOutput)));
		output = g_strstrip (output);

		/* Use $ as the result, if no name was given. */
		if (strlen (output) < 1) {
			g_free (output);
			output = g_strdup ("$");
		}

		R = rel_manager_get_by_name(manager, input);
		if (!R) {
			rv_user_error ("No such relation", "There is no relation with name \"%s\".", input);
		}
		else {
			lua_State * L = rv_lang_new_state(self->rv);
			char * lua_rel_name = NULL;
			KureError * err = NULL;

			/* Apply naming conventions which might be hidden in the Kure library
			 * (e.g. $ to __dollar). */
			lua_rel_name = kure_lang_expr_to_lua (input, &err);
			if (!lua_rel_name) {
				/* This is an internal error and should not be displayed to the
				 * user. */
				g_warning ("Unable to convert expression to LUA. Reason: %s.\n",
						err ? err->message : "Unknown");
				if (err) kure_error_destroy(err);
			}
			else {
				/* if n=0, use the identity. Otherwise, iterate n times. */
				gchar * lua_code = g_strdup_printf (
						"_tmp = %s\n"
						"if (0==%d) then _tmp = kure.compat.I(_tmp)\n"
						"          else for i = 1, %d do _tmp = %s(_tmp) end end\n"
						"return _tmp",
						lua_rel_name, n, n, fun);
				KureRel * impl = NULL;

				impl = kure_lua_exec(L, lua_code, &err);
				if (!impl) {
					printf ("Error. Reason: %s\n", err?err->message:"Unknown");
					if (err) kure_error_destroy(err);
				}
				else {
					Rel * res = rel_new_from_impl (output, impl);

					// compute.c
					gboolean inserted = rv_user_rename_or_not (self->rv, res);
					if (!inserted) {
						rel_destroy (res);
					}
					else {
						relation_window_set_relation (relation_window_get_instance(), res);
					}
				}

				g_free (lua_code);
				free (lua_rel_name);
			}

			lua_close (L);
		}

		g_free (output);
	}
}
