/*
 * file_ops.c
 *
 *  Copyright (C) 1995-2011 Peter Schneider, Werner Lehmann, Stefan Bolus,
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
#include "Function.h"
#include "Graph.h"
#include "Domain.h"
#include "Program.h"
#include "label.h"
#include "msg_boxes.h"
#include "LabelWindow.h"
#include "DirWindow.h"
#include "XddFile.h"
#include "Relview.h"
#include "file_ops.h" // includes IOHandler.h
#include "FileLoader.h" // DefaultReplacePolicyHandler


/****************************************************************************/
/* NAME: load_prog_file                                                     */
/* FUNKTION: laedt ein ausgewaehltes file                                   */
/* UEBERGABEPARAMETER: KEINER                                               */
/* RUECKGABEWERT: KEINER                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 27.03.95                                                             */
/* LETZTE AENDERUNG AM: 27.03.95                                            */
/****************************************************************************/

typedef struct {
	const char * filename;

	Relview * rv;
	//IOHandler_ReplaceCallback replace_callback;
	//gpointer user_data;

	FunManager * funs;
	ProgManager * progs;

	GError * err;
} ProgFile;

Kure_bool _on_program (void * object, const char * code, const char * lua_code)
{
	ProgFile * info = (ProgFile*) object;
	if ( !info->err) {
		Prog * f = prog_new_with_lua(code, lua_code, &info->err);
		if (f) {
			const gchar * name = prog_get_name(f);

			if (prog_manager_exists(info->progs, name)
					|| fun_manager_exists(info->funs, name)) {
				printf ("%s: Program/file with name \"%s\" already exists. "
						"Skipped!\n", info->filename, name);
			}
			else prog_manager_insert(info->progs, f);
		}
	}

	return TRUE; //go on
}

Kure_bool _on_function (void * object, const char * code, const char * lua_code)
{
	ProgFile * info = (ProgFile*) object;
	if ( !info->err) {
		Fun * f = fun_new_with_lua(code, lua_code, &info->err);
		if (f) {
			const gchar * name = fun_get_name(f);

			if (prog_manager_exists(info->progs, name)
					|| fun_manager_exists(info->funs, name)) {
				printf ("%s: Program/file with name \"%s\" already exists. "
						"Skipped!\n", info->filename, name);
			}
			else fun_manager_insert(info->funs, f);
		}
	}

	return TRUE; //go on
}

static gboolean prog_load_file(Relview * rv, const gchar * path_file,
		IOHandler_ReplaceCallback replace_callback, gpointer user_data,
		GError ** perr)
{
	KureError * kerr = NULL;
	KureParserObserver o = {0};
	ProgFile file_info = {0};
	Kure_success success;

	file_info.err = NULL; /* non-NULL indicates an error condition. */
	file_info.filename = path_file;
	file_info.funs = fun_manager_new ();
	file_info.progs = prog_manager_new ();
	file_info.rv = rv;

	o.object = &file_info;
	o.onFunction = _on_function;
	o.onProgram = _on_program;

	success = kure_lang_parse_file (path_file, &o, &kerr);
	if (! success) {
		g_set_error_literal(perr, rv_error_domain(), 0, kerr->message);
		kure_error_destroy(kerr);

		if (file_info.err) // ignore
			g_error_free(file_info.err);
	}
	else if (file_info.err) {
		if (perr) *perr = file_info.err;
		success = FALSE;
	}
	else {
		FunManager * funs = fun_manager_get_instance (),
					*local_funs = file_info.funs;
		ProgManager * progs = prog_manager_get_instance (),
					*local_progs = file_info.progs;

		/* Now merge the local objects from the file into the global context.
		 * Remark that functions and programs share the same name space and thus
		 * there must be a no program and function of same name. */

		FOREACH_PROG(local_progs, cur, iter, {
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.name = prog_get_name(cur);

			if (prog_manager_exists(progs, info.name)) {
				info.type = RV_OBJECT_TYPE_PROGRAM;

				action = replace_callback(user_data, &info);
				g_assert (action != RV_REPLACE_ACTION_ASK_USER);

				switch (action) {
				case RV_REPLACE_ACTION_REPLACE:
					prog_manager_delete_by_name (progs, info.name);
					break;
				case RV_REPLACE_ACTION_KEEP: break;
				default:
					g_warning ("action is %d\n", action);
					g_assert_not_reached();
				}
			}
			else {
				if (fun_manager_exists(funs, info.name)) {
					info.type = RV_OBJECT_TYPE_FUNCTION;

					action = replace_callback(user_data, &info);

					switch (action) {
					case RV_REPLACE_ACTION_REPLACE:
						fun_manager_delete_by_name (funs, info.name);
						break;
					case RV_REPLACE_ACTION_KEEP: break;
					default:
						g_warning ("action is %d\n", action);
						g_assert_not_reached();
					}
				}
			}
		}); /* foreach program */
		prog_manager_steal_all (progs, local_progs);

		FOREACH_FUN(local_funs,cur,iter,{
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.name = fun_get_name(cur);

			if (prog_manager_exists(progs, info.name)) {
				info.type = RV_OBJECT_TYPE_PROGRAM;

				action = replace_callback(user_data, &info);
				g_assert (action != RV_REPLACE_ACTION_ASK_USER);

				switch (action) {
				case RV_REPLACE_ACTION_REPLACE:
					prog_manager_delete_by_name (progs, info.name);
					break;
				case RV_REPLACE_ACTION_KEEP: break;
				default: g_assert_not_reached();
				}
			}
			else {
				/* Replace an existing function for a new function of same name? */
				if (fun_manager_exists(funs, info.name)) {
					info.type = RV_OBJECT_TYPE_FUNCTION;

					action = replace_callback(user_data, &info);

					switch (action) {
					case RV_REPLACE_ACTION_REPLACE:
						fun_manager_delete_by_name (funs, info.name);
						break;
					case RV_REPLACE_ACTION_KEEP: break;
					default:
						g_warning ("action is %d\n", action);
						g_assert_not_reached();
					}
				}
			}
		}); /* foreach function */
		fun_manager_steal_all (funs, local_funs);
	}

	fun_manager_destroy(file_info.funs);
	prog_manager_destroy(file_info.progs);

	return success;
}


static gboolean _prog_load_file (IOHandler * handler, const gchar * filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	gboolean ret;
	Relview * rv = rv_get_instance ();
	DefaultReplacePolicyHandler * policy_handler = default_replace_policy_handler_new (rv);
	default_replace_policy_handler_set_parent_callback (policy_handler, replace, user_data);

	ret = prog_load_file (rv, filename, default_replace_policy_handler_get_callback(policy_handler),
			policy_handler, perr);
	default_replace_policy_handler_destroy (policy_handler);
	return ret;
}

/*!
 * Create a \ref IOHandler for .prog files.
 */
IOHandler * prog_get_handler ()
{
	IOHandler * ret = g_new0 (IOHandler,1);
	static IOHandlerClass * c = NULL;
	if (!c) {
		c = g_new0 (IOHandlerClass,1);

		c->load = _prog_load_file;

		c->name = g_strdup ("Programs/Functions");
		c->extensions = g_strdup ("prog");
		c->description = g_strdup ("Stores programs and functions in RelView's "
				"programming language.");
	}
	ret->c = c;
	return ret;
}


/*!
 * Parse the given file and return the labels if no error occured.
 * Defined in labelparser.y.
 */
extern GList/*<Label*>*/ * parse_label_file (const gchar * filename, GError ** perr);

/****************************************************************************/
/* NAME: load_label_file                                                    */
/* FUNKTION: laedt ein ausgewaehltes file                                   */
/* UEBERGABEPARAMETER: KEINER                                               */
/* RUECKGABEWERT: KEINER                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 27.03.95                                                             */
/* LETZTE AENDERUNG AM: 04.07.2000 (Fehler beim Einlesen korrigiert)        */
/****************************************************************************/
static gboolean label_load_file(Relview * rv, const gchar * path_file,
		IOHandler_ReplaceCallback replace_callback, gpointer user_data,
		GError ** perr)
{
	GList/*<Label*>*/ * labels = parse_label_file (path_file, perr);

	if (NULL == labels) {
		return FALSE;
	}
	else {
		GList/*<Label*>*/ * iter = labels;

		while (iter) {
			Label * label = (Label*)iter->data;
			const gchar * name = label_get_name (label);
			gboolean really_insert = FALSE;

			if (rv_label_exists (rv, name)) {
				RvReplaceAction action;
				IOHandlerReplaceInfo info = {0};
				info.name = name;
				info.type = RV_OBJECT_TYPE_LABEL;

				action = replace_callback(user_data, &info);

				switch (action) {
				case RV_REPLACE_ACTION_REPLACE:
					rv_label_remove_by_name (rv, name);
					really_insert = TRUE;
					break;
				case RV_REPLACE_ACTION_KEEP: break;
				default: g_warning ("prog_load_file: Unknown action: %d\n", action);
				}
			}
			else really_insert = TRUE;

			if (really_insert) {
				/* Manually remove the element and insert it into the global
				 * list. */

				GList * save = iter;
				iter = iter->next;

				rv_label_add (rv, (Label*)save->data);
				labels = g_list_delete_link (labels, save);
			}
			else {
				iter = iter->next;
			}
		} /* foreach label */

		g_list_foreach (labels, (GFunc)label_destroy, NULL);
		g_list_free (labels);
	}

	label_window_update(label_window_get_instance());
	return TRUE;
}


static gboolean _label_load_file (IOHandler * handler, const gchar * filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	gboolean ret;
	Relview * rv = rv_get_instance ();
	DefaultReplacePolicyHandler * policy_handler = default_replace_policy_handler_new (rv);
	default_replace_policy_handler_set_parent_callback (policy_handler, replace, user_data);

	ret = label_load_file (rv, filename, default_replace_policy_handler_get_callback(policy_handler),
			policy_handler, perr);
	default_replace_policy_handler_destroy (policy_handler);
	return ret;
}


/*!
 * Create a \ref IOHandler for .label files.
 */
IOHandler * label_get_handler ()
{
	IOHandler * ret = g_new0 (IOHandler,1);
	static IOHandlerClass * c = NULL;
	if (!c) {
		c = g_new0 (IOHandlerClass,1);

		c->load = _label_load_file;

		c->name = g_strdup ("Labels");
		c->extensions = g_strdup ("label");
		c->description = g_strdup ("Stores labels for relations and graphs.");
	}
	ret->c = c;
	return ret;
}
