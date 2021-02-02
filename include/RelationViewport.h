/*
 * RelationViewport.h
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

#ifndef RELATIONVIEWPORT_H
#  define RELATIONVIEWPORT_H

#include <gtk/gtk.h>
#include "Relation.h"
#include "label.h"

/* ----------------------------------------------------- RelationViewport --- */

typedef enum _RelationViewportLineMode
{
  MODE_DOWN_UP = KURE_DIR_DOWN_UP, /* | */
  MODE_LEFT_RIGHT = KURE_DIR_LEFT_RIGHT, /* - */
  MODE_DOWN_RIGHT = KURE_DIR_DOWN_RIGHT, /* \ */
  MODE_DOWN_LEFT = KURE_DIR_DOWN_LEFT /* / */
} RelationViewportLineMode;

typedef struct _RelationViewport RelationViewport;


/* If the function returns FALSE, the menu will not be shown. */
typedef gboolean (*RelationViewportPreparePopupFunc) (RelationViewport*,
                                                      GtkWidget*, gpointer); 

#define RELATION_VIEWPORT_PREPARE_POPUP_FUNC(f) \
        ((RelationViewportPreparePopupFunc)(f))


/* generic mechanism to retrieve the labels for a given relation. */
typedef void (*RelationViewportGetLabelsFunc) (RelationViewport*,
                                               Rel*, 
                                               const gchar **, /* cols */
                                               const gchar **, /* rows */
                                               gpointer);
#define RELATION_VIEWPORT_GET_LABELS_FUNC(f) \
        ((RelationViewportGetLabelsFunc)(f))


RelationViewport * relation_viewport_new ();
void relation_viewport_destroy (RelationViewport * self);

Relview * relation_viewport_get_relview (RelationViewport * self);

void relation_viewport_set_popup (RelationViewport * self,
                                  GtkWidget * popup,
                                  RelationViewportPreparePopupFunc f,
                                  gpointer user_data);

gboolean relation_viewport_has_relation (RelationViewport * self);
void relation_viewport_set_relation (RelationViewport * self, Rel * rel);
Rel * relation_viewport_get_relation (RelationViewport * self);
RelationViewportLineMode
relation_viewport_get_line_mode (RelationViewport * self);
/* delta is the size of the displayed rects for the relation bits. */
int _relation_viewport_get_delta (RelationViewport * self);
GtkWidget * relation_viewport_get_drawable (RelationViewport * self);
GtkWidget * relation_viewport_get_widget (RelationViewport * self);
void _relation_viewport_attach (RelationViewport * self, GtkWidget * widget);
RelationViewport * relation_viewport_get (GtkWidget * widget);
void relation_viewport_redraw (RelationViewport * self);
void relation_viewport_set_zoom (RelationViewport * self, int zoom);
int relation_viewport_get_zoom (RelationViewport * self);
void relation_viewport_clear_relation (RelationViewport * self);

void relation_viewport_invalidate_rect (RelationViewport * self,
                                        GdkRectangle * rc);
void relation_viewport_set_line_mode (RelationViewport * self,
                                      RelationViewportLineMode);

void relation_viewport_configure (RelationViewport * self);
void relation_viewport_refresh (RelationViewport * self);

#endif /* RelationViewport.h */
