/*
 * GraphWindow.h
 *
 *  Copyright (C) 2009,2010 Stefan Bolus, University of Kiel, Germany
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

#ifndef GRAPHWINDOW_H
#  define GRAPHWINDOW_H

#include <gtk/gtk.h>
#include "Graph.h"
#include "label.h"

#if !defined IN_RV_PLUGIN
# include "gui/SelectionManager.h"
#endif

typedef struct _GraphWindow GraphWindow;

#if !defined IN_RV_PLUGIN
typedef enum _GraphWindowRedrawType
{
  OLD, NEW
} GraphWindowRedrawType;
#endif

XGraph * 		graph_window_get_graph (GraphWindow*);
gboolean 		graph_window_has_graph (GraphWindow*);
void 			graph_window_set_graph (GraphWindow*,XGraph*);
gboolean 		graph_window_is_visible (GraphWindow * gw);
gboolean 		graph_window_is_editable (GraphWindow * gw);

void 			graph_window_hide (GraphWindow * gw);
void 			graph_window_show (GraphWindow * gw);

GraphWindow *   graph_window_get_instance       ();

gint 			graph_window_get_display_width (GraphWindow * gw);
gint 			graph_window_get_display_height (GraphWindow * gw);

#if !defined IN_RV_PLUGIN
void 			graph_window_ctor (GraphWindow * gw);

gboolean 		graph_window_is_grid_active (GraphWindow * gw);

GraphWindow * 	graph_window_new ();
void 			graph_window_attach (GraphWindow*, GtkWindow*);
GraphWindow * 	graph_window_get (GtkWidget*);
SelectionManager * graph_window_get_selection_manager (GraphWindow * gw);

GdkPoint 		graph_window_get_visible_origin (GraphWindow*);
GdkRectangle 	graph_window_get_visible_rect (GraphWindow*);
void 			graph_window_set_gdk_cursor (GraphWindow * gw,
                                  GdkCursorType cursor_type);
void 			graph_window_reset_cursor (GraphWindow * gw);
void 			graph_window_set_cursor_position (GraphWindow * gw, gint x, gint y);

void 			graph_window_redraw (GraphWindow * gw, GraphWindowRedrawType type);
void 			graph_window_invalidate_display (GraphWindow * gw, gboolean displayGraph);
void 			graph_window_display_node (GraphWindow * gw, XGraphNode * node);
void 			graph_window_display_edge (GraphWindow * gw, XGraphEdge * edge);

gint 			graph_window_get_phy_display_width (GraphWindow * gw);
gint 			graph_window_get_phy_display_height (GraphWindow * gw);
GtkWidget *		graph_window_get_widget (GraphWindow*);
void 			graph_window_expose (GraphWindow * gw, GdkEventExpose * event);

void 			graph_window_create_node (GraphWindow * gw, int x, int y);
void 			graph_window_create_edge (GraphWindow * gw, XGraphNode * from, XGraphNode * to);

void 			graph_window_align_to_grid (GraphWindow * gw, GdkPoint /*inout*/ * pt);
GdkPoint 		graph_window_mouse_to_viewport (GraphWindow * gw, gint x, gint y);

void 			graph_window_move_node_to (GraphWindow * gw, XGraphNode * node, gint x, gint y);

/* Der Begriff 'select' bezieht sich hier auf die Auswahl, die verwendet wird,
 * um Kanten zu erstellen und hat nichts mit dem Begriff zu tun, der fuer den
 * SelectionManager verwendet wird. */
gboolean 		graph_window_has_selected_node (GraphWindow * gw);
void 			graph_window_set_selected_node (GraphWindow * gw, XGraphNode * node);
XGraphNode * 			graph_window_get_selected_node (GraphWindow * gw);
void 			graph_window_unselect_node (GraphWindow * gw);

XGraphNode *	graph_window_node_from_position (GraphWindow * gw, gint x, gint y);
XGraphEdge *	graph_window_edge_from_position (GraphWindow * gw, gint x, gint y);

void 			graph_window_fit_zoom_to_graph (GraphWindow * gw);
void 			graph_window_set_zoom (GraphWindow * gw, double zoom);
double 			graph_window_get_zoom (GraphWindow * gw);
void 			graph_window_reset_zoom (GraphWindow * gw);

void 			graph_window_destroy (GraphWindow * gw);


void 			graph_window_set_label (GraphWindow * self, Label * label);
Label * 		graph_window_get_label (GraphWindow * self);


/* For internal use only. */
void 			_graph_window_add_to_history (GraphWindow * self, const gchar * name);
void 			_graph_window_clear_history (GraphWindow * self);
GList/*<const gchar*>*/ * _graph_window_get_most_recent (GraphWindow * self, gint n);

void            graph_window_destroy_instance   ();
#endif

#endif /* GraphWindow.h */

