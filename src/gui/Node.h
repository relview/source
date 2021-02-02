/* This module contains functions to handle virtual nodes in the
 * graph-window. There are currently two major groups. The first
 * group serves functioniality to determine whether a given point
 * (the mouse) is over a node. The second group are routines to
 * adapt the nodes to the SelectionManager module (node_sel_*).
 *
 * The routines to adapt nodes to the Drag&Drop Manager are 
 * defined out (node_handle_(drap|drop)_*).
 *
 * There is no header file for this file. It is directly included
 * into the graphwindow_gtk.c module. This module uses at least one
 * routine from the Edge.c module, so that file must included first
 * in the including file. */

#ifndef GW_NODE_C
#  define GW_NODE_C

#undef DnD
/* even if you define DnD, drag&drop will not work. It's only
 * included for future demands. */

#include <gtk/gtk.h>
#include "SelectionManager.h"
#include "Graph.h"

GdkRegion *     node_build_sensor       (double radius);
gboolean        node_is_over            (XGraphNode * node, int xp, int yp);

/* adapter routines for the selection manager. See SelectionClass in 
 * SelectionManager.h/.c for details. */
gboolean        node_sel_is_over        (SelectionManager * sm, Selectable sel, gint xp, gint yp);
void            node_sel_highlight      (SelectionManager * sm, Selectable sel);
void            node_sel_unhighlight    (SelectionManager * sm, Selectable sel);
void            node_sel_select         (SelectionManager * sm, Selectable sel);
void            node_sel_unselect       (SelectionManager * sm, Selectable sel);
gpointer        node_sel_iter_begin     (SelectionManager * sm, SelectionClass * class);
void            node_sel_iter_next      (gpointer * piter);
gboolean        node_sel_iter_has_next  (gpointer iter);
Selectable      node_sel_iter_object    (gpointer iter);

#ifdef DnD
/* (unused) adapter routines for the drag&drop manager (DragManager.h/.c) */
void            node_handle_drag_begin  (Dragable * o, const DragManager * m);
void            node_handle_drag_motion (Dragable * o, const DragManager * m);
void            node_handle_drop        (Dragable * o, const DragManager * m);
#endif

#endif /* Node.c */
