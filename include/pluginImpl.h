/*
 * pluginImpl.h
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

#ifndef PLUGINIMPL_H_
#define PLUGINIMPL_H_

#include "plugin.h"

typedef struct _Plugin Plugin;
typedef struct _PlugInManager PluginManager;

/* Changed from enabled to disabled or vice-versa. */
typedef void (*PluginManagerObserver_changedFunc) (gpointer, PluginManager*);

typedef struct _PluginManagerObserver {
        PluginManagerObserver_changedFunc changed;

        gpointer object;
} PluginManagerObserver;


/* Changed from enabled to disabled or vice-versa. */
typedef void (*PluginObserver_changedFunc) (gpointer, Plugin*);
typedef void (*PluginObserver_onDeleteFunc) (gpointer, Plugin*);

typedef struct _PluginObserver {
        PluginObserver_changedFunc changed;
        PluginObserver_onDeleteFunc onDelete;

        gpointer object;
} PluginObserver;

void 			rv_plugin_intf_init ();

void 			plugin_enable (Plugin * self);
void 			plugin_disable (Plugin * self);
gboolean 		plugin_is_enabled (const Plugin * self);
const gchar * 	plugin_get_name (const Plugin * self);
const gchar * 	plugin_get_descr (const Plugin * self);
const gchar * 	plugin_get_file (const Plugin * self);
void 			plugin_register_observer (Plugin * self, PluginObserver * o);
void 			plugin_unregister_observer (Plugin * self, PluginObserver * o);

PluginManager * plugin_manager_new ();
void 			plugin_manager_register_observer (PluginManager * self, PluginManagerObserver * o);
void 			plugin_manager_unregister_observer (PluginManager * self, PluginManagerObserver * o);
void 			plugin_manager_changed (PluginManager * self);
void 			plugin_manager_destroy (PluginManager * self);
gboolean 		plugin_manager_is_loaded (PluginManager * self, const gchar * file);
gboolean		plugin_manager_add_file (PluginManager * self, const gchar * file, GError ** perr);
void			plugin_manager_refresh_dir (PluginManager * self, const gchar * dirname);
void 			plugin_manager_refresh (PluginManager * self);
void			plugin_manager_add_dirs (PluginManager * self, const GSList/*<const gchar*>*/ * dirlist);
void 			plugin_manager_add_dir (PluginManager * self, const gchar * dirname);
const GList/*<Plugin*>*/ * plugin_manager_get_plugins (PluginManager * self);
Plugin * 		plugin_manager_lookup_by_id (PluginManager * self, rvp_plugin_id_t id);
PluginManager * plugin_manager_create_instance ();
PluginManager * plugin_manager_get_instance ();

#endif /* PLUGINIMPL_H_ */
