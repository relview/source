/*
 * DebugWindow.c
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

#include "Function.h"
#include "Domain.h"
#include "Program.h"
#include "RelationViewport.h"
#include "RelationWindow.h"
#include "Relview.h" /* rv_get_glade_xml */
#include "prefs.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <lauxlib.h> // luaL_typename

#include "DebugWindow.h"


/*******************************************************************************
 *                             Auxiliary Functions                             *
 *                              Fri, 11 Feb 2011                               *
 ******************************************************************************/

// lineno is 1-indexed. Returns NULL is the given line is out of bounds.
// Does not include the trailing newline.
static gchar * _getline_in_string (const gchar * s, int lineno)
{
	int curline = 1;
	const gchar * p = s;
	while (*p && curline < lineno) {
		if (*p == '\n') curline ++;
		p++;
	}
	if (!*p) return NULL;
	else {
		size_t linelen = 0;
		const gchar * linestart = p;
		while (*p && *p != '\n') { linelen++; p++; }
		return g_strndup (linestart, linelen);
	}
}


static gchar * _extract_name (const gchar * def)
{
	static GRegex * regex = NULL;
	GMatchInfo * match_info;
	gchar * ret = NULL;

	if ( !regex) {
		const gchar * pattern = "function\\s+([a-zA-Z_][a-zA-Z0-9_]*)";
		regex = g_regex_new(pattern, 0, 0, NULL);
		g_assert (regex != NULL);
	}

	if ( !g_regex_match(regex, def, 0, &match_info))
		return NULL;
	else {
		ret = g_match_info_fetch (match_info, 1);
		g_match_info_free(match_info);
	}

	return ret;
}


// ar must have been creates using lua_getinfo with nls
static gchar * _get_lua_fun_name (lua_Debug * ar)
{
	if (ar->what[0] == 'C') {
		if (ar->name)
			return g_strdup (ar->name);
		else
			return g_strdup ("(Unknown C function)");
	}
	else if (g_str_equal(ar->what, "tail")) /* tail call */ {
		return g_strdup ("(optimized out)");
	}
	else {
		if (ar->namewhat[0] == '\0') {
			if (ar->source) {
				gchar * line = _getline_in_string (ar->source, ar->linedefined);
				if (line) {
					gchar * fname = _extract_name (line);
					return fname;
				}
				else return g_strdup(ar->short_src);
			}
			else return g_strdup(ar->short_src);

		}
		else if (0 == g_ascii_strcasecmp(ar->namewhat, "lua")
				|| 0 == g_ascii_strcasecmp(ar->namewhat, "global")
				|| 0 == g_ascii_strcasecmp(ar->namewhat, "field")
				|| 0 == g_ascii_strcasecmp(ar->namewhat, "local")) {
			if(ar->name) return g_strdup (ar->name);
			else {
				/* extract the name from the source code if possible. */
				gchar * line = _getline_in_string (ar->source, ar->linedefined);
				verbose_printf (VERBOSE_DEBUG, "_get_lua_fun_name: "
						"ar->name is NULL. Trying to read the name from the source ...\n");
				if (line) {
					gchar * fname = _extract_name (line);
					return fname;

				}
				else {
					return g_strdup(ar->short_src);
				}
			}
		}
		else {
			return g_strdup (ar->short_src);
		}
	}
}


/*******************************************************************************
 *                           Typedefs and Structures                           *
 *                              Fri, 11 Feb 2011                               *
 ******************************************************************************/

typedef enum _DebugWindowResp {
  RESP_NEXT, RESP_GO, RESP_GO_END, RESP_CLOSE
} DebugWindowResp;


typedef enum _DebugWindowNodeType
{
	TYPE_NIL, TYPE_LUA_FUN, TYPE_REL, TYPE_DOM, TYPE_INT, TYPE_STRING, TYPE_UNKNOWN
} DebugWindowNodeType;

/* Additional information, that should be stored for a relation with
 * respect to debugger needs. */
typedef struct _DebugWindowRelationDecoration DebugWindowRelationDecoration;
struct _DebugWindowRelationDecoration
{
  gchar *colLabelName, *rowLabelName; /*!< Must be freed. */
};

struct _DebugWindow
{
	Relview * rv;
	lua_State * L;

  GtkWidget * window;
  RelationViewport * viewport;

  GtkWidget * treeview;
  GtkTreeStore * treestore;
  GtkTreeSelection * treesel;

  GtkWidget * relPopup;

  GtkWidget * console;

  DebugWindowResp resp;

  /* for the status bar. */
  guint statusContextId;

  RelationObserver relObserver;

  GHashTable * relDecoHash;

  gboolean is_hooked; /*!< TRUE if a Lua hook is installed. If yes, stepping
				       * is in effect. If not, debugging is disabled. */

  int prog_depth_count; /* Program depth, that currently we debug. */
};


/*******************************************************************************
 *                             Function Prototypes                             *
 *                              Wed, 21 Apr 2010                               *
 ******************************************************************************/

static void 	_debug_window_load_from_xml(DebugWindow * self);
static void 	_debug_window_console_print(DebugWindow * self, const char * str, int len);
static void		_debug_window_console_printf(DebugWindow * self, const gchar * fmt, ...);

static void 	_debug_window_clear_stack_display (DebugWindow * self);
void 			_debug_window_reset (DebugWindow * self);

void 			_debug_window_enable_debugging (DebugWindow * self);
void 			_debug_window_disable_debugging (DebugWindow * self);

void 			_debug_window_update_state(DebugWindow * self);

static gboolean _debug_window_shall_check_assertions(DebugWindow * self);
static gboolean _debug_window_shall_auto_close_window(DebugWindow * self);
static void 	_debug_window_statusbar_set(DebugWindow * self, const gchar * msg);


/*******************************************************************************
 *                            Function Definitions                             *
 *                              Wed, 21 Apr 2010                               *
 ******************************************************************************/

//
static int _debug_window_give_control (DebugWindow * self)
{
	gtk_main ();
	return self->resp;
}

/*!
 * Returns the state of the "Check Assertions" check box in the given
 * \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \return Returns TRUE, if the check box is checked, FALSE otherwise.
 */
static gboolean _debug_window_shall_check_assertions(DebugWindow * self)
{
	GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());
	GtkWidget * widget = GTK_WIDGET(gtk_builder_get_object(builder, "cbCheckAssertions_DW"));
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

/*!
 * Set the message in the status bar for the given \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \param msg The message to set.
 */
static void _debug_window_statusbar_set(DebugWindow * self, const gchar * msg)
{
	GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());
	GtkWidget * widget = GTK_WIDGET(gtk_builder_get_object(builder, "statusbar_DW"));

	gtk_statusbar_pop(GTK_STATUSBAR(widget), self->statusContextId);
	gtk_statusbar_push(GTK_STATUSBAR(widget), self->statusContextId, msg);
}


/*!
 * Returns the state of the "Close window automatically" check box in the given
 * \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \return Returns TRUE, if the check box is checked, FALSE otherwise.
 */
static gboolean _debug_window_shall_auto_close_window(DebugWindow * self)
{
	GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());
	GtkWidget * widget = GTK_WIDGET(gtk_builder_get_object(builder, "cbAutoCloseWindow_DW"));
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

/*!
 * Called whenever Lua starts to interpret a line.
 */
static void _debug_window_on_lua_line_hook (lua_State * L, lua_Debug * ar)
{
	/* (1) Update the stack
	 * (2) Update the display; the relation may have changed.
	 * (3) Show the line in the console
	 * (4) Give over the control to the user. */

	DebugWindow * self = debug_window_get_instance();
	int success = lua_getinfo(L, "nS", ar);

	if (success) {
		if (ar->source[0] == '@') {
			verbose_printf (VERBOSE_TRACE, "(Internal call in file %s)\n", ar->source+1);
			return;
		}
		else {
			gchar * line = _getline_in_string (ar->source, ar->currentline);
			if ( !line) {
				_debug_window_console_printf (self, "(Line information invalid or incomplete)\n");
			}
			else {
				gchar * prog_name = _get_lua_fun_name(ar);
				gchar * linePtr = line;


				/* Each line can contain a first part of the form __line=%d;
				 * which is used inside Kure to track line numbers in the
				 * original source files. This part has to be removed here.
				 * One could think about using it for our own purpose someday. */
				if (g_regex_match_simple ("^\\s*__line\\s*=\\s*\\d+;", linePtr, 0, 0))
					linePtr = 1+strchr (linePtr,';');

				/* For each Relview program containing local declarations, Kure
				 * add corresponding local declarations in the generated Lua
				 * code. These are of the form "local <varname>" and should be
				 * ignored in the output. But:
				 *
				 * NOTE: Local declarations are lines and a "step" means to
				 * "execute" a line. When we encounter a local declaration from
				 * the user's perspective, nothing happens. So it is could
				 * better not to hide these messages. */

				linePtr = g_strstrip (linePtr);

				_debug_window_console_print (self, linePtr, -1);
				_debug_window_console_printf (self, " (prog: %s)\n", prog_name);

				g_free(prog_name);
				g_free(line);
			}
		}
	}
	else {
		_debug_window_console_printf (self, "(No line information available)\n");
	}

	_debug_window_update_state(self);

	{
		DebugWindowResp resp = _debug_window_give_control(self);
		if (resp == RESP_CLOSE)
			luaL_error(L, "User has canceled.");
	}
}


/*!
 * Callback for \ref lua_sethook.
 */
void _debug_window_lua_hook_func (lua_State * L, lua_Debug * ar)
{
	static struct { int type; char * name; } events[] = {
			{ LUA_HOOKCALL, "call" },
			{ LUA_HOOKLINE, "line" },
			{ LUA_HOOKRET, "return" },
			{ LUA_HOOKTAILRET, "tail" },
			{-1, "unknown"}}, *iter = NULL;

	if (g_verbosity >= VERBOSE_TRACE) {
		for (iter = events ; iter->type != -1 ; ++iter)
			if (iter->type == ar->event) break;

		verbose_printf (VERBOSE_TRACE, "_debug_window_lua_hook_func: Got a %s event.\n", iter->name);
	}

	switch (ar->event) {
	//case LUA_HOOKCALL: break;
	case LUA_HOOKLINE: _debug_window_on_lua_line_hook(L, ar); break;
	//case LUA_HOOKRET: break;
	//case LUA_HOOKTAILRET: break;
	default:
		;
	}
}

void _debug_window_enable_debugging (DebugWindow * self)
{
	if ( !self->is_hooked) {
		/* Install the call, line and return hook. */
		verbose_printf (VERBOSE_INFO, "Debugging is now enabled.\n");
		lua_sethook(self->L, _debug_window_lua_hook_func, LUA_MASKCALL
				| LUA_MASKLINE | LUA_MASKRET, 0);
		self->is_hooked = TRUE;
	}
}

void _debug_window_disable_debugging (DebugWindow * self)
{
	if (self->is_hooked) {
		verbose_printf (VERBOSE_INFO, "Debugging is now disabled.\n");
		lua_sethook(self->L, NULL, 0, 0);
		self->is_hooked = FALSE;
	}
}

/*!
 * Callback for gtk_tree_model_foreach. */
static gboolean _destroy_tree_store_entry (GtkTreeModel * model,
		GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
	gpointer ptr;
	DebugWindowNodeType type;

	gtk_tree_model_get(model, iter, 1/*column id*/, &type, 2, &ptr, -1 /*end*/);
	if (type == TYPE_REL) {
		Rel * rel = (Rel*) ptr;

		/* We must not destroy the implementation, since it belongs to the
		 * Lua state. */
		rel_steal_impl (rel);
		rel_destroy (rel);
	}
	else { }

	return FALSE; /*continue*/
}

/*!
 * Clears all elements from the tree view which is used to display the current
 * stack and its elements. Also does all necessary cleanups.
 */
void _debug_window_clear_stack_display (DebugWindow * self)
{
	gtk_tree_model_foreach (GTK_TREE_MODEL(self->treestore),
			_destroy_tree_store_entry, NULL/*user-data*/);
	gtk_tree_store_clear (self->treestore);
}


void _debug_window_reset (DebugWindow * self)
{
	GtkBuilder * builder = rv_get_gtk_builder(self->rv);

#define GET_WIDGET(name) GTK_WIDGET(gtk_builder_get_object(builder, name))

	/* There is no default relation in the viewport. */
	relation_viewport_set_relation(self->viewport, NULL);

	/* Set the default zoom level for the relation viewport. */
	relation_viewport_set_zoom(self->viewport, prefs_get_int("settings",
			"relationwindow_zoom", 3));

	{
		GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->console));
		gtk_text_buffer_set_text(buffer, "", 0);

		/* Set up the text in the console */
		_debug_window_console_printf(self, "The next statement is:\n");
	}

	_debug_window_clear_stack_display (self);

	/* (Re-)Enable the buttons. */
	gtk_widget_set_sensitive(GET_WIDGET("buttonNext"),TRUE);
	gtk_widget_set_sensitive(GET_WIDGET("buttonGo"),TRUE);

	/* (De-)Activate the checkboxes for assertion checking and automatically
	 * closing the debug window. Glade doesn't seem to do this for us. */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(GET_WIDGET("cbCheckAssertions_DW")), TRUE);
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(GET_WIDGET("cbAutoCloseWindow_DW")), FALSE);

	self->is_hooked = FALSE;

#undef GET_WIDGET
}


static int _debug_window_assert_failed_first (DebugWindow * self,
		lua_State * L, const char * name)
{
	gint resp; /* user response to "Open the debugger?" (yes/no) */

	/* show the user the failed assertion. */
	{
		lua_Debug ar = {0};
		int success;
		gchar * fun_name;
		GtkWidget * dialog;

		/* We shouldn't use the Lua code line number information, because it
		 * can differ significantly from the line number in the original
		 * RelView source code. */

		/* 0 is "assert_func" (we) and 1 is "assert" in "kure.compat". */
		success = lua_getstack (L, 2, &ar) && lua_getinfo(L, "lSn", &ar);
		if (success) {
			fun_name = _get_lua_fun_name(&ar);
		}
		else {
			g_warning ("debug_window_assert_failed_first: "
					"lua_getstack or lua_getinfo failed.\n");
			fun_name = g_strdup ("(unknown)");
		}

		dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_YES_NO, "Assertion \"%s\" failed.",
				name);

		gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
				"The assertion defined in program \"%s\" "
				"failed. Open the debugger?", fun_name);
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		g_free (fun_name);
	}

	if (GTK_RESPONSE_YES == resp)
	{
		self->L = L;

		g_assert ( !debug_window_is_visible(self));

		_debug_window_reset(self);
		debug_window_show(self);

		_debug_window_enable_debugging(self);

		/* We have now finished. The next step is to wait for the first
		 * hook call. */

	}
	else {
		/* the user don't want to open the debugger. */
	}

	return 0;
}

static int _debug_window_assert_failed_again (DebugWindow * self,
		lua_State * L, const char * name)
{
	lua_Debug ar = {0};
	int success;
	gchar * fun_name;

	if ( !_debug_window_shall_check_assertions(self)) return 0;

	/* We shouldn't use the Lua code line number information, because it
	 * can differ significantly from the line number in the original
	 * RelView source code. */

	/* 0 is "assert_func" (we) and 1 is "assert" in "kure.compat". */
	success = lua_getstack (L, 2, &ar) && lua_getinfo(L, "lSn", &ar);
	if (success) {
		fun_name = _get_lua_fun_name(&ar);
	}
	else {
		g_warning ("debug_window_assert_failed_again: "
				"lua_getstack or lua_getinfo failed.\n");
		fun_name = g_strdup ("(unknown)");
	}

    _debug_window_console_printf(self,
    		"  Assertion \"%s\" (progam: %s) failed.\n", name, fun_name);

    /* Debugging was disabled. Let's ask the user if he want to enable it
     * again. */
    if ( !self->is_hooked) {
		int resp;

		GtkWidget * dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_YES_NO, "Assertion \"%s\" failed.",
				name);

		gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
				"The assertion defined in program \"%s\" "
				"failed. Enable debugging again?", fun_name);
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (GTK_RESPONSE_YES == resp)
			_debug_window_enable_debugging(self);
    }

    g_free(fun_name);
    return 0;
}

/*!
 * Called from Lua if an assertion failed. The first argument is the name of
 * the assertion.
 */
int debug_window_assert_failed_func (lua_State * L)
{
	const char * name = lua_tostring (L, 1);
	DebugWindow * self = debug_window_get_instance();

	/* If an assertion fails for the first time, offer the user the option to
	 * open the debugger window and reset it's contents. If the debugger window
	 * is already visible, i.e. another assertion failed before, write this
	 * event to the console, but do nothing else. */
	if ( !debug_window_is_visible(self))
		return _debug_window_assert_failed_first (self, L, name);
	else
		return _debug_window_assert_failed_again (self, L, name);
}


void debug_window_finished_func (lua_State * L)
{
	DebugWindow * self = debug_window_get_instance();

	/* If the debugger isn't open, break immediately. */
	if ( !debug_window_is_visible(self)) return;
	else {
		GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());

		_debug_window_disable_debugging(self);

		/* After the evaluation there is neither a program on the stack, nor
		 * a local relation. */
		_debug_window_clear_stack_display (self);

		/* change the state in the statusbar and in the console */
		_debug_window_console_print(self,
				"---------- Evaluation finished ----------", -1);
		_debug_window_statusbar_set(self, "Evaluation finished.");

		/* Disable the buttons */
		gtk_widget_set_sensitive(
				GTK_WIDGET(gtk_builder_get_object(builder, "buttonNext")), FALSE);
		gtk_widget_set_sensitive(
				GTK_WIDGET(gtk_builder_get_object(builder, "buttonGo")), FALSE);

		/* Ask the user to close the window, if the corresponding check box is
		 * not activated. */
		if (_debug_window_shall_auto_close_window(self)) {
			goto close_the_window;
		}
		else {
			int resp;
			GtkWidget * dialog =
					gtk_message_dialog_new(GTK_WINDOW(self->window),
							GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
							GTK_BUTTONS_YES_NO, "Evaluation finished");

			gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog),
					"Close the debugger window?");
			gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
			resp = gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			if (GTK_RESPONSE_YES == resp) {
				goto close_the_window;
			}
		}

		/* if the user don't want to close the window, we start a new Gtk main queue, so
		 * the user can play around (with the empty debugger window). It can close the
		 * window using the delete-event. */
		_debug_window_give_control(self);
		return;

close_the_window:
		debug_window_hide(self);
	}
}


void debug_window_error_func (lua_State * L)
{
	DebugWindow * self = debug_window_get_instance();

	_debug_window_clear_stack_display(self);

	if (debug_window_is_visible(self)) {
		_debug_window_disable_debugging(self);
		debug_window_hide(self);
	}
}


/*!
 * Callback for the cell renderer for the 'dimension' column of the debug
 * window's tree view. Will be called before rendering is done. Used here to
 * set up the text for the column ("<rows> X <columns>").
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param tree_column
 * \param cell The GtkCellRenderer object.
 * \param tree_model Corresponding GtkTreeModel.
 * \param iter The current row.
 * \param data Always NULL.
 */
static
void _debug_window_dimension_cell_data_func(GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer *cell,
                                            GtkTreeModel *tree_model,
                                            GtkTreeIter *iter, gpointer data)
{
	gpointer ptr;
	DebugWindowNodeType type;

	gtk_tree_model_get(tree_model, iter, 1/*column id*/, &type, 2, &ptr, -1 /*end*/);
	if (TYPE_REL == type) {
		Rel * rel = (Rel*) ptr;
		gchar * str = rv_user_format_impl_dim (rel_get_impl(rel));

		g_object_set(G_OBJECT(cell), "text", str, "visible", TRUE, NULL /*end*/);
	    g_free(str);
	}
	else if (TYPE_LUA_FUN == type) {
		/* Ignore. */
		g_object_set(cell, "visible", FALSE, NULL /*end*/);
	}
	else /* no corresponding relation */ {
		const char * typename = (type == TYPE_NIL) ? "(Uninitialized)"
				: (type == TYPE_DOM) ? "(Domain)"
				: (type == TYPE_INT) ? "(Int)"
						: (type == TYPE_STRING) ? "(String)"
								: "(Unknown)";

		g_object_set(G_OBJECT(cell), "text", typename, "visible", TRUE, NULL /*end*/);
	}
}


/*!
 * Callback for the cell renderer for the 'name' column of the debug window's
 * tree view. Will be called before rendering is done. Used here to set up
 * font weight for programs (will be drawn bold).
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param tree_column
 * \param cell The GtkCellRenderer object.
 * \param tree_model Corresponding GtkTreeModel.
 * \param iter The current row.
 * \param data Always NULL.
 */
static
void _debug_window_name_cell_data_func(GtkTreeViewColumn *tree_column,
                                       GtkCellRenderer *cell,
                                       GtkTreeModel *tree_model,
                                       GtkTreeIter *iter, gpointer data)
{
	const gchar * name;
	DebugWindowNodeType type;

	gtk_tree_model_get(tree_model, iter, 0/*col id*/, &name,
			1, &type, -1 /*end*/);

	if (TYPE_LUA_FUN == type) {
		/* Draw program names bold. */
		g_object_set(G_OBJECT(cell), "weight", PANGO_WEIGHT_BOLD, "weight-set",
				TRUE, NULL /*end*/);
	}
	else {
		g_object_set(G_OBJECT(cell), "weight-set", FALSE, NULL /*end*/);
	}

	g_object_set(cell, "text", name, "visible", TRUE, NULL /*end*/);
}


/*!
 * Signal handler for the "changed" signal of the \ref DebugWindow's tree view.
 * Called, when the user selects a row from the tree view, i.e. a program, or
 * a relation.
 *
 * \note Actually the signal is emitted by the tree selection, that belongs
 *       to the tree view.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param treesel The GtkTreeSelection.
 * \param self The \ref DebugWindow.
 */
static void _debug_window_tree_sel_changed(GtkTreeSelection * treesel,
                                           DebugWindow * self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar * name;
	DebugWindowNodeType type;
	void * ptr;

	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &name, 1, &type, 2, &ptr,-1);

		if (NULL == ptr) {
			/* The user selected something which has no representation. Ignore
			 * the selection and do not change the relation shown on the right
			 * if there is one. */
		}
		else if (TYPE_REL == type) {
			relation_viewport_set_relation(self->viewport, (Rel*)ptr);
		}
		else { /* Similar to the first case. Maybe a domain. */ }
	}
	else relation_viewport_set_relation(self->viewport, NULL);

}


/*!
 * Signal handler for the "clicked" signal of the "Next" button. Called, when
 * the user clicks on the button.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param button The button handle.
 * \param self The \ref DebugWindow.
 */
static void _debug_window_on_next_clicked(GtkButton * button, DebugWindow * self)
{
  self->resp = RESP_NEXT;
  gtk_main_quit();
}


/*!
 * Signal handler for the "clicked" signal of the "Go" button. Called, when the
 * user clicks on the button.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param button The button handle.
 * \param self The \ref DebugWindow.
 */
static void _debug_window_on_go_clicked(GtkButton * button, DebugWindow * self)
{
	_debug_window_disable_debugging(self);
	self->resp = RESP_GO;
	gtk_main_quit();
}


/*!
 * Signal handler for the "delete-event" signal of the \ref DebugWindow's
 * GtkWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param widget The \ref DebugWindow's window that emitted the signal.
 * \param event
 * \param self The \ref DebugWindow.
 */
static gboolean _debug_window_on_delete(GtkWidget * widget, GdkEvent * event,
                                        DebugWindow * self)
{
	debug_window_hide (self);

	/* the question is, how to start a new gtk main queue, after the evaluation
	 * finished. */
	self->resp = RESP_CLOSE;
	gtk_main_quit();

	if (self->L) {
		/* It is not allowed to call luaL_error directly. The call does a long
		 * jump which seems to collide with GLib's signal emits. Instead, we
		 * use self->resp to advise the caller to throw an error.*/
		//luaL_error (self->L, ...);
	}
	else {
		g_warning ("_debug_window_on_delete: We don't have a Lua state. "
				"That's strange!");
	}

	return TRUE; /* don't call other handlers */
}


/*!
 * Loads the GUI from glade xml file, initializes some stuff, sets up signal
 * handlers an so on.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 */
void _debug_window_load_from_xml(DebugWindow * self)
{
	Relview * rv = self->rv;
	GtkBuilder * builder = rv_get_gtk_builder(rv);

#define GET_WIDGET(name) GTK_WIDGET(gtk_builder_get_object(builder, name))

	self->viewport = relation_viewport_new();
	self->window = GET_WIDGET("windowDebug");
	gtk_window_set_position (GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);

	g_assert(self->window != NULL);

  /* Load the popup menu for the relation viewport an set it. */
  //_debug_window_load_rel_popup_menu_from_xml(self);
  //relation_viewport_set_popup(self->viewport, self->relPopup,
  //RELATION_VIEWPORT_PREPARE_POPUP_FUNC (_debug_window_prepare_rel_popup), self);

  /* Tell the relation viewport, that we are responsible for handling
   * labels. */
  //relation_viewport_set_labels_func(self->viewport,
		  //RELATION_VIEWPORT_GET_LABELS_FUNC(_debug_window_get_labels_func), (gpointer) self);

	/* Add the relation viewport to the debug window and set its
	 * intial zoom. */
	{
		GtkWidget * frame = GET_WIDGET("frameDebugRelVp");
		gtk_container_add(GTK_CONTAINER(frame),
				relation_viewport_get_widget(self->viewport));
	}

	/* Show all children, but not the window itself. */
	gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(self->window)));

	self->treeview = GET_WIDGET("treeviewRelations");
	self->console = GET_WIDGET("textviewConsole");

	g_assert(self->treeview != NULL);
	g_assert(self->console != NULL);

	/* create a mark to let us scroll to the bottom of the textview
	 * (see below in _debug_window_on_step) */
	{
		GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->console));
		GtkTextIter iter;

		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "scroll", &iter, TRUE /* left gravity */);
	}

	/* prepare the treeview for local program environments */
	self->treestore = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_INT/*type*/, G_TYPE_POINTER);
	gtk_tree_view_set_model(GTK_TREE_VIEW(self->treeview),
			GTK_TREE_MODEL(self->treestore));

	{
		/* draw program names in bold font weight */

		GtkCellRenderer * renderer = gtk_cell_renderer_text_new();

		gtk_tree_view_insert_column_with_data_func(
				GTK_TREE_VIEW(self->treeview), 0 /*position*/, "Name",
				renderer,
				(GtkTreeCellDataFunc) _debug_window_name_cell_data_func, NULL,
				NULL);
	}

	{
		/* We must use another cell renderer here! */
		GtkCellRenderer * renderer = gtk_cell_renderer_text_new();

		gtk_tree_view_insert_column_with_data_func(
				GTK_TREE_VIEW(self->treeview), 1 /*position*/, "Dimension",
				renderer,
				(GtkTreeCellDataFunc) _debug_window_dimension_cell_data_func,
				NULL, NULL);
	}

	/* We use a tree selection object, instead of the tree view's `row-activated`
	 * signal, since the latter will only emitted on double click. That the
	 * selection's `changed` event is occasionally emitted without any change, is
	 * less inconvenient. */
	self->treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->treeview));
	gtk_tree_selection_set_mode(self->treesel, GTK_SELECTION_SINGLE);
	g_signal_connect_data(G_OBJECT(self->treesel), "changed",
			G_CALLBACK(_debug_window_tree_sel_changed), (gpointer) self, NULL,
			FALSE);

	/* Connect the signals */
	{
		GtkWidget * widget;

		widget = GET_WIDGET("buttonNext");
		g_signal_connect_data(G_OBJECT(widget), "clicked",
				G_CALLBACK(_debug_window_on_next_clicked), (gpointer) self, NULL, 0);

		widget = GET_WIDGET("buttonGo");
		g_signal_connect_data(G_OBJECT(widget), "clicked",
				G_CALLBACK(_debug_window_on_go_clicked), (gpointer) self, NULL, 0);

		g_signal_connect_data(G_OBJECT(self->window), "delete-event",
				G_CALLBACK(_debug_window_on_delete), (gpointer) self, NULL, 0);
	}

	/* Get a context id for our messages in the status bar. */
	self->statusContextId = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(GET_WIDGET("statusbar_DW")),
			"General messages");

	_debug_window_reset (self);

#undef GET_WIDGET
}


/*!
 * Creates a new \ref DebugWindow and initializes it.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \return Returns the newly created \ref DebugWindow instance.
 */
DebugWindow * debug_window_new()
{
  DebugWindow * self = g_new0(DebugWindow, 1);
  self->rv = rv_get_instance();

  _debug_window_load_from_xml(self);

  self->relDecoHash = g_hash_table_new(g_direct_hash, g_direct_equal);

  return self;
}


/*!
 * Destroys the given \ref DebugWindow an frees its memory on the heap.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 */
void debug_window_destroy(DebugWindow * self)
{
#warning "debug_window_destroy: NOT IMPLEMENTED!"
#if 0
	/* Unselect a currently selected relation (if present). This avoids future
   * indirect calls to the DebugWindow (e.g. because the relation was
   * deleted). */
  _debug_window_unselect_relation(self);

  g_object_unref(G_OBJECT(self->treestore));

  /* delete the text mark to scroll the textview to the bottom line */
  {
    GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->console));
    GtkTextMark * mark = gtk_text_buffer_get_mark(buffer, "scroll");
    gtk_text_buffer_delete_mark(buffer, mark);
  }

  /* destroy the top-level window */
  gtk_widget_destroy(self->window);

  /* Delete the hash table for the relation decorations. */
  g_hash_table_foreach(self->relDecoHash,
      (GHFunc) _debug_window_rel_deco_hash_foreach, (gpointer) self);

  /* the glade xml part don't have to be destroyed. Glade will reuse it
   * next time. AND ... there is no routine to destroy it. */

  g_free(self);
#endif
}


/*!
 * Append the given string to the text view in the given \ref DebugWindow.
 * The text view will automatically scrolled to the bottom, if necessary.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \param str The string to append to the text view.
 * \param len The length of the given string. Will be determined, if -1
 *            is given.
 */
void _debug_window_console_print(DebugWindow * self, const char * str,
                                        int len)
{
  GtkTextBuffer * buffer= NULL;
  GtkTextIter iter;
  GtkTextMark * mark;

  /* update the next step in the textview (see also the auto-scroll example in gtk-demo) */
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->console));
  gtk_text_buffer_get_end_iter(buffer, &iter);

  gtk_text_buffer_insert(buffer, &iter, str, len);

  /* scroll the textview to the bottom line. */
  mark = gtk_text_buffer_get_mark(buffer, "scroll");
  gtk_text_buffer_move_mark(buffer, mark, &iter);
  gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(self->console), mark);

  return;
}


/*!
 * Append the given formated string to the text view of the given
 * \ref DebugWindow. The text view ill automatically scrolled to the bottom,
 * if necessary.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \param fmt The format string as for \ref printf.
 */
void _debug_window_console_printf(DebugWindow * self, const gchar * fmt,
                                         ...)
{
	gchar * str;
	va_list ap;
	va_start (ap, fmt);
	str = g_strdup_vprintf(fmt, ap);

	_debug_window_console_print(self, str, -1);

	g_free(str);
	va_end (ap);
}


/*!
 * Returns the GtkWindow, which belongs to the \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \return Returns the GtkWindow casted to GtkWidget.
 */
GtkWidget * debug_window_get_window(DebugWindow * self)
{
  return self->window;
}


/*!
 * Shows s already created \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 */
void debug_window_show(DebugWindow * self)
{
  gtk_widget_show(debug_window_get_window(self));
}


/*!
 * Hides a \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 */
void debug_window_hide(DebugWindow * self)
{
  gtk_widget_hide(debug_window_get_window(self));
}


/*!
 * Returns the visibility state of the given \ref DebugWindow.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 * \return Returns TRUE, if the debug window is visible. FALSE otherwise.
 */
gboolean debug_window_is_visible(DebugWindow * self)
{
  return self && GTK_WIDGET_VISIBLE(debug_window_get_window(self));
}


/*!
 * Fills the tree store with the evaluation stack contents (program and local
 * relations).
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param store The GtkTreeStore to fill.
 * \param L Current evaluation state.
 */
static void _tree_create_gtk_tree_model_rec(GtkTreeStore * store, lua_State * L)
{
	lua_Debug ar;
	int levels, l, outermostLevel;
	GtkTreeIter par_iter = {0};
    GtkTreeIter iter, iter_childs;

	/* 0 is the topmost function. After this loop the outermost call is at the
	 * beginning of the "list". */
	for (levels = 0 ; lua_getstack(L, levels, &ar) ; levels++) { ; }

	/* The outermost call is always a tail call. That is, because Kure wraps
	 * the term to evaluate into a return-statement. Thus, we have to ignore
	 * the that level. */
	outermostLevel = levels-2;
	for (l = outermostLevel ; l >= 0 && lua_getstack(L, l, &ar) ; --l) {
		const char * varname;
		int j;

		if ( !lua_getinfo (L, "nSl", &ar)) {
			g_warning ("Unable to getinfo for current stack level %d.", l);
			gtk_tree_store_append(store, &iter, (l < outermostLevel) ? &par_iter : NULL);
			gtk_tree_store_set(store, &iter,
					0, "(Unknown)", /* the program name */
					1, TYPE_LUA_FUN,
					2, NULL,
					-1 /*end*/);
		}
		else {

			/* Create a node for the function/program itself. */
			{
				gchar * s = _get_lua_fun_name(&ar);

				/* Use NULL for the parent at the root level. */
				gtk_tree_store_append(store, &iter, (l < outermostLevel) ? &par_iter : NULL);
				gtk_tree_store_set(store, &iter,
						0, s, /* the program name */
						1, TYPE_LUA_FUN,
						2, NULL,
						-1 /*end*/);

				g_free(s);
			}

			/* Add the local relations as the node childrens to the tree store. */
			for (j = 1 ; (varname = lua_getlocal(L, &ar, j)) ; ++j)
			{
				/* Ignore temporary variables. Lua uses parentheses to distinguish
				 * those from normal variables. */
				if (varname[0] == '(') { /* Ignored */ }
				else {
					gpointer ptr = NULL;
					DebugWindowNodeType type;

					if (kure_lua_isrel(L, -1)) {
						/* Note: We have to use the original KureRel object here. That
						 *       is because the user must have to chance to change the
						 *       relations displayed to him. On the other side, because
						 *       the Rel object are just adapters we have to be
						 *       careful when we destroy them. */
						KureRel * impl = kure_lua_torel(L,-1,NULL);
						ptr = rel_new_from_impl(varname, impl);
						type = TYPE_REL;
					}
					else if (kure_lua_isdom(L,-1)) {
						ptr = kure_lua_todom (L,-1,NULL);
						type = TYPE_DOM;
					}
					else if (lua_isnoneornil(L,-1)) {
						ptr = NULL;
						type = TYPE_NIL;
					}
					else {
						ptr = NULL;
						type = TYPE_UNKNOWN;

						/* One example for such a variable is the auxiliary line number
						 * which is used inside Kure. Its name is __line. */
					}

					if (type != TYPE_UNKNOWN) {
						gtk_tree_store_append(store, &iter_childs, &iter);
						gtk_tree_store_set(store, &iter_childs,
								0, varname,
								1, type,
								2, ptr,
								-1 /*EOL*/);
					}
				}

				lua_pop(L,1);
			}
		}

		par_iter = iter;
	}
}


/*!
 * Update the tree view according to the current evaluation state of the
 * \ref DebugWindow.
 *
 * \todo Currently the tree view will loose its selection, when this function
 *       is called.
 *
 * \author stb
 * \date 05.08.2008
 *
 * \param self The \ref DebugWindow.
 */
void _debug_window_update_state(DebugWindow * self)
{
	GtkTreeSelection * treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->treeview));
	GtkTreePath * path = NULL;

	/* Lookup a current selected relation. We will reselect it after the
	 * update, if one is selected. */
	{
		GtkTreeIter iter;
		GtkTreeModel * model;

		if (gtk_tree_selection_get_selected(treesel, &model, &iter)) {
			path = gtk_tree_model_get_path(model, &iter);
		}
		else { /* no selection */ }
	}


	/* Clear the treeview's contents and recreate the rows from the updated
	 * state. This will remove all selections in the treeview. */
	_debug_window_clear_stack_display(self);

	if (self->L) {
		_tree_create_gtk_tree_model_rec(self->treestore, self->L);
		gtk_tree_view_expand_all(GTK_TREE_VIEW(self->treeview));
	}

	/* Reset the selection, if a relation was selected before the update.
	 * If the relation is no longer in the treeview, this results in an
	 * empty selection. */
	if (path != NULL) {
		gtk_tree_selection_select_path(treesel, path);
	}
}



/*******************************************************************************
 *                            DebugWindow Singleton                            *
 *                              Fri, 11 Feb 2011                               *
 ******************************************************************************/

DebugWindow * g_debugWindow = NULL;


/*!
 * Create and/or return the global DebugWindow instance. The singleton
 * replaces the global variable debug and all other associated global
 * variables. They are available through the DebugWindow object now.
 *
 * \author stb
 * \author 31.07.2008
 * \return Returns a DebugWindow instance.
 */
DebugWindow * debug_window_get_instance ()
{
	if ( !g_debugWindow)
		g_debugWindow = debug_window_new ();

	return g_debugWindow;
}


/*! Destroys the global DebugWindow instance, if there is one.
 *
 * \author stb
 * \author 31.07.2008
 */
void debug_window_destroy_instance ()
{
  if (g_debugWindow) {
    debug_window_destroy (g_debugWindow);
    g_debugWindow = NULL;
  }
}
