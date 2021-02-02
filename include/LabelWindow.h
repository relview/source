/*
 * LabelWindow.h
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

#ifndef LABELWINDOW_H_
#define LABELWINDOW_H_

#include "Relview.h"
#include <gtk/gtk.h>

typedef struct _LabelWindow LabelWindow;

LabelWindow * 		label_window_get_instance ();
LabelWindow * 		label_window_new (Relview * rv);
void 				label_window_destroy (LabelWindow * self);
void 				label_window_show (LabelWindow * self);
void 				label_window_hide (LabelWindow * self);
gboolean 			label_window_is_visible (LabelWindow * self);
GtkWidget * 		label_window_get_widget (LabelWindow * self);
void 				label_window_update (LabelWindow * self);

#endif /* LABELWINDOW_H_ */
