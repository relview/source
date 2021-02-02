/*
 * FileWindow.h
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

#ifndef FILEWINDOW_H_
#define FILEWINDOW_H_

typedef struct _FileWindow FileWindow;

FileWindow * file_window_get_instance ();
void		 file_window_show (FileWindow * self);
void		 file_window_hide (FileWindow * self);
gboolean	 file_window_is_visible (FileWindow * self);

/*!
 * Returns the window's \ref GtkWidget object.
 */
GtkWidget * file_window_get_widget (FileWindow * self);

#endif /* FILEWINDOW_H_ */
