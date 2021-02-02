/*
 * GraphWindow.c
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

#include "GraphWindowImpl.h"
#include "RelationWindow.h"

#include "Relview.h"
#include "utilities.h" /* getCanvasOrigin, getVisibleRect */
#include "prefs.h"
#include "Node.h" /* node_is_over */
#include "Edge.h" /* edge_is_over */
#include <stdlib.h> /* calloc */
#include <string.h>
#include <assert.h>


/* Callbacks for the XGraphObserver */
static void _graph_window_on_delete (GraphWindow*, XGraph*);
static void _graph_window_renamed (GraphWindow*, XGraph*, const gchar*);
static void _graph_window_changed (GraphWindow*, XGraph*);
static void _graph_window_layout_changed (GraphWindow*, XGraph*);
static void _graph_window_on_delete_node (GraphWindow*, XGraph*, XGraphNode*);
static void _graph_window_on_delete_edge (GraphWindow*, XGraph*, XGraphEdge*);

static void _graph_window_update_title (GraphWindow * gw);


#define GRAPH_WINDOW_OBJECT_ATTACH_KEY "GraphWindowObj"

static void _graph_window_block_observer (GraphWindow * self)
{ self->block_observer = TRUE; }
static void _graph_window_unblock_observer (GraphWindow * self)
{ self->block_observer = FALSE; }

void _graph_window_add_to_history (GraphWindow * self, const gchar * name)
{
	/* If the entry is already in the history, remove it and re-prepend it
	 * to the queue. */
	GList * iter = g_queue_find_custom(&self->history, name,
			(GCompareFunc) strcmp);
	if (iter) {
		g_free (iter->data);
		g_queue_delete_link (&self->history, iter);
	}

	g_queue_push_head (&self->history, g_strdup (name));
}

/*!
 * Removes dead entries from the history. Used internally by the history
 * routines.
 */
static void _graph_window_update_history (GraphWindow * self)
{
	XGraphManager * manager = xgraph_manager_get_instance();
	GList/*<gchar>*/ * iter = self->history.head;
	for ( ; iter ; ) {
		if (! xgraph_manager_exists(manager, (gchar*) iter->data)) {
			GList * tmp = iter->next;

			g_free (iter->data);
			g_queue_delete_link (&self->history, iter);

			iter = tmp;
		}
		else iter = iter->next;
	}
}

/*!
 * Returns (at most) the n graphs which were most recently shown in the graph
 * window. The list can be empty.
 *
 * \return Returns a list which has to be freed using \ref g_list_free. NULL is
 *         returned, if the list is empty (as usual for GList).
 */
GList/*<const gchar*>*/ * _graph_window_get_most_recent (GraphWindow * self, gint n)
{
	GList/*<const gchar*>*/ * ret = NULL;
	GList/*<gchar>*/ * iter = self->history.head;
	gint copied = 0;

	_graph_window_update_history (self);

	for ( ; iter && copied < n; iter = iter->next, ++ copied) {
		ret = g_list_prepend (ret, iter->data); /* reverse order */
	}

	return g_list_reverse (ret);
}

/*!
 * Returns the complete history, i.e. all graphs which were shown in the
 * graph window so far.
 */
GList/*<const gchar*>*/ * _graph_window_get_recent (GraphWindow * self)
{
	_graph_window_update_history (self);
	return g_list_copy (self->history.head);
}

void _graph_window_clear_history (GraphWindow * self)
{
	GList/*<gchar*>*/ * iter = self->history.head;
	for ( ; iter ; iter = iter->next) {	g_free (iter->data); }
	g_queue_clear (&self->history);
}


/*! Returns whether there is a graph associated to the given graph window.
 *
 * \author stb
 * \param gw The graph window.
 * \return Returns TRUE, if the window has a graph associated to it.
 */
gboolean graph_window_has_graph (GraphWindow * gw)
{
  return gw->graph != NULL;
}

/*! Sets the given (mouse) cursor.
 *
 * \author stb
 * \param gw The graph window.
 * \param cursor_type A Gdk cursor.
 */
void graph_window_set_gdk_cursor (GraphWindow * gw,
                                  GdkCursorType cursor_type)
{
  GdkCursor * cursor = gdk_cursor_new (cursor_type);
  gdk_window_set_cursor (gw->drawingarea->window, cursor);
  gdk_cursor_destroy (cursor);
}

/*! Resets the (mouse) cursor of the given graph window. Reseting actually
 * means, that the cursor will set to NULL, so the cursor of the parent window
 * will be used. See gdk_window_set_cursor(...);
 *
 * \author stb
 * \param gw The graph window.
 */
void graph_window_reset_cursor (GraphWindow * gw)
{
  gdk_window_set_cursor (gw->drawingarea->window, NULL);
}


GdkPoint graph_window_get_visible_origin (GraphWindow * gw)
{
  GdkPoint bpt = getCanvasOrigin (gw->hscroll, gw->vscroll);
  GdkPoint pt = { bpt.x, bpt.y };
  return pt;
}

/*!
 * \author stb
 * \param gw The graph window.
 * \return Returns the absolute rectangle of the visible portion of the
 *         graph associated to the graph in the given graph window.
 */
GdkRectangle graph_window_get_visible_rect (GraphWindow * gw)
{
  return getVisibleRect (gw->drawingarea, gw->hscroll, gw->vscroll);
}

GtkWidget *	graph_window_get_widget (GraphWindow * self) { return self->window; }


GraphWindow * graph_window_new ()
{
  return (GraphWindow*) calloc (1, sizeof (GraphWindow));
}

void graph_window_set_graph(GraphWindow * gw, XGraph * graph)
{
    if (gw->graph != graph)
	{
		Label * label = NULL;

		if (gw->graph)
			xgraph_unregister_observer(gw->graph, &gw->graphObserver);

		gw->graph = graph;

		if (gw->graph) {
			RelManager * manager = rv_get_rel_manager (gw->rv);
			Rel * rel = rel_manager_get_by_name (manager, xgraph_get_name(graph));

			xgraph_register_observer(gw->graph, &gw->graphObserver);

			if (rel) {
				RelationViewport * vp = relation_window_get_viewport (relation_window_get_instance ());
				LabelAssoc assoc = rv_label_assoc_get (gw->rv, rel, vp);

				if (assoc.labels[0] == assoc.labels[1])
					label = assoc.labels[0];
			}

		}

    graph_window_set_label (gw, label);

		graph_window_redraw(gw, NEW);
		_graph_window_update_title(gw);
		if (graph != NULL)
			_graph_window_add_to_history(gw, xgraph_get_name(graph));

        }
}

XGraph * graph_window_get_graph (GraphWindow * gw)
{
  return gw->graph;
}


void graph_window_attach (GraphWindow * gw, GtkWindow * window)
{
  g_object_set_data (G_OBJECT(window), GRAPH_WINDOW_OBJECT_ATTACH_KEY,
                     (gpointer) gw);
  gw->window = GTK_WIDGET(window);
}

GraphWindow * graph_window_get (GtkWidget * widget)
{
  while (widget) {
    if (GTK_IS_MENU (widget)) {
      widget = gtk_menu_get_attach_widget (GTK_MENU(widget));
      if (! widget) {
        fprintf (stderr,
                 "The clicked menu doesn't have an attached widget.\n");
        assert (widget != NULL);
      }
    }
    else {
      gpointer * pdata
        = g_object_get_data (G_OBJECT(widget), GRAPH_WINDOW_OBJECT_ATTACH_KEY);

      if (pdata) {
        return (GraphWindow*) pdata;
      }

      widget = gtk_widget_get_parent (widget);
    }
  }

  assert (FALSE);
  return NULL;
}

gboolean graph_window_is_visible (GraphWindow * gw)
{
  return gw && gw->window && GTK_WIDGET_VISIBLE (gw->window);
}


gboolean graph_window_is_editable (GraphWindow * gw) { return gw->editable; }

/* (physische) Breite des Zeichenfensters */
gint graph_window_get_phy_display_width (GraphWindow * gw)
{
  return gw->drawingarea->allocation.width;
}

gint graph_window_get_phy_display_height (GraphWindow * gw)
{
  return gw->drawingarea->allocation.height;
}


/* (virtueller) zeichenbereich */
gint graph_window_get_display_width (GraphWindow * gw)
{
  double zoom = graph_window_get_zoom (gw);
  return gw->drawingarea->allocation.width / zoom + 0.5;
}

/* (virtueller) zeichenbereich */
gint graph_window_get_display_height (GraphWindow * gw)
{
  double zoom = graph_window_get_zoom (gw);
  return gw->drawingarea->allocation.height / zoom + 0.5;
}

void graph_window_hide (GraphWindow * gw)
{
  if (gw->window) {
    gtk_widget_hide (gw->window);
  }
}

void graph_window_show (GraphWindow * gw)
{
  //prefs_restore_window_info (GTK_WINDOW (gw->window));
  gtk_widget_show (gw->window);
}


void graph_window_set_cursor_position (GraphWindow * gw, gint x, gint y)
{
  gchar position [30];
  sprintf (position, "%d / %d", x, y);
  gtk_label_set_text (GTK_LABEL(gw->labelPosition), position);
}

SelectionManager * graph_window_get_selection_manager (GraphWindow * gw)
{
  return gw->selManager;
}

gboolean graph_window_is_grid_active (GraphWindow * gw)
{
  return gw->useGrid;
}

/* x and y are in absolute coordinates */
void graph_window_create_node (GraphWindow * gw, int x, int y)
{
	XGraph * gr = graph_window_get_graph(gw);

    XGraphNode * node = NULL;

	/* Block 'changed' and 'layout-changed' for us. */
	//_graph_window_block_observer (gw);
	{
		node = xgraph_create_node(gr);
    	graph_window_move_node_to (gw, node, x,y);
	}
	//_graph_window_unblock_observer (gw);

	// Not implemented.
	//graph_window_display_node (gw, node);
}


void graph_window_align_to_grid (GraphWindow * gw, GdkPoint /*inout*/ * pt)
{
  pt->x = ((pt->x + gw->delta_grid / 2) / gw->delta_grid) * gw->delta_grid;
  pt->y = ((pt->y + gw->delta_grid / 2) / gw->delta_grid) * gw->delta_grid;
}


GdkPoint graph_window_mouse_to_viewport (GraphWindow * gw, gint x, gint y)
{
  GdkPoint origin = graph_window_get_visible_origin (gw);
  GdkPoint pt = {x + origin.x, y + origin.y};

  return pt;
}

/* aligns the point to grid, when needed */
/*!
 * NOTE: Notifications are not blocked by default. Calls
 *       xgraph_node_layout_set_pos(...) to change the position.
 */
void graph_window_move_node_to (GraphWindow * gw, XGraphNode * node, gint x, gint y)
{
	GdkPoint pt = { x, y };

	if (graph_window_is_grid_active(gw))
		graph_window_align_to_grid(gw, &pt);

	xgraph_node_layout_set_pos (xgraph_node_get_layout(node), pt.x, pt.y);
}


gboolean graph_window_has_selected_node (GraphWindow * gw)
{
	return gw->selectedNode != NULL;
}

void graph_window_set_selected_node (GraphWindow * gw, XGraphNode * node)
{
	//_graph_window_block_observer(gw);
	{
		if (!xgraph_node_is_helper(node))
			xgraph_node_set_selected_for_edge (node, TRUE);
		gw->selectedNode = node;
	}
	//_graph_window_unblock_observer(gw);

	// not implemented.
	//graph_window_display_node(gw, node);
}


XGraphNode * graph_window_get_selected_node (GraphWindow * gw)
{
  return gw->selectedNode;
}

void graph_window_unselect_node (GraphWindow * gw)
{
#if 1
	XGraphNode * node = gw->selectedNode;
	if (node) {

		_graph_window_block_observer(gw);
		{
			xgraph_node_set_selected_for_edge(node, FALSE);
		}
		_graph_window_unblock_observer(gw);

		graph_window_display_node(gw, node);

		gw->selectedNode = NULL;
	}
#else
  XGraphNode * node = gw->selectedNode;

  if (gw->selectedNodeState != SELECTED_SECOND)
    node->state = gw->selectedNodeState;
  else
    node->state = 0;

  graph_window_display_node (gw, gw->selectedNode);

  gw->selectedNode = NULL;
#endif
}

/* absolute coordinates */
XGraphNode * graph_window_node_from_position (GraphWindow * gw, gint x, gint y)
{
	XGraph * gr = graph_window_get_graph(gw);
	XGraphNode * ret = NULL;

	XGRAPH_FOREACH_NODE(gr,cur,iter,{
		if (node_is_over(cur, x, y)) {
			ret = cur;
			break;
		}
	});

	return ret;
}

/* absolute coordinates */
XGraphEdge * graph_window_edge_from_position (GraphWindow * gw, gint x, gint y)
{
	XGraph * gr = graph_window_get_graph(gw);
	XGraphEdge * ret = NULL;

	XGRAPH_FOREACH_EDGE(gr,cur,iter,{
		if (edge_is_over(gr, cur, x, y)) {
			ret = cur;
			break;
		}
	});

	return ret;
}

void _graph_window_update_title (GraphWindow * gw)
{
	if (graph_window_has_graph (gw)) {
		XGraph * gr = graph_window_get_graph(gw);
		gchar * headline = g_strdup_printf ("%s with %d nodes",
				xgraph_get_name(gr), xgraph_get_node_count(gr));

		gtk_window_set_title(GTK_WINDOW (gw->window), headline);
		g_free (headline);
	}
	else {
		gchar * headline = "No graph.";
		gtk_window_set_title(GTK_WINDOW (gw->window), headline);
	}
}

void graph_window_create_edge (GraphWindow * gw, XGraphNode * from, XGraphNode * to)
{
	XGraph * gr = graph_window_get_graph(gw);
	XGraphEdge * edge = NULL;
#if 1
	_graph_window_block_observer(gw);
	{
		edge = xgraph_create_edge (gr, from, to);
	}
	_graph_window_unblock_observer(gw);

	if (edge)
		graph_window_display_edge (gw, edge);

#else
    if ( graph_edge_exists (gr, from, to))
    return;
  else {
    edgelistptr el = make_edge ("", from->number, to->number, VISIBLE);
    Edge * edge = NULL, * tmpEdge;

    assert (el != NULL);

    append_edge (gr, el);
    edge = get_edge (gr, from->number, to->number);
    assert (edge != NULL);

    tmpEdge = get_edge (gr, to->number, from->number);
    if (tmpEdge) {
      intlistptr il = tmpEdge->path;
      while (il) {
        prepend_edge_pathnode (edge, il->number);
        il = il->next;
      }
    }

    graph_window_display_edge (gw, edge);
    if ( ! rel_graph_modified)
      init_rel_dir ();
    rel_graph_modified = TRUE;
    gr->state = MODIFIED;
  }
#endif
}

/*!
 * Implements the XGraphObserver.onDelete callback.
 */
void _graph_window_on_delete (GraphWindow * self, XGraph * gr)
{
	XGraphManager * manager = xgraph_manager_get_instance ();

	/* If there is another graph in the manager, show it instead. */
	if (xgraph_manager_get_graph_count(manager) > 1) {
		XGRAPH_MANAGER_FOREACH_GRAPH(manager, cur, iter, {
				if (cur != gr) {
					graph_window_set_graph (self, cur);
					break;
				}
		});
	}
	else graph_window_set_graph(self, NULL);
}

static void _graph_window_renamed (GraphWindow * self, XGraph * gr, const gchar * old_name)
{
	_graph_window_update_title (self);
}

static void _graph_window_changed (GraphWindow * self, XGraph * gr)
{

	if (self->block_observer) {  }
	else {
		graph_window_redraw(self, OLD);
		_graph_window_update_title (self);
	}
}

static void _graph_window_layout_changed (GraphWindow * self, XGraph * gr)
{
	if (! self->block_observer) {

		graph_window_redraw(self, OLD);
	}
	else {

	}
}

/*!
 * Interface for GraphObserver. Will be called, if the graph deletes one of
 * it's nodes.
 *
 * \author stb
 * \param gw The GraphWindow, which belongs the GraphObserver to.
 *           Similar to the `this` pointer in c++.
 * \param gr The Graph which called the handler.
 * \param node The node the delete.
 */
static
void _graph_window_on_delete_node (GraphWindow * gw, XGraph * gr,
                                   XGraphNode * node)
{
	//if (gw->block_observer)
		//return;
	//else {
		SelectionManager * sm = graph_window_get_selection_manager(gw);
		sm->unref(sm, node);

		if (graph_window_get_selected_node(gw) == node)
			graph_window_unselect_node(gw);

		/* unreference the node itself */
		sm->unref(sm, (Selectable) node);
	//}
}


/*!
 * Interface for GraphObserver. Will be called, if the graph deletes one of
 * it's edges.
 *
 * \author stb
 * \param gw The GraphWindow, which belongs the GraphObserver to.
 *           Similar to the `this` pointer in c++.
 * \param gr The Graph which called the handler.
 * \param edge The edge the delete.
 */
static
void _graph_window_on_delete_edge (GraphWindow * gw, XGraph * gr,
                                   XGraphEdge * edge)
{
	//if (gw->block_observer)
		//return;
	//else {
		SelectionManager * sm = graph_window_get_selection_manager(gw);
		sm->unref(sm, edge);
	//}
}


// Implements \ref MenuDomainExtractFunc. See \ref graph_window_ctor below.
static gpointer _extract_menu_domain_object (GtkMenuItem * item, gpointer dummy)
{
	return (gpointer) graph_window_get(GTK_WIDGET(item));
}


/*!
 * Called if one of the current labels is being deleted. Callback for
 * LabelObserver.onDelete(...).
 */
static void _graph_window_on_delete_label (gpointer user_data, Label * label)
{
	GraphWindow * self = (GraphWindow*) user_data;

	self->label = NULL;
	graph_window_redraw(self, OLD);
}


void graph_window_set_label (GraphWindow * self, Label * label)
{
	if (self->label) label_unregister_observer (self->label, &self->labelObserver);
	self->label = label;
	if (self->label) label_register_observer (self->label, &self->labelObserver);

	graph_window_redraw(self, OLD);
}


Label * graph_window_get_label (GraphWindow * self) { return self->label; }


static void _on_label_assocs_changed (gpointer object, Relview * rv)
{
	GraphWindow * self = (GraphWindow*) object;
	RelManager * manager = rv_get_rel_manager (rv);
	XGraph * graph = graph_window_get_graph (self);
	Label * label = NULL;

	verbose_printf (VERBOSE_TRACE, __FILE__": _on_label_assocs_changed\n");

	if (graph) {
		const gchar * name = xgraph_get_name (graph);
		Rel * rel = rel_manager_get_by_name (manager, name);

		if (rel) {
			RelationViewport * vp = relation_window_get_viewport (relation_window_get_instance ());
			LabelAssoc assoc = rv_label_assoc_get (rv, rel, vp);

			if (assoc.labels[0] == assoc.labels[1])
				label = assoc.labels[0];
		}

		graph_window_set_label (self, label);
	}
}


void graph_window_ctor (GraphWindow * gw)
{
	Relview * rv = rv_get_instance();
	GtkBuilder * builder = rv_get_gtk_builder(rv);
	Workspace * workspace = rv_get_workspace(rv);

  gw->rv = rv;

  /* Prepare the Relview observer */
  gw->rvObserver.object = gw;
  gw->rvObserver.labelAssocsChanged = _on_label_assocs_changed;
  rv_register_observer (gw->rv, &gw->rvObserver);

  gw->useGrid = FALSE;
  gw->delta_grid = 24;

  /* Create the graph window */
  gw->window = GTK_WIDGET(gtk_builder_get_object(builder, "windowGraph"));
  g_assert (gw->window);

  //gtk_window_set_opacity (GTK_WINDOW(gw->window), 0.5);

  gw->drawingarea = GTK_WIDGET(gtk_builder_get_object(builder, "drawingareaGW"));
  g_assert (gw->drawingarea);
  gw->hscroll = GTK_WIDGET(gtk_builder_get_object(builder, "hscrollbarGW"));
  g_assert (gw->hscroll);
  gw->vscroll = GTK_WIDGET(gtk_builder_get_object(builder, "vscrollbarGW"));
  g_assert (gw->vscroll);
  gw->labelPosition = GTK_WIDGET(gtk_builder_get_object(builder, "labelPositionGW"));
  g_assert (gw->labelPosition);
  gw->labelTooltip = GTK_WIDGET(gtk_builder_get_object(builder, "labelTooltipGW"));
  g_assert (gw->labelTooltip);
  gw->cbZoomLevel = GTK_WIDGET(gtk_builder_get_object(builder, "cbZoomLevelGW"));
  g_assert (gw->cbZoomLevel);

  g_assert (gw->window && gw->drawingarea && gw->hscroll && gw->vscroll
		  && gw->labelPosition && gw->labelTooltip && gw->cbZoomLevel);

  gw->editable = TRUE;
  gw->zoom = 1.0; /*100%*/
  gw->block_observer = FALSE;
  g_queue_init (&gw->history);
  graph_window_attach (gw, GTK_WINDOW(gw->window));

  /* gw->cr cannot be createf from gw->drawingarea->window here. It seems
   * that the window property was not created yet. */
  gw->cr = NULL;

  gw->backpixmap = NULL;
  gw->crOffset = NULL;

  /* Add the window to the workspace. Also restores position and
   * visibility. */
  workspace_add_window (workspace, GTK_WINDOW(gw->window), "graph-window");

  if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(gw->window))) {
	  graph_window_show (gw);
  }


  if (gw->selManager == NULL) {
    /* initialize a selection manager */

    SelectionClass nodeClass, edgeClass;

    gw->selManager = selection_manager_new ();
    gw->selManager->setData (gw->selManager, (gpointer) gw);

    nodeClass.isOver = node_sel_is_over;
    nodeClass.highlight = node_sel_highlight;
    nodeClass.unhighlight = node_sel_unhighlight;
    nodeClass.select = node_sel_select;
    nodeClass.unselect = node_sel_unselect;
    nodeClass.begin = node_sel_iter_begin;
    nodeClass.next = node_sel_iter_next;
    nodeClass.hasNext = node_sel_iter_has_next;
    nodeClass.object = node_sel_iter_object;
    nodeClass.priority = 2;
    nodeClass.multiple = TRUE;
    nodeClass.data = (void*)GRAPH_SELECTION_TYPE_NODES;

    gw->selManager->registerClass (gw->selManager, &nodeClass);

    edgeClass.isOver = edge_sel_is_over;
    edgeClass.highlight = edge_sel_highlight;
    edgeClass.unhighlight = edge_sel_unhighlight;
    edgeClass.select = edge_sel_select;
    edgeClass.unselect = edge_sel_unselect;
    edgeClass.begin = edge_sel_iter_begin;
    edgeClass.next = edge_sel_iter_next;
    edgeClass.hasNext = edge_sel_iter_has_next;
    edgeClass.object = edge_sel_iter_object;
    edgeClass.priority = 1;
    edgeClass.multiple = TRUE;
    edgeClass.data = (void*)GRAPH_SELECTION_TYPE_EDGES;

    gw->selManager->registerClass (gw->selManager, &edgeClass);
  }

  assert (NULL == gw->graph);

  memset (&gw->graphObserver, 0, sizeof (XGraphObserver));
  gw->graphObserver.object = (gpointer) gw;
  gw->graphObserver.onDeleteNode
    = (XGraphObserver_onDeleteNodeFunc) _graph_window_on_delete_node;
  gw->graphObserver.onDeleteEdge
    = (XGraphObserver_onDeleteEdgeFunc) _graph_window_on_delete_edge;
  gw->graphObserver.changed
	= (XGraphObserver_changedFunc) _graph_window_changed;
  gw->graphObserver.layoutChanged
    = (XGraphObserver_layoutChangedFunc) _graph_window_layout_changed;
  gw->graphObserver.renamed
    = (XGraphObserver_renamedFunc) _graph_window_renamed;
  gw->graphObserver.onDelete
    = (XGraphObserver_onDeleteFunc) _graph_window_on_delete;

  /* Label and LabelObserver */
  gw->label = NULL;
  gw->labelObserver.object = gw;
  gw->labelObserver.onDelete = _graph_window_on_delete_label;

  /* Register a \ref MenuDomain so e.g. plugins can add menu items to this
   * window. This is only necessary only once. */
  {
		MenuManager * mm = rv_get_menu_manager(rv);

		if (!menu_manager_get_domain(mm, GRAPH_WINDOW_MENU_PREFIX))
		{
			struct {
				char * path;
				char * builder_name;
			} *p, subs[] = { {"drawings", "menuitemGraphDrawingGW"},
							 {"actions", "menuitemActionsGW"},
							 {0}};

			gw->menu_domain = menu_domain_new(GRAPH_WINDOW_MENU_PREFIX,
					_extract_menu_domain_object);

			for (p = subs ; p->path != NULL ; ++p) {
				GtkWidget * menu = GTK_WIDGET(gtk_builder_get_object(builder, p->builder_name));
				g_assert (menu != NULL);
				menu_domain_add_subdomain(gw->menu_domain, p->path,
						GTK_MENU_SHELL(gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu))));
			}

			menu_manager_add_domain(mm, gw->menu_domain);
		}

#if 0
		printf ("Adding some random menu entries ...\n");
		{
			MenuEntry * b;
			MenuEntry * a;
			MenuDomain * dom;

			b = menu_entry_new ("B", _b_is_en, _b_on_act, (gpointer)0x456);
			dom = menu_manager_get_domain (mm, "/graph-window");
			g_assert (dom != NULL);
			menu_domain_add (dom, "drawings", b);

			a = menu_entry_new ("A", _a_is_en, _a_on_act, b);
			dom = menu_manager_get_domain (mm, "/graph-window");
			g_assert (dom != NULL);
			menu_domain_add (dom, "actions", a);
		}
#endif
	}
}

/*! */
void graph_window_fit_zoom_to_graph(GraphWindow * gw) {
	if (!graph_window_has_graph(gw)) {
		return;
	}
	else {
		XGraph * gr = graph_window_get_graph(gw);
		gint left, top, right, bottom;
		gint graph_width, graph_height;
		double zoom_x, zoom_y;

		/* it's not the real width and height. Actually it's the position
		 * of the rightmost and bottommost node. */
		xgraph_get_display_extent(gr, &left, &top, &right, &bottom);
		graph_width = right - left;
		graph_height = bottom - top;

		zoom_x = graph_window_get_phy_display_width(gw)
				/ (double) (graph_width);
		zoom_y = graph_window_get_phy_display_height(gw)
				/ (double) (graph_height);

		/* do not zoom in! */
		{
			double zoom = MIN(zoom_x, zoom_y);
			if (zoom >= 1)
				graph_window_reset_zoom(gw);
			else
				graph_window_set_zoom(gw, zoom);
		}
	}
}

void graph_window_set_zoom (GraphWindow * gw, double zoom)
{
  gw->zoom = zoom;
}

double graph_window_get_zoom (GraphWindow * gw) { return gw->zoom; }

void graph_window_reset_zoom (GraphWindow * gw)
{
  gtk_combo_box_set_active (GTK_COMBO_BOX (gw->cbZoomLevel), 0 /*index*/);
}


GraphWindow * g_graphWindow = NULL;


/*! Create and/or return the global GraphWindow instance. The singleton
 * replaces the global variable graph and all other associated global
 * variables. They are available through the GraphWindow object now.
 *
 * \author stb
 * \return Returns a GraphWindow instance.
 */
GraphWindow * graph_window_get_instance ()
{
  if (NULL ==  g_graphWindow) {
    g_graphWindow =  graph_window_new ();
    graph_window_ctor (g_graphWindow);
  }
  return g_graphWindow;
}

/*! Destroys the global GraphWindow instance, if there is one.
 *
 * \author stb
 */
void graph_window_destroy_instance ()
{
  if (g_graphWindow) {
    graph_window_destroy (g_graphWindow);
    g_graphWindow = NULL;
  }
}
