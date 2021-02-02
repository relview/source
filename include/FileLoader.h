/*
 * FileLoader.h
 *
 *  Created on: Jun 26, 2009
 *      Author: stefan
 */

#ifndef FILELOADER_H_
#define FILELOADER_H_

#include <glib.h>
#include "RelviewTypes.h"
#include "IOHandler.h"

typedef struct _FileLoader FileLoader;


FileLoader * 	file_loader_new (Relview * rv);
FileLoader * 	file_loader_new_with_handler (Relview * rv, IOHandler * handler);
void 			file_loader_destroy (FileLoader * self);

RvReplacePolicy file_loader_get_replace_policy (FileLoader * self);
void 			file_loader_set_replace_policy (FileLoader * self, RvReplacePolicy policy);
IOHandler *		file_loader_get_handler (FileLoader * self);
void			file_loader_set_handler (FileLoader * self, IOHandler * handler);

/*!
 * Returns the current action. The default action is IO_HANDLER_ACTION_LOAD.
 */
IOHandlerActions file_loader_get_action (FileLoader * self);

/*!
 * Can be used to replace the default action IO_HANDLER_ACTION_LOAD. Only.
 * one load action is allowed. Otherwise the action actually used is undefined.
 */
void			file_loader_set_action (FileLoader * self, IOHandlerActions action);

/*!
 * If a handler was set, this handler is used. Otherwise the suffix is used
 * to guess an appropriate handler from the registered ones. Returns FALSE,
 * if no such handler was found. If FALSE is returned, perr is set.
 */
gboolean 		file_loader_load_file (FileLoader * self, const gchar * fname, GError ** perr);

/*!
 * If no handler was set, TRUE is returned, if there is a handler for the
 * filename's extension. If a handler was set, TRUE is _always_ returned!
 */
gboolean 		file_loader_can_load (FileLoader * self, const gchar * fname);


/* ------------------------------------------------------------------------- */

typedef struct _DefaultReplacePolicyHandler DefaultReplacePolicyHandler;

DefaultReplacePolicyHandler * default_replace_policy_handler_new (Relview * rv);
void default_replace_policy_handler_destroy (DefaultReplacePolicyHandler * self);
void default_replace_policy_handler_set_parent_callback (DefaultReplacePolicyHandler * self,
		IOHandler_ReplaceCallback,gpointer user_data);
IOHandler_ReplaceCallback default_replace_policy_handler_get_callback
	(DefaultReplacePolicyHandler * self);

#endif /* FILELOADER_H_ */
