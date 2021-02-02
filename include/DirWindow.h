/*
 * DirWindow.h
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

#ifndef DIRWINDOW_H_
#define DIRWINDOW_H_

#include <gtk/gtk.h>

typedef struct _DirWindow DirWindow;

/*!
 * Returned the windows' GtkWidget object.
 */
GtkWidget * 	dir_window_get_widget (DirWindow*);

/*!
 * Returns the global DirWindow singleton.
 */
DirWindow * 	dir_window_get_instance ();

void 			dir_window_show (DirWindow * self);
void 			dir_window_hide (DirWindow * self);
gboolean 		dir_window_is_visible (DirWindow * self);

void 			dir_window_update (DirWindow * self);

/*!
 * Show the hidden relations, or not.
 */
void			dir_window_set_show_hidden (DirWindow * self, gboolean yesno);

#endif /* DIRWINDOW_H_ */
