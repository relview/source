/*
 * DirWindow.h
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

#include "Function.h"
#include "Program.h"

#include "utilities.h"
#include "msg_boxes.h" /* name_not_allowed */
#include "Relview.h" /* rv_get_gtk_builder, rv_get_instance */
#include "DirWindow.h" /* init_rel_dir */
#include "RelationWindow.h"
#include "pluginImpl.h"


void rv_plugin_intf_init ()
{
	/* Currently no initialization needed. */
}

void rvp_relation_window_set_relation (Rel * rel)
{
	if (rel != NULL)
	relation_window_set_relation (relation_window_get_instance(), rel);
}

void rvp_disable_plugin (rvp_plugin_id_t id)
{
	PluginManager * manager = plugin_manager_get_instance();
	Plugin * plugin = plugin_manager_lookup_by_id (manager, id);
	if (plugin)
		plugin_disable (plugin);
}

