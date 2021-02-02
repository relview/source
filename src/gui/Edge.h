/*
 * Edge.h
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

/* This module contains functions to handle the virtual edges in the graph-
 * window. As in the Node.c module there are routines to determine whether
 * a given point is over an edge, and adapter routines for the selection
 * manager, so edges can become selected by the user edge_sel_*. See the
 * SelectionManager module for details.
 *
 * There is no header file for this module. It will directly included into
 * the graphwindow_gtk.c file. The Vec2D.c module is needed, so it must be
 * included in the including file, before this one.
 */

#ifndef GW_EDGE_C
#  define GW_EDGE_C

#include <gtk/gtk.h>
#include "SelectionManager.h"
#include "Graph.h"

/*! half of the overall margin of the sensitive area around the line segment.
 * 
 * |--margin--|(line)|--margin--|
 *     ...       ...     ...
 * |--margin--|(line)|--margin--|
 *
 */
#define EDGE_MARGIN_WIDTH 7

/* half of the overall margin of the sensitive area around the arc of an
 * reflexice edge. Similar to a concept of a default edge (see above). */
#define EDGE_ARC_MARGIN_WIDTH 5

#define EDGE_ARC_RADIUS RADIUS

typedef struct _EdgePathPoint
{
  gint x, y;
} EdgePathPoint;

typedef struct _EdgePath
{
  /* the first is the from node, the last is the to node. */
  EdgePathPoint * points;
  gint pointCount;
} EdgePath;

gboolean        edge_is_over_raw        (int xa, int ya, int xb, int yb,
                                        int xp, int yp, int marginWidth);
GdkPoint        circle_point            (double radius, double angle);
GdkRegion *     edge_build_reflexive_sensor     (int radius, int marginWidth);
gboolean        edge_is_over_reflexive  (XGraphNode * node, int xp, int yp,
                                         int radius, int marginWidth);
gboolean        edge_is_over            (XGraph * gr, XGraphEdge * edge,
                                         int xp, int yp);
gboolean        edge_sel_is_over        (SelectionManager * sm, Selectable sel, gint xp, gint yp);
void            edge_sel_highlight      (SelectionManager * sm, Selectable sel);
void            edge_sel_unhighlight    (SelectionManager * sm, Selectable sel);
void            edge_sel_select         (SelectionManager * sm, Selectable sel);
void            edge_sel_unselect       (SelectionManager * sm, Selectable sel);
gpointer        edge_sel_iter_begin     (SelectionManager * sm, SelectionClass * class);
void            edge_sel_iter_next      (gpointer * piter);
gboolean        edge_sel_iter_has_next  (gpointer iter);
Selectable      edge_sel_iter_object    (gpointer iter);

void            edge_path_destroy       (EdgePath * path);
void            edge_path_dump          (EdgePath * path);
EdgePath *      edge_path_build         (XGraph * gr, XGraphEdge * edge);
gint            edge_path_get_segment_count     (EdgePath * path);
void            edge_path_get_segment   (EdgePath * path, gint i,
                                         EdgePathPoint *from,
                                         EdgePathPoint *to);

#endif /* Edge.c */
