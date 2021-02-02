/*
 * IOHandler.h
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

#ifndef IOHANDLER_H_
#define IOHANDLER_H_

#include <glib.h>
#include "Graph.h"
#include "Relation.h"
#include "RelviewTypes.h"

#define IO_HANDLER(obj) ((IOHandler*)obj)

// TODO: Overwrite policy?

typedef struct _IOHandler IOHandler;

typedef enum _IOHandlerActions {
	IO_HANDLER_ACTION_LOAD = 0x1,
	IO_HANDLER_ACTION_SAVE = 0x2,
	IO_HANDLER_ACTION_LOAD_RELS = 0x4,
	IO_HANDLER_ACTION_LOAD_GRAPHS = 0x8,
	IO_HANDLER_ACTION_SAVE_SINGLE_REL = 0x10,
	IO_HANDLER_ACTION_SAVE_SINGLE_GRAPH = 0x20,
} IOHandlerActions;

typedef struct _IOHandlerReplaceInfo
{
	RvObjectType type;
	const gchar * name;

	/* Alternatively (mutual exclusive), a RvObject can be passed which
	 * is used if non-NULL. */
	RvObject * obj;
} IOHandlerReplaceInfo;

/*!
 * Returns TRUE if the given object shall be overwritten. The IOHandler
 */
typedef RvReplaceAction (*IOHandler_ReplaceCallback)
	(gpointer /*user_data*/, const IOHandlerReplaceInfo*);

typedef struct _IOHandlerClass
{
	/* For the load operations, the 'replace callback' may be NULL. In this
	 * case, IO_HANDLER_REPLACE_POLICY_ASK_USER should be used for each
	 * object. */

	/* Relations */
	gboolean (*loadRels) (IOHandler*, const gchar* /*filename*/,
			IOHandler_ReplaceCallback, gpointer /*user_data*/, GError**);
	gboolean (*saveSingleRel) (IOHandler*, const gchar * filename, Rel * rel, GError ** perr);

	/* Graphs */
	gboolean (*loadGraphs) (IOHandler*, const gchar* /*filename*/,
			IOHandler_ReplaceCallback, gpointer /*user_data*/, GError**);
	gboolean (*saveSingleGraph) (IOHandler*, const gchar * filename, XGraph * gr, GError ** perr);

	/* Unspecified, e.g. XDD */
	gboolean (*load) (IOHandler*, const gchar * filename,
			IOHandler_ReplaceCallback, gpointer /*user_data*/, GError ** perr);
	gboolean (*save) (IOHandler*, const gchar * filename, GError ** perr);

	void (*destroy) (IOHandler*); /*!< Must not be NULL. */

	gchar * extensions; /*!< Comma separated list of extensions the object can
						 *   handle. E.g. xdd,ascii, if your object can handle
						 *   files with a "xdd" or "ascii" suffix. Comparisons
						 *   are case insensitive. Must not be NULL and must
						 *   contain NO spaces or tabs! */

	gchar * name; /*!< Must not be NULL and should not be empty. The name is
				   *   displayed to the user. */

	gchar * description; /*!< Should not be NULL and should not be empty. The
	                      *   description is displayed to the user. */
} IOHandlerClass;


/*abstract*/ struct _IOHandler
{
	IOHandlerClass * c; /*!< Must be the first argument. */
};


void     io_handler_destroy (IOHandler * self);

gboolean io_handler_load_rels (IOHandler * self, const gchar * filename, IOHandler_ReplaceCallback, gpointer /*user_data*/, GError ** perr);
gboolean io_handler_load_graphs (IOHandler * self, const gchar * filename, IOHandler_ReplaceCallback, gpointer /*user_data*/, GError ** perr);
gboolean io_handler_load (IOHandler * self, const gchar * filename, IOHandler_ReplaceCallback, gpointer /*user_data*/, GError ** perr);

/*!
 * Calls one of the functions \ref io_handler_load_rels,
 * \ref io_handler_load_graphs and \ref io_handler_load depending on the given
 * action. Only one action must be given. Otherwise a random function is called.
 */
gboolean io_handler_load_by_action (IOHandler * self, IOHandlerActions action,
		const gchar * filename, IOHandler_ReplaceCallback, gpointer /*user_data*/, GError ** perr);

gboolean io_handler_save_single_rel (IOHandler * self, const gchar * filename, Rel * rel, GError ** perr);
gboolean io_handler_save_single_graph (IOHandler * self, const gchar * filename, XGraph * gr, GError ** perr);
gboolean io_handler_save (IOHandler * self, const gchar * filename, GError ** perr);

const gchar * io_handler_get_name (IOHandler * self);
const gchar * io_handler_get_description (IOHandler * self);
const gchar * io_handler_get_extensions (IOHandler * self);

IOHandlerActions io_handler_get_supported_actions (IOHandler * self);
gboolean io_handler_supports_all_actions (IOHandler * self, IOHandlerActions a);
gboolean io_handler_supports_any_action (IOHandler * self, IOHandlerActions a);
gboolean io_handler_supports_action (IOHandler * self, IOHandlerActions a);

/*!
 * Returns TRUE, if the given \ref IOHandler is responsible for the given file.
 */
gboolean io_handler_matches (IOHandler * self, const gchar * filename);

/*!
 * Returns any extension supported by the given \ref IOHandler. Never returns
 * NULL. The result must be freed with g_free.
 */
gchar * io_handler_get_any_extension (IOHandler * self);

/*!
 * Returns the supported extensions as a list. The user has to free the list,
 * as well as the contained strings after use. The list is never empty.
 */
GList/*<gchar*>*/ * io_handler_get_extensions_as_list (IOHandler * self);

#endif /* IOHANDLER_H_ */
