/*
 * PluginManager.c
 *
 *  Copyright (C) 2009,2010,2011 Stefan Bolus, University of Kiel, Germany
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
#include "pluginImpl.h"
#include "Observer.h"
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#include <assert.h>
#include <string.h>
#include "version.h"

/*******************************************************************************
 *                               Plug-In Manager                               *
 *                     (Manages Plug-Ins for the system.)                      *
 *                                                                             *
 *                              Thu, 30 Jul 2009                               *
 ******************************************************************************/

#define PLUGIN_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,PluginObserver,obj,func, __VA_ARGS__)

struct _Plugin
{
	const RvPlugInInfo * info; /* Belongs to the plugins. */
	gboolean enabled;
	gchar * file; /* absolute path name */
	rvp_plugin_id_t id;

	GSList/*<PluginObserver*>*/ * observers;
};

struct _PlugInManager
{
    //GtkListStore * model;
    GQueue/*<Plugin*>*/ plugins;
    GList/*<gchar*>*/ * dirlist;
    gint next_id;

	GSList/*<PluginManagerObserver*>*/ * observers;
};


/*!
 * Create a new plugin. The plugin itself must be loaded already. The
 * \ref Plugin object is more like a container to store and access the
 * underlying \ref RvPlugInInfo object.
 */
static Plugin * _plugin_new (const RvPlugInInfo * info, rvp_plugin_id_t id,
		const gchar * file)
{
	if (NULL == info) return NULL;
	else {
		Plugin * self = g_new0 (Plugin,1);
		self->file = g_strdup (file);
		self->enabled = FALSE;
		self->info = info;
		self->id = id;
		return self;
	}
}

static void _plugin_changed (Plugin * self) {
	PLUGIN_OBSERVER_NOTIFY(self,changed,_0()); }

void plugin_enable (Plugin * self)
{
	if ( !self->enabled) {
		self->enabled = TRUE;

	    if (self->info->enable) {
	    	verbose_printf (VERBOSE_DEBUG, __FILE__": Plugin \"%s\" enabled.\n", self->info->name);
	    	self->info->enable("");
	    	_plugin_changed (self);
	    }
	    else
	    	g_warning ("Unable to enable plugin. Function 'enable' is missing (NULL).");
	}
}

void plugin_disable (Plugin * self)
{
	if ( self->enabled) {
		self->enabled = FALSE;

		verbose_printf (VERBOSE_DEBUG, __FILE__": Plugin \"%s\" disabled.\n", self->info->name);

		if (self->info->disable) /* may be NULL */ {
			self->info->disable();
			_plugin_changed (self);
		}
		else {
			verbose_printf (VERBOSE_DEBUG, __FILE__": No disable method available.\n");
		}
	}
}

gboolean plugin_is_enabled (const Plugin * self) { return self->enabled; }
const gchar * plugin_get_name (const Plugin * self) { return self->info->name; }
const gchar * plugin_get_descr (const Plugin * self) { return self->info->descr; }
const gchar * plugin_get_file (const Plugin * self) { return self->file; }

/*!
 * Destroy the given plugin. This can only be done by the \ref PluginManager.
 */
static void _plugin_destroy (Plugin * self)
{
	PLUGIN_OBSERVER_NOTIFY(self,onDelete,_0());

	plugin_disable (self);
	if (self->info->quit)
		self->info->quit ();

	g_free (self);
}

void plugin_register_observer (Plugin * self, PluginObserver * o)
{
	if (NULL == g_slist_find (self->observers, o))
		self->observers = g_slist_prepend (self->observers, o);
}

void plugin_unregister_observer (Plugin * self, PluginObserver * o) {
	self->observers = g_slist_remove (self->observers, o); }


#define PLUGIN_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,PluginManagerObserver,obj,func, __VA_ARGS__)

PluginManager * plugin_manager_new ()
{
    PluginManager * self = g_new0 (PluginManager, 1);

    self->next_id = 1;

    /* Initialize the plugin interface. */
    rv_plugin_intf_init ();

    g_queue_init (&self->plugins);

    return self;
}

void plugin_manager_register_observer (PluginManager * self, PluginManagerObserver * o)
{
	if (NULL == g_slist_find (self->observers, o))
		self->observers = g_slist_prepend (self->observers, o);
}

void plugin_manager_unregister_observer (PluginManager * self, PluginManagerObserver * o) {
	self->observers = g_slist_remove (self->observers, o); }

void plugin_manager_changed (PluginManager * self) {
	PLUGIN_MANAGER_OBSERVER_NOTIFY(self,changed,_0()); }


void plugin_manager_destroy (PluginManager * self)
{
	GList * iter = self->plugins.head;

	for ( ; iter ; iter = iter->next )
		_plugin_destroy ((Plugin*) iter->data);

    g_list_foreach(self->dirlist,(GFunc)g_free,NULL);
    g_list_free(self->dirlist);

    g_free (self);
}


static const RvPlugInInfo *
_load_plugin (const gchar * file, GError ** err, rvp_plugin_id_t id)
{
    GModule * module;
    RvPlugInMainFunc plugin_main = NULL;
    RvPlugInInfo * info = NULL;
    int version[2] = {RELVIEW_MAJOR_VERSION, RELVIEW_MINOR_VERSION};

    VERBOSE(VERBOSE_DEBUG, printf ("Loading plug-in: %s\n", file););

    module = g_module_open(file, G_MODULE_BIND_LOCAL | G_MODULE_BIND_LAZY);
    if (!module)
    {
        g_set_error(err, NULL, "open error", "%s", g_module_error());
        return NULL;
    }
    if (!g_module_symbol(module, "rv_plugin_main", (gpointer *) &plugin_main))
    {
        g_set_error(err, NULL, "symbol error", "%s: %s", file,
                g_module_error());
        if (!g_module_close(module)) g_warning("%s: %s", file,
                g_module_error());
        return NULL;
    }
    if (plugin_main == NULL)
    {
        g_set_error(err, NULL, "plugin_main==NULL", "symbol plugin_main is NULL");
        if (!g_module_close(module)) g_warning("%s: %s", file,
                g_module_error());
        return NULL;
    }

    info = plugin_main();
    if (info->init) {
        if (! info->init(version, id)) {
            g_set_error(err, NULL, "init returned FALSE", "%s", file);
            if (!g_module_close(module)) g_warning("%s: %s", file,
                    g_module_error());
            return NULL;
        }
    }

    return info;
}

gboolean plugin_manager_is_loaded (PluginManager * self, const gchar * file)
{
    if (NULL == file) return FALSE;
    else {
    	GList * iter = self->plugins.head;

    	for ( ; iter ; iter = iter->next ) {
    		if (g_str_equal (((Plugin*) iter->data)->file, file))
    			return TRUE;
    	}

        return FALSE;
    }
}

static rvp_plugin_id_t _plugin_manager_alloc_id (PluginManager * self)
{
	return self->next_id++;
}

/* file must be an absolute pathname */
gboolean
plugin_manager_add_file (PluginManager * self, const gchar * file, GError ** perr)
{
    if (plugin_manager_is_loaded (self, file)) {
        return TRUE;
    }
    else {
        const RvPlugInInfo * info = NULL;
        rvp_plugin_id_t id = _plugin_manager_alloc_id (self);

        info = _load_plugin (file, perr, id);
        if (! info)
            return FALSE;
        else {
        	g_queue_push_tail (&self->plugins, _plugin_new (info, id, file));
        	plugin_manager_changed (self);
            return TRUE;
        }
    }
}

void
plugin_manager_refresh_dir (PluginManager * self, const gchar * dirname)
{
    GError * err = NULL;
    GDir * dir = g_dir_open (dirname, 0, &err);
    if (NULL == dir) {
        //g_log("relview", G_LOG_LEVEL_WARNING,
        //        "Unable to directory %s. Reason: %s.", dirname, err->message);
        g_error_free(err);
        return;
    }
    else {
      const gchar * file;

      while ((file = g_dir_read_name (dir))) {
        if (g_str_has_suffix(file,"."G_MODULE_SUFFIX)) {
            gchar * abs_file = g_build_filename (dirname,file,NULL);
            GError * err = NULL;
            gboolean ret = FALSE;

            /* Won't add a module a second time. */
            ret = plugin_manager_add_file(self,abs_file,&err);
            if (! ret) {
                g_log ("relview", G_LOG_LEVEL_MESSAGE,
                        "Unable to load module %s: Reason: %s", abs_file,
                        err->message);
                g_error_free(err);
            }

            g_free (abs_file);
        }
      }

      g_dir_close(dir);
    }
}

/* refreshs all directories previouly added. */
void plugin_manager_refresh (PluginManager * self)
{
    GList * iter = self->dirlist;
    for ( ; iter ; iter = iter->next)
        plugin_manager_refresh_dir(self, (gchar*) iter->data);
}


/* will be loaded immediately. Strings in dirlist are duplicated. */
void
plugin_manager_add_dirs (PluginManager * self, const GSList/*<const gchar*>*/ * dirlist)
{
    const GSList * iter = dirlist;

    for ( ; iter ; iter = iter->next) {
        const gchar * dirname = (gchar*)iter->data;
        if (! g_list_find_custom(self->dirlist, dirname, (GCompareFunc)strcmp)) {
            self->dirlist = g_list_prepend(self->dirlist, g_strdup (dirname));
            plugin_manager_refresh_dir (self, dirname);
        }
        else { /* dirname is already in the list. */ }
    }
}

void plugin_manager_add_dir (PluginManager * self, const gchar * dirname)
{
    GSList * l = g_slist_prepend (NULL, (gpointer)dirname);
    plugin_manager_add_dirs(self, l);
    g_slist_free(l);
}

const GList/*<Plugin*>*/ * plugin_manager_get_plugins (PluginManager * self) {
	return self->plugins.head;
}

Plugin * plugin_manager_lookup_by_id (PluginManager * self, rvp_plugin_id_t id)
{
	GList/*<Plugin*>*/ * iter = self->plugins.head;
	for ( ; iter ; iter = iter->next) {
		Plugin * cur = (Plugin*) iter->data;
		if (cur->id == id)
			return cur;
	}
	return NULL;
}

static PluginManager * _plugin_manager = NULL;

PluginManager * plugin_manager_create_instance ()
{
    _plugin_manager = plugin_manager_new();
    assert (_plugin_manager != NULL);
    return _plugin_manager;
}

PluginManager * plugin_manager_get_instance ()
{
    assert (_plugin_manager);
    return _plugin_manager;
}

