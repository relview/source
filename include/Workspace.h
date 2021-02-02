/*
 * Workspace.h
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

#ifndef WORKSPACE_H_
#define WORKSPACE_H_

#include <gtk/gtk.h>

typedef struct _Workspace Workspace;

Workspace * 	workspace_new ();
void 			workspace_destroy (Workspace * self);
void 			workspace_add_window (Workspace * self, GtkWindow * window, const gchar * key);
gboolean 		workspace_is_window_visible (Workspace * self, gpointer * window);
gboolean 		workspace_is_window_visible_by_default (Workspace * self, GtkWindow * window);
void 			workspace_save (Workspace * self);

#endif /* WORKSPACE_H_ */
