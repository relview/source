/*
 * FileLoader.c
 *
 *  Created on: Jun 26, 2009
 *      Author: stefan
 */

#include "FileLoader.h"
#include "Relview.h"

struct _FileLoader
{
	Relview * rv;
	IOHandler * handler;
    RvReplacePolicy policy;
    IOHandlerActions action;
};

FileLoader * file_loader_new (Relview * rv)
{
    FileLoader * self = g_new0 (FileLoader, 1);
    self->rv = rv;
    self->policy = RV_REPLACE_POLICY_ASK_USER;
    self->action = IO_HANDLER_ACTION_LOAD;
    return self;
}

FileLoader * file_loader_with_handler (Relview * rv, IOHandler * handler)
{
    FileLoader * self = file_loader_new (rv);
    self->handler = handler;
    return self;
}

void file_loader_destroy (FileLoader * self) { g_free (self); }

RvReplacePolicy file_loader_get_replace_policy (FileLoader * self) { return self->policy; }
void file_loader_set_replace_policy (FileLoader * self, RvReplacePolicy policy)
{ self->policy = policy; }

IOHandler *		file_loader_get_handler (FileLoader * self) { return self->handler; }
void			file_loader_set_handler (FileLoader * self, IOHandler * handler)
{ self->handler = handler; }

IOHandlerActions file_loader_get_action (FileLoader * self) { return self->action; }
void			file_loader_set_action (FileLoader * self, IOHandlerActions action)
{ self->action = action; }


static RvReplaceAction _replace_callback (gpointer user_data,
		const IOHandlerReplaceInfo * info)
{
	return RV_REPLACE_POLICY_ASK_USER;
}

gboolean file_loader_load_file (FileLoader * self, const gchar * fname, GError ** perr)
{
	IOHandler * handler = self->handler;

	if (! handler) {
		handler = rv_get_io_handler_for_file_and_action (self->rv, fname, self->action);
		if (! handler) {
			g_set_error (perr, 0,0, "No handler for file \"%s\".", fname);
			return FALSE;
		}
	}

	return io_handler_load_by_action (handler, self->action, fname,
				_replace_callback, self, perr);
}

/* ------------------------------------------------------------------------- */

struct _DefaultReplacePolicyHandler
{
	Relview * rv;
	RvReplacePolicy policy;

	IOHandler_ReplaceCallback parent_callback;
	gpointer parent_user_data;
};

DefaultReplacePolicyHandler * default_replace_policy_handler_new (Relview * rv)
{
	DefaultReplacePolicyHandler * self = g_new0 (DefaultReplacePolicyHandler,1);
	self->rv = rv;
	self->policy = RV_REPLACE_POLICY_ASK_USER;
	return self;
}

void default_replace_policy_handler_destroy (DefaultReplacePolicyHandler * self)
{ g_free (self); }

RvReplaceAction _default_replace_policy_handler_callback (gpointer user_data,
		const IOHandlerReplaceInfo * info)
{
	DefaultReplacePolicyHandler * self = (DefaultReplacePolicyHandler*) user_data;
	RvReplaceAction action = RV_REPLACE_POLICY_ASK_USER;

	/* If there is a parent callback, first ask it what to do. */
	if (self->parent_callback)
		action = self->parent_callback (self->parent_user_data, info);

	/* The parent callback said we have to ask the user or use our own
	 * policy. */
	if (action == RV_REPLACE_POLICY_ASK_USER)
	{
		if (self->policy == RV_REPLACE_POLICY_REPLACE_ALL)
			action =  RV_REPLACE_ACTION_REPLACE;
		else if (self->policy == RV_REPLACE_POLICY_KEEP_ALL)
			action = RV_REPLACE_POLICY_KEEP;
		else {
			RvReplacePolicy tmp;

			if (info->obj != NULL)
				tmp = rv_user_ask_replace_object2 (self->rv, info->obj, NULL, TRUE);
			else
				tmp = rv_user_ask_replace_object (self->rv, info->type, info->name, NULL, TRUE);

			action = RV_REPLACE_POLICY_TO_ACTION(tmp);
			if (tmp == RV_REPLACE_POLICY_KEEP_ALL) self->policy = tmp;
			else if (tmp == RV_REPLACE_POLICY_REPLACE_ALL) self->policy = tmp;
			else { /* remains ask-user */ }
		}
	}

	return action;
}

IOHandler_ReplaceCallback default_replace_policy_handler_get_callback
	(DefaultReplacePolicyHandler * self)
{
	return _default_replace_policy_handler_callback;
}

void default_replace_policy_handler_set_parent_callback (DefaultReplacePolicyHandler * self,
		IOHandler_ReplaceCallback replace, gpointer user_data)
{
	self->parent_callback  = replace;
	self->parent_user_data = user_data;
}
