/*
 * Relview.h
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

#ifndef RELVIEW_H
#  define RELVIEW_H

#include "RelviewTypes.h"

#include <gtk/gtk.h>
#include "IOHandler.h"
#include "Observer.h"
#include "Menu.h"
#if !defined IN_RV_PLUGIN
#include "Workspace.h"
#endif
#include "Namespace.h"
#include "Domain.h"
#include "Function.h"
#include "Program.h"
#include "label.h"

/*!
 * Associates a \ref Rel and a viewport, e.g. a \ref RelationViewport,
 * with a pair of \ref Label objects.
 */
typedef struct _LabelAssoc
{
	Rel * rel;
	gpointer viewable;
	Label * labels[2]; /*0:row, 1:col*/
} LabelAssoc;

typedef enum _Verbosity
{
	/* the higher, the less important */
	VERBOSE_QUIET = 0,
	VERBOSE_WARN = 1,
	VERBOSE_PROGRESS = 2,
	VERBOSE_INFO = 3,
	VERBOSE_DEBUG = 4,
	VERBOSE_TRACE = 5,
	VERBOSE_ALL
} Verbosity;

#ifdef VERBOSE
# undef VERBOSE
#endif
# define VERBOSE(level,cmd) if(g_verbosity>=level) { cmd; }

void verbose_printf(Verbosity level, const char * fmt, ...);

/* Defined in Relview.c */
extern Verbosity g_verbosity;


gboolean        rv_ask_rel_name         (const GString * caption,
                                         const GString * descr,
                                         GString * /*inout*/ name,
                                         gboolean defIsEmpty);
gboolean        rv_ask_rel_name_def     (gchar ** pname, gboolean defIsEmpty);

#if !defined IN_RV_PLUGIN
void            rv_init (int argc, char ** argv);
GtkBuilder * 	rv_get_gtk_builder (Relview * self);
gint 			rv_get_verbosity (Relview * self);
void 			rv_set_verbosity (Relview * self, gint level);
Workspace * 	rv_get_workspace (Relview * self);
GQuark 			rv_error_domain ();
#endif

Relview * 		rv_get_instance ();
gchar *         rv_find_data_file (const gchar * fname);
gchar * 		rv_find_startup_file (const gchar * fname);
const GSList * 	rv_get_default_plugin_dirs (Relview * self);

KureContext * 	rv_get_context (Relview * self);
Namespace * 	rv_get_namespace (Relview * self);

RelManager * 	rv_get_rel_manager (Relview * self);
DomManager * 	rv_get_dom_manager (Relview * self);
FunManager * 	rv_get_fun_manager (Relview * self);
ProgManager * 	rv_get_prog_manager (Relview * self);

/*!
 * Register a \ref IOHandler to the system. The ownership of the IOHandler
 * object is transfered to the \ref Relview object. If the IOHandler is already
 * registered, nothing happens.
 */
void rv_register_io_handler (Relview * self, IOHandler * handler);


GList/*<IOHandler*>*/ * rv_get_io_handlers (Relview * self);

/*!
 * Unregister the given \ref IOHandler. If the given handler is not registered
 * nothing happens.
 *
 * \remark The object is destroyed during the call if and only if the handler
 *         was registered.
 *
 * \return Returns TRUE if the handler was unregistered.
 */
gboolean rv_unregister_io_handler (Relview * self, IOHandler * handler);


/*!
 * The list should be freed by g_list_free. Only a single action may be passed.
 * Returns NULL in case of no matching \ref IOHandler.
 */
GList/*<IOHandler*>*/ * rv_get_io_handler_for_action (Relview * self, IOHandlerActions a);

/*!
 * Similar to \ref rv_get_io_handler_for_action. But returns all \ref IOHandler
 */
GList/*<IOHandler*>*/ * rv_get_io_handler_for_any_action (Relview * self, IOHandlerActions a);

MenuManager * rv_get_menu_manager (Relview * self);

/*!
 * Returns a registered \ref IOHandler object, which is capable to handle the
 * given filename (by extension) with the given action. Returns NULL, if no
 * such \ref IOHandler object could have been found. If there is more than one
 * candidate, the most recently added handler is returned.
 */
IOHandler * rv_get_io_handler_for_file_and_action(Relview * self,
		const gchar * filename, IOHandlerActions action);


void rv_register_observer (Relview * self, RelviewObserver * o);
void rv_unregister_observer (Relview * self, RelviewObserver * o);


void rv_user_error_markup (const gchar * title, const gchar * text, ...);
void rv_user_error (const gchar * title, const gchar * text, ...);

/*!
 * Similar to \ref rv_user_long_error, but it uses a scrollable text box which
 * allows to display a more comprehensive description of the error.
 */
void rv_user_error_with_descr (const char * title, const gchar * descr,	const gchar * text, ...);

void rv_user_message (const gchar * title, const gchar * text, ...);


/* Returned string must be freed using g_free. Returns NULL in case of
 * an error. Is a non-NULL value is returned, it is a valid, but maybe
 * already used, relation name. */
//gchar * rv_user_ask_rel_name (const gchar * title);


/*!
 * Asks the user to enter information on a relation which shall be created.
 *
 * \param rv The current \ref Relview object.
 * \param def_name Default name; may be NULL.
 * \param cols The number of columns. Must be initialized. If it is different
 *             from, it will be used as the default number of cols.
 * \param rows Similar to cols, but for the rows.
 * \return Returns non-NULL if the user entered valid data, i.e. strictly
 *         positive values for 'cols' and 'rows' and he entered a valid, but
 *         not necessarily unique relation name.
 */
gchar * rv_user_new_relation (Relview * rv, const gchar * def_name,
			mpz_t cols, mpz_t rows);


gboolean rv_user_rename_or_not (Relview * self, Rel * rel);


/*!
 * Allow the user to edit a domain in a dialog. No string must be NULL.
 * All strings have to be initialized. Returns TRUE if the user pressed
 * apply and returns FALSE otherwise. The string may be changed regardless
 * of the return value, i.e. they may have changed if FALSE is returned.
 */
gboolean rv_user_edit_domain (Relview * rv, GString * name,
		DomainType *ptype, GString * comp1, GString * comp2);


/*!
 * Allow the user to edge a function or enter a new one. 'def' must not be
 * NULL and is used as the default function definition. Thus, if no such one
 * is available, 'def' should be NULL. The function is checked for parseability
 * using fun_new_from_def. Nonetheless, you should check for correctness on
 * your own before you use the definition. 'def' must be initialized.
 */
gboolean rv_user_edit_function (Relview * rv, GString * def);


RvReplacePolicy rv_user_ask_replace_object (Relview * rv, RvObjectType type,
		const gchar * name, const gchar * descr, gboolean more);

RvReplacePolicy rv_user_ask_replace_object2 (Relview * rv, RvObject * obj,
		const gchar * descr, gboolean more);

gboolean rv_user_ask_remove_object (Relview * rv, RvObjectType type,
		const gchar * name, const gchar * descr);

/*!
 * Request a random fill probability from the user. The returned value is in
 * [0,1].
 *
 * \author stb
 * \date 17.04.2007
 * \param pvalue If the user pressed "OK" the selected value will be
 *               stored in return.
 * \return Returns TRUE, if the user pressed "OK", FALSE otherwise.
 */
gboolean rv_user_get_random_fill_percentage (Relview * rv, gfloat * /*out*/ pvalue);


/*!
 * Formats a large integer.
 */
gchar * rv_user_format_large_int (mpz_t ents);


/*!
 * Returns a newly allocated string which contains the relations's dimension
 * in the format "<rows> x <cols>".
 */
gchar * rv_user_format_impl_dim (KureRel * rel);


/*!
 * Shows a dialog to export a object using the given action, title for the
 * file chooser, the object and so on.
 */
gboolean rv_user_export_object (IOHandlerActions action,
						   	  const gchar * title,
						   	  gpointer obj,
						   	  gchar* (*default_filename)(gpointer) /* due to "$" to "dollar" */,
						   	  gboolean (*export) (IOHandler*,const gchar*,gpointer,GError**));


/*!
 * Similar to \ref rv_user_export_object . Shows a dialog for the given action
 * to load an object.
 */
gboolean rv_user_import_object (IOHandlerActions action,
						   	  const gchar * title,
						   	  gboolean (*import) (IOHandler*,const gchar*,GError**));


/*!
 * Let the user select a label. Returns true only if the user clicked the OK
 * button. If labelName is "", no label is preselected. If in the output
 * labelName is "", no label was selected by the user. If FALSE is returned,
 * labelName should be ignored. The variable labelName has to be initialized
 * in either case.
 */
gboolean rv_user_ask_label (Relview * rv, GString * /*input*/ labelName);


/*!
 * Create a new \ref lua_State for computation inside RelView. This operation
 * does the following things.
 *    - It creates a \ref lua_State L with \ref kure_lua_new .
 *    - _Each_ function and program inside RelView is loaded into L.
 *    - _Each_ relation and domain inside RelView is loaded into L via copy.
 *
 * The returned state can be destroyed using \ref lua_close. Remind that
 * RelView's \ref KureContext is references by the state and thus, you may
 * not call \ref kure_context_detroy. \ref rv_destroy would be, as well as,
 * \ref kure_context_deref okay.
 *
 * \note Once the new state is returned, changes to objects inside RelView
 *       don't have any effect on the state.
 *
 * \attention Remark that "$" is not a valid identifier in Lua. To overcome
 *            that, "$" is mapped to the special name KURE_DOLLAR_SUBST. Thus,
 *            when programming in Lua directly you have to use this special
 *            name instead of "$". Occurrences of "$" inside Kure's own
 *            language are covered automatically. \ref kure_lang_set_rel_copy
 *            and \ref kure_lang_get_rel also do the replace for use.
 */
lua_State * rv_lang_new_state (Relview * rv);


/*!
 * Evaluate the given expression an return the resulting relation. Returns NULL
 * on error and sets *perr if perr is non-NULL.
 *
 * Expressions are, e.g., "x^", "[k,j]*-t", etc. Any global object can be used.
 * Evaluation of Lua code is not supported by this function.
 *
 * Because the function creates a new \ref lua_State with each call and
 * destroys it directly after use, it can be quiet slow depending on the number
 * of global objects.
 *
 * \see rv_lang_new_state
 */
KureRel * rv_lang_eval (Relview * self, const char * expr, GError ** perr);


/*******************************************************************************
 *                                   Labels                                    *
 *                     (Labels for Relations and Graphs.)                      *
 *                                                                             *
 *                               Wed, 9 Feb 2011                               *
 ******************************************************************************/


/*!
 * Returns the label with the given name if one exists. Returns NULL otherwise.
 * Label names are compared case-sensitively.
 */
Label * rv_label_get_by_name (Relview * self, const gchar * name);


/*!
 * Return NULL != rv_label_get_by_name(self,name)
 */
gboolean rv_label_exists (Relview * self, const gchar * name);


/*!
 * Returns true if the labels inserted and false otherwise. The label is not
 * inserted if another label with the given names already exists.
 *
 * Once a label was added to the \ref Relview object it is not allowed to
 * destroy it. The label is solely owned by the Relview object.
 */
gboolean rv_label_add (Relview * self, Label * label);


/*!
 * Removes and destroys the label with the given name if it is non-persistent.
 */
void rv_label_remove_by_name (Relview * self, const gchar * name);


/*!
 * Returns a newly allocated list containing all registered labels. The list
 * has to be freed with \ref g_list_free.
 */
GList/*<Label*>*/ * rv_label_list (Relview * self);


/*!
 * Removes all non-persistent labels.
 */
void rv_label_clear (Relview * self);


/*******************************************************************************
 *                              Label Association                              *
 *              (Associate a label to a Relation on a viewable.)               *
 *                                                                             *
 *                              Mon, 14 Feb 2011                               *
 ******************************************************************************/


/*!
 * Returns the associated labels for the given relation and viewable if there
 * is an association and NULL otherwise.
 */
LabelAssoc rv_label_assoc_get (Relview * self, Rel * rel, gpointer viewable);

void rv_label_assoc_set (Relview * self, const LabelAssoc * assoc);

//void rv_label_assoc_remove (Relview * self, Rel * rel, gpointer viewable);

#endif /* Relview.h */
