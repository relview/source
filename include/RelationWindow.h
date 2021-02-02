/*
 * RelationWindow.h
 *
 *  Copyright (C) 2008,2009,2010 Stefan Bolus, University of Kiel, Germany
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

#ifndef RELATIONWINDOW_H
#  define RELATIONWINDOW_H

#include <gtk/gtk.h>
#include "RelationViewport.h"

#define RELATION_WINDOW_MENU_PREFIX "/relation-window"

typedef struct _RelationWindow RelationWindow;
typedef struct _RelationWindowObserver RelationWindowObserver;

RelationWindow * relation_window_new ();
void relation_window_destroy (RelationWindow*);
void relation_window_get_size (RelationWindow * self,
                               int * pwidth, int * pheight);
RelationViewport * relation_window_get_viewport (RelationWindow * self);
gboolean relation_window_has_relation (RelationWindow * self);
gboolean relation_window_is_visible (RelationWindow * self);
void relation_window_hide (RelationWindow * self);
void relation_window_show (RelationWindow * self);
void relation_window_set_relation (RelationWindow * self, Rel * rel);
Rel * relation_window_get_relation (RelationWindow * self);
GtkWidget * relation_window_get_window (RelationWindow * self);


RelationWindow *   relation_window_get_instance       ();
void               relation_window_destroy_instance   ();


void show_relationwindow (void);
void hide_relationwindow (void);
int relationwindow_isVisible (void);
void redraw_rel (RelationWindow*);

void menuitemNew_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemDelete_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemRename_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemClear_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemRandomFill_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemRelationToGraph_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemPrint_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemLabelRel_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemUnlabelRel_activate (GtkMenuItem * menuitem, gpointer user_data);
void menuitemCreate01Label_activate (GtkMenuItem * menuitem, gpointer user_data);


typedef void (*RelationWindowObserver_relChangedFunc) (gpointer, RelationWindow*,
		Rel * old_rel, Rel * new_rel);

/*interface*/ struct _RelationWindowObserver
{
	/* The displayed relation has changed it's name or pointer. */
	RelationWindowObserver_relChangedFunc relChanged;

	gpointer object;
};

void relation_window_register_observer (RelationWindow * self, RelationWindowObserver * o);
void relation_window_unregister_observer (RelationWindow * self, RelationWindowObserver * o);

#endif /* RelationWindow.h */
