/*
 * IOHandler.c
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

#include "IOHandler.h"
#include <string.h>

void io_handler_destroy (IOHandler * self) { self->c->destroy (self); }

gboolean io_handler_load_rels (IOHandler * self, const gchar * filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	if (self->c->loadRels) return self->c->loadRels (self,filename,replace,user_data,perr);
	else return TRUE;
}

gboolean io_handler_load_graphs (IOHandler * self, const gchar * filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	if (self->c->loadGraphs) return self->c->loadGraphs (self,filename,replace,user_data,perr);
	else return TRUE;
}

gboolean io_handler_load (IOHandler * self, const gchar * filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	if (self->c->load) return self->c->load (self,filename,replace,user_data,perr);
	else return TRUE;
}

gboolean io_handler_load_by_action (IOHandler * self, IOHandlerActions action,
		const gchar * filename, IOHandler_ReplaceCallback replace,
		gpointer user_data, GError ** perr)
{
	struct {
		IOHandlerActions action;
		gboolean (*f) (IOHandler*,const gchar*,
				IOHandler_ReplaceCallback,gpointer,GError** );
	} *ptr, table[] = { { IO_HANDLER_ACTION_LOAD,  io_handler_load},
						{ IO_HANDLER_ACTION_LOAD_RELS, io_handler_load_rels },
						{ IO_HANDLER_ACTION_LOAD_GRAPHS, io_handler_load_graphs },
						{ 0, NULL }};
	for (ptr = table ; ptr->f ; ++ ptr) {
		if (ptr->action & action)
			return ptr->f (self, filename, replace, user_data, perr);
	}

	g_warning ("io_handler_load_by_action: Unknown action!: %d", (gint)action);
	g_set_error (perr, 0,0, "io_handler_load_by_action: Unknown action!: %d", (gint)action);
	return FALSE;
}

gboolean io_handler_save_single_rel (IOHandler * self, const gchar * filename, Rel * rel, GError ** perr)
{
	if (self->c->saveSingleRel) return self->c->saveSingleRel (self,filename,rel,perr);
	else return TRUE;
}

gboolean io_handler_save_single_graph (IOHandler * self, const gchar * filename, XGraph * gr, GError ** perr)
{
	if (self->c->saveSingleGraph) return self->c->saveSingleGraph (self,filename,gr,perr);
	else return TRUE;
}

gboolean io_handler_save (IOHandler * self, const gchar * filename, GError ** perr)
{
	if (self->c->save) return self->c->save (self,filename,perr);
	else return TRUE;
}

const gchar * io_handler_get_name (IOHandler * self) { return self->c->name; }
const gchar * io_handler_get_description (IOHandler * self) { return self->c->description; }
const gchar * io_handler_get_extensions (IOHandler * self) { return self->c->extensions; }

gboolean io_handler_matches (IOHandler * self, const gchar * filename)
{
	gchar ** exts = g_strsplit (self->c->extensions, ",", 0);
	gchar ** iter = exts;
	gint len = strlen (filename);
	gboolean ret = FALSE;

	for ( ; *iter && !ret ; ++ iter) {
        gint slen = strlen (*iter);
        if (len < slen) /* filename too short */ continue;
        else {
            if (filename[len-slen-1] == '.'
            		&& (0 == g_ascii_strncasecmp(filename+len-slen,*iter,slen)))
                ret = TRUE;
        }
	}

	g_strfreev (exts);
	return ret;
}

/*!
 * Returns TRUE, if the given \ref IOHandler supports any of the given actions.
 * Especially TRUE is returned if a is empty.
 */
gboolean io_handler_supports_any_action (IOHandler * self, IOHandlerActions a)
{
	if (0 == a) return TRUE;
#define _IMPL(a,b) ((a)&&(b))
	return _IMPL(a & IO_HANDLER_ACTION_LOAD, self->c->load != NULL)
			|| _IMPL(a & IO_HANDLER_ACTION_SAVE, self->c->save != NULL)
			|| _IMPL(a & IO_HANDLER_ACTION_LOAD_GRAPHS, self->c->loadGraphs != NULL)
			|| _IMPL(a & IO_HANDLER_ACTION_LOAD_RELS, self->c->loadRels != NULL)
			|| _IMPL(a & IO_HANDLER_ACTION_SAVE_SINGLE_GRAPH, self->c->saveSingleGraph != NULL)
			|| _IMPL(a & IO_HANDLER_ACTION_SAVE_SINGLE_REL, self->c->saveSingleRel != NULL);
#undef _IMPL
}

gboolean io_handler_supports_action (IOHandler * self, IOHandlerActions a)
{ return io_handler_supports_any_action (self, a); }

gchar * io_handler_get_any_extension (IOHandler * self)
{
	gchar ** exts = g_strsplit (self->c->extensions, ",", 0);
	if (! exts) return NULL;
	else {
		gchar * ret = g_strdup (exts[0]);
		g_strfreev(exts);
		return ret;
	}
}

GList/*<gchar*>*/ * io_handler_get_extensions_as_list (IOHandler * self)
{
	gchar ** exts = g_strsplit (self->c->extensions, ",", 0);
	gchar ** iter = exts;
	GList/*<gchar*>*/ * ret = NULL;

	for (iter = exts ; *iter ; ++ iter)
		ret = g_list_prepend (ret, g_strdup (*iter));

	g_strfreev(exts);
	return g_list_reverse (ret);
}
