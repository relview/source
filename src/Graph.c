/*
 * Graph.c
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

#include <gtk/gtk.h>
#include "Relation.h"
#include "RelationProxyAdapter.h"
#include "graph.h"
#include "Observer.h"
#include "prefs.h" // for rel_allow_display

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <assert.h>

LayoutPoint * layout_point_new (gfloat x, gfloat y)
{
	LayoutPoint * self = g_new0(LayoutPoint,1);
	self->x = x;
	self->y = y;
	return self;
}

LayoutPoint * layout_point_copy (const LayoutPoint * orig)
{ return layout_point_new (orig->x, orig->y); }


/* ------------------------------------------------------------------------- */

#include "Graph.h"
#include "GraphImpl.h"

static void _xgraph_node_layout_copy (XGraphNodeLayout * dst, const XGraphNodeLayout * src);
static XGraphNode * _xgraph_node_copy (XGraphNode * self);
static void _xgraph_node_dtor (XGraphNode * self);
static void _xgraph_edge_dtor (XGraphEdge * self);
static void _xgraph_edge_layout_dtor (XGraphEdgeLayout * self);
XGraphNode * _xgraph_node_ctor (XGraph * gr);
XGraphNode * _xgraph_node_ctor_gui (XGraph * gr);
gint _xgraph_next_internal_id (XGraph * self);
void _xgraph_node_layout_init (XGraphNodeLayout * self, XGraphNode * node);
gchar * _xgraph_get_next_name (XGraph * self);
void _xgraph_dtor (XGraph * self);
XGraphEdge * _xgraph_edge_ctor (XGraphNode * from, XGraphNode * to);
void _xgraph_edge_layout_init (XGraphEdgeLayout * self, XGraphEdge * edge);
void _xgraph_reassign_names_beginning_with_id (XGraph * self, gint id);
static gboolean _xgraph_node_id_data_cmp (const XGraphNode * a,
		const XGraphNode * b, gpointer data);
static void _xgraph_node_set_name_s (XGraphNode * self, const gchar * new_name);
static gboolean _xgraph_builder_build_edge (GraphBuilder * builder, gint fromId, gint toId);
static gboolean _xgraph_builder_build_node (GraphBuilder * builder, gint id);
static void _xgraph_builder_destroy (GraphBuilder * builder);
static void _xgraph_set_manager (XGraph * self, XGraphManager * manager);

static XGraphEdge * _xgraph_edge_copy (XGraphEdge * self);
static XGraphEdgeShared * _xgraph_edge_shared_ctor (XGraphEdgeShared * self);
static void _xgraph_edge_shared_dtor (XGraphEdgeShared * self);
static XGraphEdgeShared * _xgraph_edge_shared_new ();
static void _xgraph_edge_shared_destroy (XGraphEdgeShared * self);
static XGraphEdgeShared * _xgraph_edge_shared_copy (const XGraphEdgeShared * self);

gboolean xgraph_node_is_helper (XGraphNode * self) { return FALSE; }

/*!
 * Creates a new graph with the given name. The new graph doesn't contain
 * nodes or edges.
 */
#define _xgraph_ctor(name) xgraph_new(name)

gdouble layout_point_distance (const LayoutPoint * a, const LayoutPoint * b)
{ return sqrt (pow(a->x - b->x,2) + pow(a->y - b->y,2)); }

gboolean layout_point_equal (LayoutPoint a, LayoutPoint b)
{ return a.x == b.x && a.y == b.y; }

/*******************************************************************************
 *                                Graph Manager                                *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

XGraphManager * g_graph_manager_singleton = NULL;

#define _NS(name) xgraph_##name
#define _priv_NS(name) _xgraph_##name


XGraphManager * xgraph_manager_get_instance ()
{
	if (! g_graph_manager_singleton)
		g_graph_manager_singleton = xgraph_manager_new ();
	return g_graph_manager_singleton;
}

/*!
 * Is called, if a graph in the manager changed its name.
 */
static void _xgraph_manager_on_renamed (XGraphManager * self, XGraph * gr, const gchar * old_name)
{
	const gchar * new_name = xgraph_get_name(gr);
	gpointer orig_key = NULL, value = NULL;

	/* We have to update our entry in the hash table to the new name. We
	 * suppose that the key doesn't already exists in the hash table. */
	if (! g_hash_table_lookup_extended (self->graphs, old_name, &orig_key, &value)) {
		g_warning ("_xgraph_manager_on_renamed: Missing graph \"%s\" "
				"in manager.\n", old_name);
	}
	else {
		/* We may not remove the entry. This would cause the hash table to
		 * destroy the graph. Thus, we first have to steal and to reinsert
		 * it. */
		g_assert ((gpointer)gr == value);

		g_hash_table_steal (self->graphs, old_name);
		g_free (orig_key);

		g_assert (NULL == g_hash_table_lookup(self->graphs, new_name));

		g_hash_table_insert (self->graphs, g_strdup(new_name), gr);
	}
}

/*!
 * Is called, if a graph is going to be deleted.
 */
static void _xgraph_manager_on_delete (XGraphManager * self, XGraph * gr)
{
	/* Nothing to do. */
}

XGraphManager * xgraph_manager_new ()
{
	XGraphManager * self = g_new0 (XGraphManager, 1);
	self->block_notify = 0;
	self->graphs = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify) _xgraph_dtor);
	self->observers = NULL;

	/* Set the callbacks for the XGraphObserver */
	self->graph_observer.renamed
		= (XGraphObserver_renamedFunc) _xgraph_manager_on_renamed;
	self->graph_observer.onDelete
		= (XGraphObserver_onDeleteFunc) _xgraph_manager_on_delete;

	return self;
}

void xgraph_manager_destroy (XGraphManager * self)
{
	XGRAPH_MANAGER_OBSERVER_NOTIFY(self,onDelete,_0());

	/* The graphs itself are destroyed by the hash table. */

	g_hash_table_destroy(self->graphs);
	g_slist_free(self->observers);

	g_free (self);
}

/*!
 * Removes the graph from the manager without destroying it. If all goes right,
 * then the node does not belong to any manager anymore.
 */
void xgraph_manager_steal (XGraphManager * self, XGraph * gr)
{
	gpointer orig_key;

	g_return_if_fail(self == xgraph_get_manager(gr));
	gr->manager = NULL;

	g_hash_table_lookup_extended(self->graphs, xgraph_get_name(gr), &orig_key, NULL);
	g_hash_table_steal (self->graphs, orig_key);
	g_free (orig_key);

	gr->manager = NULL;

	xgraph_manager_changed (self);

	g_assert (! g_hash_table_lookup (self->graphs, xgraph_get_name(gr)));
}

/*!
 * Deletes the graph with the given name, or does nothing, if there is no
 * such graph.
 */
void xgraph_manager_delete_by_name (XGraphManager * self, const gchar * name)
{ xgraph_destroy (xgraph_manager_get_by_name(self,name)); }

/*!
 * Steals all graphs from victim whose names are not already appear in the
 * current manager.
 *
 * Emits 'changed'.
 */
void xgraph_manager_steal_all_from (XGraphManager * self, XGraphManager * victim)
{
	GList/*<const gchar*>*/ *names = xgraph_manager_get_names (victim), *iter;

	xgraph_manager_block_notify(self);
	xgraph_manager_block_notify(victim);

	for (iter = names ; iter ; iter = iter->next) {
		const gchar * name = (const gchar*) iter->data;
		if (! xgraph_manager_exists (self, name)) {
			XGraph * gr = xgraph_manager_get_by_name (victim, name);
			xgraph_manager_steal (victim, gr);
			xgraph_manager_insert (self, gr);
		}
	}

	xgraph_manager_unblock_notify(victim);
	xgraph_manager_unblock_notify(self);

	xgraph_manager_changed(victim);
	xgraph_manager_changed(self);

	g_list_free(names);
}

/*!
 * Similar to \ref xgraph_manager_steal_all_from. Graphs in the current manager
 * are destroyed if a graph in victim with the same name exists.
 *
 * Emits 'changed'.
 */
void xgraph_manager_steal_all_from_and_overwrite (XGraphManager * self, XGraphManager * victim)
{
	GList/*<const gchar*>*/ *names = xgraph_manager_get_names (victim), *iter;

	xgraph_manager_block_notify(self);
	xgraph_manager_block_notify(victim);

	for (iter = names ; iter ; iter = iter->next) {
		const gchar * name = (const gchar*) iter->data;
		XGraph * gr = xgraph_manager_get_by_name (victim, name);

		xgraph_manager_delete_by_name (self,name);
		xgraph_manager_steal (victim, gr);
		xgraph_manager_insert (self, gr);
	}

	xgraph_manager_unblock_notify(victim);
	xgraph_manager_unblock_notify(self);

	xgraph_manager_changed(victim);
	xgraph_manager_changed(self);

	g_list_free(names);
}

XGraph * _NS(manager_get_by_name) (XGraphManager * self, const gchar * name)
{
	return (XGraph*) g_hash_table_lookup(self->graphs, name);
}

gboolean _NS(manager_exists) (XGraphManager * self, const gchar * name)
{
	return _NS(manager_get_by_name) (self,name) != NULL;
}

/*!
 * Inserts a graph into the given manager. Insertion only works, if the
 * given graph does not belong to another graph manager. In the latter case,
 * one has to use xgraph_manager_transfer.
 *
 * The graph is not inserted, if a name with the given name already exists.
 *
 * \return Returns TRUE, if the graph was inserted and FALSE otherwise.
 */
gboolean _NS(manager_insert) (XGraphManager * self, XGraph * graph)
{
	if (xgraph_get_manager(graph) != NULL) return FALSE;
	else {
		const gchar * name = _NS(get_name)(graph);

		if (_NS(manager_exists (self, name)))
			return FALSE;
		else {
			g_hash_table_insert(self->graphs, g_strdup(name), graph);
			_xgraph_set_manager (graph, self);

			xgraph_manager_changed (self);
			return TRUE;
		}
	}
}

XGraphIterator* xgraph_manager_iterator (XGraphManager * self)
{
	XGraphIterator * iter = g_new0 (XGraphIterator, 1);
	g_hash_table_iter_init (&iter->iter, self->graphs);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}

/*!
 * Creates a new graph with the given name. If a graph with the given name
 * already exists, NULL is returned.
 */
XGraph * _NS(manager_create_graph) (XGraphManager * self, const gchar * name)
{
	if (xgraph_manager_exists(self, name))
		return NULL;
	else {
		XGraph * gr = _xgraph_ctor (name);
		assert (xgraph_manager_insert (self, gr));
		return gr;
	}
}

void _NS(manager_register_observer) (XGraphManager * self, XGraphManagerObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void _NS(manager_unregister_observer) (XGraphManager * self, const XGraphManagerObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }

gboolean xgraph_manager_is_empty (XGraphManager * self)
{ return xgraph_manager_get_graph_count(self) == 0; }

gint xgraph_manager_get_graph_count (XGraphManager * self)
{
	return g_hash_table_size (self->graphs);
}

/*!
 * Returns the names of all graphs in the manager.
 */
GList/*<const gchar*>*/ * xgraph_manager_get_names (XGraphManager * self)
{ return g_hash_table_get_keys (self->graphs); }

void xgraph_manager_changed (XGraphManager * self)
{
	if (self->block_notify <= 0)
		XGRAPH_MANAGER_OBSERVER_NOTIFY(self,changed,_0());
}

void xgraph_manager_block_notify (XGraphManager * self) { self->block_notify ++; }
void xgraph_manager_unblock_notify (XGraphManager * self)
{ self->block_notify = MAX(0, self->block_notify-1); }

/*******************************************************************************
 *                                    Graph                                    *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

void _xgraph_dtor (XGraph * self)
{
	xgraph_clear_s(self);

	g_hash_table_destroy (self->edges);
	g_hash_table_destroy (self->nodes);

	g_free (self->name);
	g_slist_free (self->observers);
}

void xgraph_iterator_next (XGraphIterator * self)
{ self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }

gboolean xgraph_iterator_is_valid (XGraphIterator * self) { return self->is_valid; }
XGraph * xgraph_iterator_get (XGraphIterator * self) { return self->cur; }
void xgraph_iterator_destroy (XGraphIterator * self) { g_free (self); }


/*!
 * Creates a hash key for an array of two integer values, i.e. gint[2].
 */
guint _hash_gint_2 (const gint * pair)
{
	switch (sizeof(gint)) {
	case 4:
		return (pair[0] & 0xffff) | ((pair[1] & 0xffff) << 16);
	case 8: /* fall through */
	default:
		return pair[0] + pair[1];
	}
}

gboolean _is_gint_2_equal (const gint * a, const gint * b)
{
	return a[0] == b[0] && a[1] == b[1];
}

/*!
 * If name is NULL, "" is used instead without notification.
 */
XGraph * xgraph_new (const gchar * name)
{
	XGraph * self = g_new0 (XGraph, 1);
	self->manager = NULL;
	self->name    = g_strdup (name ? name : "");
	self->nodes   = g_hash_table_new_full (g_direct_hash, g_direct_equal,
			NULL, (GDestroyNotify)_xgraph_node_dtor);
	self->edges   = g_hash_table_new_full ((GHashFunc)_hash_gint_2,
			(GEqualFunc)_is_gint_2_equal,
			(GDestroyNotify)g_free,
			(GDestroyNotify)_xgraph_edge_dtor);
	self->is_hidden = FALSE;
	self->observers = NULL;
	self->next_internal_id = 1;
	self->block_notify = 0;

	self->layout.is_visible = TRUE;
	return self;
}

/*!
 * Destroys the current graphs, removes it from it's manager (if their is one)
 * and notifies all necessary observers. Remark that the manager may also
 * notify his observers.
 */
void xgraph_destroy (XGraph * self)
{
	if (self) {
		XGraphManager * manager = xgraph_get_manager(self);

		XGRAPH_OBSERVER_NOTIFY(self,onDelete,_0());

		if (manager)
			xgraph_manager_steal (manager, self);
		_xgraph_dtor (self);
		g_free (self);
	}
}

/*!
 * Renames the given graph. This will fail if another graph inside the graph's
 * manager already has the given name. The graph itself will notify all its
 * observers. The manager may also notify its observers. The operation succeeds,
 * if the current and the new name coincide.
 */
gboolean xgraph_rename (XGraph * self, const gchar * new_name)
{
	if (g_str_equal(xgraph_get_name(self), new_name)) return TRUE;
	else {
		XGraphManager * manager = xgraph_get_manager (self);
		if (manager && xgraph_manager_exists(manager, new_name)) {
			return FALSE;
		}
		else {
			gchar * old_name = self->name;

			self->name = g_strdup (new_name);

			XGRAPH_OBSERVER_NOTIFY(self, renamed, _1(old_name));
			g_free (old_name);
			return TRUE;
		}
	}
}

static void _xgraph_layout_dtor (XGraphLayout * self) { }

static void _xgraph_layout_copy (XGraphLayout * dst, const XGraphLayout * src)
{ *dst = *src; }

static void _xgraph_layout_assign (XGraphLayout * self, const XGraphLayout * src)
{
	_xgraph_layout_dtor (self);
	_xgraph_layout_copy (self, src);
}

static void _xgraph_edge_get_hash_key (XGraphEdge * self, gint * key)
{
	key[0] = xgraph_edge_get_from_id (self);
	key[1] = xgraph_edge_get_to_id (self);
}

static void _xgraph_edge_layout_copy (XGraphEdgeLayout * dst, const XGraphEdgeLayout * src);


/*!
 * After the call, the edge refers to the same nodes, as the original. Supposes
 * node to have same IDs in both graphs.
 */
XGraphEdge * _xgraph_edge_copy_to (const XGraphEdge * self, XGraph * gr)
{
	XGraphEdge * copy = g_new0 (XGraphEdge, 1);
	XGraphEdge * way_back = NULL;
	copy->is_selected      = copy->is_selected;

	copy->is_highlighted   = self->is_highlighted;
	copy->is_marked_first  = self->is_marked_first;
	copy->is_marked_second = self->is_marked_second;
	copy->is_visible       = self->is_visible;

	copy->from             = xgraph_get_node_by_id (gr, xgraph_node_get_id(self->from));
	copy->to               = xgraph_get_node_by_id (gr, xgraph_node_get_id(self->to));

	xgraph_edge_path_assign (&copy->path, &self->path);

	if (self->way_back && (way_back = xgraph_get_edge(gr, copy->to, copy->from))) {
		copy->shared = way_back->shared;
		way_back->way_back = copy;
		copy->way_back = way_back;
	}
	else {
		copy->shared = _xgraph_edge_shared_copy (copy->shared);
	}

	{
		gint * key = g_new(gint,2);
		_xgraph_edge_get_hash_key (copy, key);
		g_hash_table_insert (gr->edges, key, copy);
	}

	return copy;
}

/*!
 * Creates a copy of the given graph in the current manager (or NULL) with the
 * given name. If their is a manager and the name is already in use, NULL is
 * returned. The copy can have the same name, if the given graph does not
 * belong to a graph manager.
 *
 * \note Neither blocks for signals, not observers are copied.
 */
XGraph * xgraph_copy (XGraph * self, const gchar * copy_name)
{
	XGraphManager * manager = xgraph_get_manager(self);
	if (manager != NULL && xgraph_manager_exists (manager, copy_name)){
		printf("FUUUU\n");

		return NULL;
	} else {
		XGraph * copy = xgraph_new (copy_name);

		copy->manager   = NULL; /* temporarily */
		copy->is_hidden = self->is_hidden;
		copy->observers = NULL;
		_xgraph_layout_assign (&copy->layout, &self->layout);
		copy->block_notify = 0;

		/* Copy the nodes. */
		XGRAPH_FOREACH_NODE(self, node, iter, {
				XGraphNode * copy_node = _xgraph_node_copy (node);
				g_hash_table_insert (copy->nodes, GINT_TO_POINTER(copy_node->id), copy_node);
		});

		/* Copy the edges. */
		XGRAPH_FOREACH_EDGE(self, edge, iter, {
				_xgraph_edge_copy_to (edge, copy);
		});

		return copy;
	}
}


/*!
 * If the graph is empty, i.e. it has no nodes, 0 is returned for each value.
 */
void xgraph_get_display_extent (XGraph * self, gint *pleft, gint * ptop,
		gint * pright, gint * pbottom)
{
	/* We have to consider the nodes, as well as the edge layouts. */
	gint left, top, right, bottom;

	if (xgraph_is_empty(self)) {
		left   = 0;
		top    = 0;
		right  = 0;
		bottom = 0;
	}
	else {
		left   = G_MAXINT;
		top    = G_MAXINT;
		right  = G_MININT;
		bottom = G_MININT;

		XGRAPH_FOREACH_NODE(self,node,iter,{
				XGraphNodeLayout * layout = xgraph_node_get_layout(node);
				left   = MIN(left, layout->x - layout->dim);
				top    = MIN(top, layout->y - layout->dim);
				right  = MAX(right, layout->x + layout->dim);
				bottom = MAX(bottom, layout->y + layout->dim);
		});


		XGRAPH_FOREACH_EDGE(self,edge,iter,{
			LayoutRect rc = xgraph_edge_layout_get_bounding_box (xgraph_edge_get_layout(edge));
			left   = MIN(left, rc.left);
			top    = MIN(top, rc.top);
			right  = MAX(right, rc.right);
			bottom = MAX(bottom, rc.bottom);
		});
	}

	g_assert (left <= right && top <= bottom);

	if (pleft) *pleft = left;
	if (ptop) *ptop = top;
	if (pright) *pright = right;
	if (pbottom) *pbottom = bottom;

}

/*!
 * See \ref xgraph_get_display_extent. If you need the display width as well as
 * the display height, use \ref xgraph_get_display_extent directly. Otherwise
 * use do a lot of stuff twice.
 */
gint xgraph_get_display_left (XGraph * self)
{
	gint left;
	xgraph_get_display_extent (self, &left, NULL, NULL, NULL);
	return left;
}

/*!
 * \see xgraph_get_display_left.
 */
gint xgraph_get_display_top (XGraph * self)
{
	gint top;
	xgraph_get_display_extent (self, NULL, &top, NULL, NULL);
	return top;
}

/*!
 * \see xgraph_get_display_left.
 */
gint xgraph_get_display_right (XGraph * self)
{
	gint right;
	xgraph_get_display_extent (self, NULL, NULL, &right, NULL);
	return right;
}

/*!
 * \see xgraph_get_display_left.
 */
gint xgraph_get_display_bottom (XGraph * self)
{
	gint bottom;
	xgraph_get_display_extent (self, NULL, NULL, NULL, &bottom);
	return bottom;
}

GdkRectangle xgraph_get_display_rect (XGraph * self)
{
	gint left,top,right,bottom;
	GdkRectangle rc;
	xgraph_get_display_extent (self, &left,&top,&right,&bottom);
	rc.x      = left;
	rc.y      = top;
	rc.width  = right - left;
	rc.height = bottom - top;

	g_assert (rc.width >= 0 && rc.height >= 0);
	return rc;
}

/*!
 * Returns the graph's manager. May be NULL.
 */
XGraphManager * xgraph_get_manager (XGraph * self) { return self->manager; }

/*!
 * Returns the number of nodes. May be 0.
 */
gint xgraph_get_node_count (XGraph * self) { return g_hash_table_size(self->nodes); }

const gchar * xgraph_get_name (XGraph * self) { return self->name; }

/*!
 * Returns the node with the given internal ID of NULL if no node with that
 * ID exists.
 */
XGraphNode * xgraph_get_node_by_id (XGraph * self, gint nodeId)
{
	return g_hash_table_lookup (self->nodes, GINT_TO_POINTER(nodeId));
}

/*!
 * Returns true, if the graph is empty, i.e. the graph has no nodes.
 */
gboolean xgraph_is_empty (XGraph * self) { return xgraph_get_node_count(self) == 0; }


/*!
 * Registers the given observer. The function won't register an observer
 * twice.
 */
void xgraph_register_observer (XGraph * self, XGraphObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void xgraph_unregister_observer (XGraph * self, const XGraphObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }

XGraphEdge * xgraph_get_edge_by_id (XGraph * self, gint fromId, gint toId)
{
	gint key[2] = { fromId, toId };
	return g_hash_table_lookup (self->edges, key);
}

XGraphEdge * xgraph_get_edge (XGraph * self, const XGraphNode * from, const XGraphNode * to)
{ return xgraph_get_edge_by_id (self, from->id, to->id); }

gboolean xgraph_has_edge_by_id (XGraph * self, gint fromId, gint toId)
{ return xgraph_get_edge_by_id(self, fromId, toId) != NULL; }

gboolean xgraph_has_edge (XGraph * self, const XGraphNode * from, const XGraphNode * to)
{ return xgraph_get_edge (self, from, to) != NULL; }


/*!
 * Returns the node with the given "name". I.e. the number which is displayed
 * to the user on screen. This may and usually is different from the internal
 * ID.
 *
 * \remarks This operation is slow. It traverses through all nodes.
 */
XGraphNode * xgraph_get_node_by_name (XGraph * self, gint name)
{
	XGraphNode * res = NULL;

	XGRAPH_FOREACH_NODE(self,node,iter,{
			if (atoi(node->name) == name) {
				res = node;
				break;
			}
	});
	return res;
}

gboolean xgraph_is_correspondence (XGraph * self) { return FALSE; }

void xgraph_clear_edges (XGraph * self) { g_warning ("xgraph_clear_edges: NOT IMPLEMENTED!\n"); }

void xgraph_unmark_edges (XGraph * self)
{
	xgraph_block_notify(self);
	XGRAPH_FOREACH_EDGE(self, edge, iter, {
		XGraphEdgeLayout * layout = xgraph_edge_get_layout(edge);
		xgraph_edge_layout_set_marked_first  (layout, FALSE);
		xgraph_edge_layout_set_marked_second (layout, FALSE);
	});
	xgraph_unblock_notify(self);
	xgraph_layout_changed(self);
}

void xgraph_unmark_nodes (XGraph * self)
{
	xgraph_block_notify(self);
	XGRAPH_FOREACH_NODE(self, node, iter, {
		XGraphNodeLayout * layout = xgraph_node_get_layout(node);
		xgraph_node_layout_set_marked_first  (layout, FALSE);
		xgraph_node_layout_set_marked_second (layout, FALSE);
	});
	xgraph_unblock_notify(self);
	xgraph_layout_changed(self);
}

/*!
 * Throws 'onDeleteEdge'. Returns TRUE on success.
 *
 * \see xgraph_delete_edge_s
 */
gboolean xgraph_delete_edge_s (XGraph * self, XGraphEdge * edge)
{
	if (xgraph_edge_get_graph(edge) != self)
		return FALSE;
	else {
		gint key[2];
		_xgraph_edge_get_hash_key(edge,key);

		g_assert (g_hash_table_lookup (self->edges, (gpointer) key));

		/* The rest is done by the hash table destroy notifiers. */
		g_hash_table_remove(self->edges, (gpointer) key);

		return TRUE;
	}
}

/*!
 * Delete the current edge. 'onDeleteEdge' and 'changed' are thrown.
 * Return TRUE, if the edges was deleted.
 */
gboolean xgraph_delete_edge (XGraph * self, XGraphEdge * edge)
{
	if (xgraph_delete_edge_s (self, edge)) {
		xgraph_changed (self);
		return TRUE;
	}
	return FALSE;
}

/*!
 * Deletes all incident edges of the given nodes. onDeleteEdge is thrown.
 */
void xgraph_delete_incident_edges_s (XGraph * self, XGraphNode * node)
{
	/* Two options:
	 *   1) Iterate over all edges, or
	 *   2) test for all nodes v, if (node,v) and/or (v,node) exists.
	 *
	 * The first is worse in the worst-case, so we choose the second. */

	XGRAPH_FOREACH_NODE(self,cur,iter,{
		XGraphEdge * edge = xgraph_get_edge(self,node,cur); /* node->cur? */
		if (edge) xgraph_delete_edge_s(self, edge);
		edge = xgraph_get_edge(self,cur,node); /* cur->node? */
		if (edge) xgraph_delete_edge_s(self, edge);
	});
}

void xgraph_delete_incident_edges (XGraph * self, XGraphNode * node)
{
	xgraph_delete_incident_edges (self, node);
	xgraph_changed (self);
}


/*!
 * Emits nothing.
 */
void _xgraph_reassign_names_beginning_with_id_s (XGraph * self, gint id)
{
	gint nextNumber = 0;
	GSequence * seq = g_sequence_new (NULL);
	GSequenceIter * iter = NULL;

	/* Node order in a traversal is arbitrary. To maintain our constraint
	 * ID_1 > ID_2 => Name_1 > Name_2, we first have to sort the nodes. */
	XGRAPH_FOREACH_NODE(self,cur,iter,{
		gint curId = xgraph_node_get_id (cur);

		if (curId < id)
			nextNumber = MAX(nextNumber, atoi(xgraph_node_get_name(cur)));
		else {
			g_assert (curId > id);
			g_sequence_insert_sorted (seq, cur,
					(GCompareDataFunc)_xgraph_node_id_data_cmp, NULL);
		}
	});

	/* Now reassign the names beginning with nextNumber+1. */
	nextNumber ++;

	iter = g_sequence_get_begin_iter (seq);
	while (! g_sequence_iter_is_end(iter)) {
		XGraphNode * cur = (XGraphNode*) g_sequence_get(iter);
		gchar * new_name = g_strdup_printf ("%d", nextNumber);
		_xgraph_node_set_name_s (cur, new_name);
		g_free (new_name);
		iter = g_sequence_iter_next(iter);
		nextNumber ++;
	}

	g_sequence_free (seq);
}


/*!
 * \see xgraph_delete_node
 *
 * \note Only suppresses the 'changed' notifications!
 */
void xgraph_delete_node_s (XGraph * self, XGraphNode * node)
{
	gint id = xgraph_node_get_id(node);

	xgraph_delete_incident_edges_s (self, node);

	g_hash_table_remove(self->nodes, GINT_TO_POINTER(id));

	/* We now have to reassign the nodes names for nodes with a higher
	 * internal id than our node. ID_1 > ID_2 => Name_1 > Name_2 */
	_xgraph_reassign_names_beginning_with_id_s (self, id);
}

/*!
 * Delete the current node. 'onDeleteNode' and 'changed' are thrown.
 * If the node has incident edges, 'onDeleteEdge' is thrown as well.
 */
void xgraph_delete_node (XGraph * self, XGraphNode * node)
{
	XGRAPH_OBSERVER_NOTIFY(self,onDeleteNode,_1(node));
	xgraph_delete_node_s (self, node);
	XGRAPH_OBSERVER_NOTIFY(self,changed,_0());
}

/*!
 * Create a node and return it.
 */
XGraphNode * xgraph_create_node_s (XGraph * self)
{
	XGraphNode * node = _xgraph_node_ctor (self);
	g_hash_table_insert (self->nodes, GINT_TO_POINTER(node->id), node);
	return node;
}

/*!
 * Create a node and return it. Calls changed notifier for the graph.
 */
XGraphNode * xgraph_create_node (XGraph * self)
{
	XGraphNode * node = xgraph_create_node_s (self);

	xgraph_changed(self);
	return node;
}

/*!
 * Blocks the notification of 'changed' and 'layout-changed'.
 */
void xgraph_block_notify (XGraph * self) { self->block_notify ++; }
void xgraph_unblock_notify (XGraph * self) { self->block_notify = MAX(0,self->block_notify-1); }

void xgraph_layout_changed (XGraph * self)
{
	if (self->block_notify <= 0) {
		XGRAPH_OBSERVER_NOTIFY(self,layoutChanged,_0());
	}
}
void xgraph_changed (XGraph * self)
{
	if (self->block_notify <= 0) {
		XGRAPH_OBSERVER_NOTIFY(self,changed,_0());
	}
}

void xgraph_reset_layout (XGraph * self)
{
	xgraph_block_notify(self);
	XGRAPH_FOREACH_EDGE(self, edge, iter, {
		xgraph_edge_layout_reset(xgraph_edge_get_layout(edge));
	});
	xgraph_unblock_notify(self);
	xgraph_layout_changed(self);
}

/**
 * Mark the given edges from a relation.
 */
void xgraph_mark_edges_by_impl (XGraph * self, const KureRel * impl, gint level)
{
	if (level < 1 || level > 2)
		g_warning ("xgraph_mark_edges_by_rel: Wrong level. 1 or 2 expected.");
	else {
		int n = xgraph_get_node_count(self);
		int rows, cols;
		void (*mark_func) (gpointer,gboolean)
				= ((level == 1) ? xgraph_edge_layout_set_marked_first
				: xgraph_edge_layout_set_marked_second);

		/* Graphs aren't that huge, so this is okay in practice. */

		if ( !kure_rel_rows_fits_si(impl)) rows = INT_MAX;
		else rows = kure_rel_get_rows_si(impl);

		if ( !kure_rel_cols_fits_si(impl)) cols = INT_MAX;
		else cols = kure_rel_get_cols_si(impl);

		xgraph_block_notify(self);

		XGRAPH_FOREACH_EDGE(self, edge, iter, {
			XGraphNode *from = xgraph_edge_get_from_node(edge);
			XGraphNode *to   = xgraph_edge_get_to_node(edge);

			/* Remember: The proxy is 0-indexed while the node's names are
			 * are 1-indexed. */
			int i = atoi(xgraph_node_get_name(from)) - 1;
			int j = atoi(xgraph_node_get_name(to)) - 1;

			if (i < rows && j < cols)
				mark_func (xgraph_edge_get_layout(edge), kure_get_bit_si(impl,i,j,NULL));
		});

		xgraph_unblock_notify(self);
		xgraph_layout_changed(self);
	}
}

#if 0
/* The given relation doesn't need to have the same dimension than the
 * given graph. */
void xgraph_mark_edges_by_rel (XGraph * self, const gchar * relName, gint level)
{
	RelManager * manager = rel_manager_get_instance();
	Rel * rel = rel_manager_get_by_name(manager, relName);
	if (! rel)
		g_warning ("xgraph_mark_edges_by_rel: Rel. \"%s\" doesn't exist "
				"in the global manager.", relName);
	else xgraph_mark_edges_by_impl (self, rel_get_impl(rel), level);
}
#endif

/* Note: Only the first column is used! This could cause problems if the
 *       given relation is not a vector. To test a relation to be a vector is
 *       an expensive operation. */
void xgraph_mark_nodes_by_impl (XGraph * self, const KureRel * impl, gint level)
{
	if (level < 1 || level > 2)
		g_warning ("xgraph_mark_nodes_by_rel: Wrong level. 1 or 2 expected.");
	else {
		int n = xgraph_get_node_count(self);
		int rows;
		void (*mark_func) (XGraphNodeLayout*,gboolean)
				= ((level == 1) ? xgraph_node_layout_set_marked_first
				: xgraph_node_layout_set_marked_second);

		/* Graphs aren't that huge, so this is okay in practice. */

		if ( !kure_rel_rows_fits_si(impl)) rows = INT_MAX;
		else rows = kure_rel_get_rows_si(impl);

		xgraph_block_notify(self);

		XGRAPH_FOREACH_NODE(self, node, iter, {
			/* Remember: The proxy is 0-indexed while the node's names are
			 * are 1-indexed. */
			int i = atoi(xgraph_node_get_name(node)) - 1;

			if (i < rows)
				mark_func (xgraph_node_get_layout(node), kure_get_bit_si(impl,i,0,NULL));
		});

		xgraph_unblock_notify(self);
		xgraph_layout_changed(self);
	}
}

gint _xgraph_next_internal_id (XGraph * self) { return self->next_internal_id ++; }

/*!
 * Returns the next ID (as a string) for a new node which shall be displayed
 * on the screen.
 */
gchar * _xgraph_get_next_name (XGraph * self)
{
	return g_strdup_printf ("%d", xgraph_get_node_count(self) + 1);
}
/*!
 * Create a new edge from->to and returns it. Returns NULL in case of an
 * error.
 */
XGraphEdge * xgraph_create_edge_s (XGraph * self, XGraphNode * from, XGraphNode * to)
{
	if (self != xgraph_node_get_graph(from))
		return NULL;

	else {
		XGraphEdge * edge = _xgraph_edge_ctor(from,to);
		gint * key = g_new(gint,2);
		_xgraph_edge_get_hash_key(edge, key);
		g_hash_table_insert (self->edges, (gpointer) key, edge);
		return edge;
	}
}

/*!
 * Create a new edge from->to and returns it. Returns NULL in case of an
 * error. Emits a 'changed' event.
 */
XGraphEdge * xgraph_create_edge (XGraph * self, XGraphNode * from, XGraphNode * to)
{
	XGraphEdge * edge = xgraph_create_edge_s (self, from, to);
	xgraph_changed(self);
	return edge;
}

#if 0
rellistptr xgraph_to_rel (XGraph * self)
{
	rellistptr rl = {0};
	g_warning ("xgraph_to_rel: NOT IMPLEMENTED!\n");
	return rl;
}
#endif

typedef struct _GraphBuilderHelper
{
	XGraph * gr;
	GHashTable * table; /*!< Maps user IDs to internal IDs. */
} GraphBuilderHelper;


/*!
 * Callback for the GraphBuilder implementation. IDs aren't internal
 * identifiers. Instead, they are identifiers used to create the node. Their
 * meaning depends on the user.
 *
 * Emits nothing.
 */
gboolean _xgraph_builder_build_edge (GraphBuilder * builder, gint fromId, gint toId)
{
	GraphBuilderHelper * helper = (GraphBuilderHelper*) builder->owner;

	XGraphNode * fromNode = g_hash_table_lookup (helper->table, GINT_TO_POINTER(fromId));
	XGraphNode * toNode = g_hash_table_lookup (helper->table, GINT_TO_POINTER(toId));

	if (!fromNode || !toNode) return FALSE;
	else{
		return NULL != xgraph_create_edge_s(helper->gr, fromNode, toNode);
	}
}

/*!
 * Create the given node with the given ID. In contrast to the most different
 * operations on node, ID is NO internal identifier here. Instead it is used
 * by the user to identify nodes, though IDs must be unique. On error, FALSE
 * is returned.
 *
 * Emits nothing.
 */
gboolean _xgraph_builder_build_node (GraphBuilder * builder, gint id)
{
	GraphBuilderHelper * helper = (GraphBuilderHelper*) builder->owner;
	if (g_hash_table_lookup(helper->table, GINT_TO_POINTER(id))) {
		return FALSE; /* Node already exists. */
	}
	else {
		XGraphNode * node = xgraph_create_node_s (helper->gr);
		g_hash_table_insert (helper->table, GINT_TO_POINTER(id), node);
		return TRUE;
	}
}

void _xgraph_builder_destroy (GraphBuilder * builder)
{
	GraphBuilderHelper * helper = (GraphBuilderHelper*) builder->owner;
	g_hash_table_destroy(helper->table);
	g_free (helper);
	g_free (builder);
}

/*!
 * Returns a builder, which allows to add nodes and edges with a standardized
 * interface. Use GraphBuilder.destroy to destroy the builder.
 */
GraphBuilder * xgraph_get_builder (XGraph * self)
{
	GraphBuilderHelper * helper = g_new0 (GraphBuilderHelper,1);
	GraphBuilder * builder = g_new0 (GraphBuilder,1);
	builder->buildEdge = (GraphBuilder_buildEdgeFunc) _xgraph_builder_build_edge;
	builder->buildNode = (GraphBuilder_buildNodeFunc) _xgraph_builder_build_node;
	builder->destroy = (GraphBuilder_destroyFunc) _xgraph_builder_destroy;

	helper->gr = self;
	helper->table = g_hash_table_new(g_direct_hash, g_direct_equal);
	builder->owner = helper;

	return builder;
}

void xgraph_apply_layout_service (XGraph * self, GraphLayoutService * service)
{
	XGRAPH_FOREACH_NODE(self, cur, iter, {
		service->layoutNode (service, cur);
	});

	XGRAPH_FOREACH_EDGE(self, cur, iter, {
		service->layoutEdge (service, cur);
	});
}


/*!
 * Implements the construction side of the "Builder" design pattern.
 */
void xgraph_build (XGraph * self, GraphBuilder * builder)
{
	XGRAPH_FOREACH_NODE(self,cur,iter,{
		builder->buildNode (builder, xgraph_node_get_id(cur));
	});
	XGRAPH_FOREACH_EDGE(self,cur,iter,{
		builder->buildEdge (builder, xgraph_edge_get_from_id(cur),
				xgraph_edge_get_to_id(cur));
	});
}

/*!
 * Creates an 1:1 mapping between the nodes of the two graphs with respect to
 * the nodes names, i.e. if $u is a node in $from and $v is a node in $to,
 * $u is mapped to $u, iff $u and $v have the same name. This mapping is
 * bijective, if both graphs have the same number of nodes and no nodes
 * has a non-default name. Otherwise it can be non-injective and
 * non-surjective.
 *
 * The map should be free'd using \ref g_hash_table_destroy .
 */
GHashTable/*<XGraphNode*,XGraphNode*>*/ * xgraph_mapping (XGraph * from,
		XGraph * to)
{
	GHashTable/*<const gchar*,XGraphNode*>*/ * n2n_to
		= g_hash_table_new (g_str_hash, g_str_equal);
	GHashTable * map = g_hash_table_new (g_direct_hash, g_direct_equal);

	XGRAPH_FOREACH_NODE(to,cur,iter,{
		g_hash_table_insert (n2n_to,(gpointer)xgraph_node_get_name(cur),cur);
	});

	XGRAPH_FOREACH_NODE(from,cur,iter,{
		g_hash_table_insert (map, cur, g_hash_table_lookup(n2n_to, xgraph_node_get_name(cur)));
	});

	return map;
}


/*!
 * Removes all nodes and edges. Emits nothing.
 */
void xgraph_clear_s (XGraph * self)
{
	/* It should be more efficient to destroy the edges first. Otherwise
	 * the node would have to find their incident edges. */
	g_hash_table_remove_all (self->edges);
	g_hash_table_remove_all (self->nodes);
}


/*!
 * Removes all nodes and edges. Emits 'changed' if the graph wasn't empty
 * already.
 */
void xgraph_clear (XGraph * self)
{
	if (xgraph_is_empty(self)) return;
	else {
		xgraph_clear_s (self);
		xgraph_changed (self);
	}
}

gboolean xgraph_is_hidden (XGraph * self) { return self->is_hidden; }

/*!
 * Emits 'changed' for the manager, if there is one and the state has changed.
 */
void xgraph_set_hidden (XGraph * self, gboolean yesno)
{
	if (yesno != self->is_hidden) {
		self->is_hidden = yesno;

		if (self->manager) xgraph_manager_changed (self->manager);
	}
}

/*******************************************************************************
 *                             Graph Node Iterator                             *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

XGraphNodeIterator * xgraph_node_iterator (XGraph * self)
{
	XGraphNodeIterator * iter = g_new0(XGraphNodeIterator,1);
	g_hash_table_iter_init(&iter->iter, self->nodes);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}

gboolean xgraph_node_iterator_is_valid (XGraphNodeIterator * self)
{ return self->is_valid; }

XGraphNode * xgraph_node_iterator_get (XGraphNodeIterator * self)
{ return self->cur; }

void xgraph_node_iterator_next (XGraphNodeIterator * self)
{ self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }

void xgraph_node_iterator_destroy (XGraphNodeIterator * self) { g_free (self); }

/*******************************************************************************
 *                         Graph Node and Node Layout                          *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

XGraphNode * _xgraph_node_ctor (XGraph * gr)
{
	XGraphNode * self = g_new0 (XGraphNode,1);
	self->graph = gr;
	self->id = _xgraph_next_internal_id (gr);
	self->is_selected = FALSE;
	self->is_selected_for_edge = FALSE;
	self->name = _xgraph_get_next_name (gr);
	_xgraph_node_layout_init (&self->layout, self);

	return self;
}

void _xgraph_node_layout_init (XGraphNodeLayout * self, XGraphNode * node)
{
	self->node = node;
	self->dim = XGRAPH_NODE_RADIUS;
	self->is_highlighted = FALSE;
	self->is_marked_first = FALSE;
	self->is_marked_second = FALSE;
	self->is_visible = TRUE;
	self->x = 0;
	self->y = 0;
}


void _xgraph_set_manager (XGraph * self, XGraphManager * manager)
{
	self->manager = manager;
}

void xgraph_node_destroy (XGraphNode * self) { xgraph_delete_node(self->graph, self); }
void xgraph_node_destroy_s (XGraphNode * self) { xgraph_delete_node_s(self->graph, self); }

gboolean xgraph_node_is_selected (XGraphNode * self) { return self->is_selected; }
gboolean xgraph_node_is_selected_for_edge (XGraphNode * self)
{ return self->is_selected_for_edge; }
void 	 xgraph_node_set_selected_s (XGraphNode * self, gboolean yesno)
{ self->is_selected = yesno; }
void 	 xgraph_node_set_selected (XGraphNode * self, gboolean yesno)
{
	if (self->is_selected != yesno) {
		xgraph_node_set_selected_s(self, yesno);
		xgraph_layout_changed(xgraph_node_get_graph(self));
	}
}
void 	 xgraph_node_set_selected_for_edge_s (XGraphNode * self, gboolean yesno)
{ self->is_selected_for_edge = yesno; }
void 	 xgraph_node_set_selected_for_edge (XGraphNode * self, gboolean yesno)
{
	if (self->is_selected_for_edge != yesno) {
		xgraph_node_set_selected_for_edge_s (self, yesno);
		xgraph_layout_changed(xgraph_node_get_graph(self));
	}
}
XGraphNodeLayout * xgraph_node_get_layout (XGraphNode * self) { return & self->layout; }
XGraph * xgraph_node_get_graph (XGraphNode * self) { return self->graph; }
gint 	 xgraph_node_get_id (XGraphNode * self) { return self->id; }
const gchar * xgraph_node_get_name (XGraphNode * self) { return self->name; }

gboolean xgraph_node_layout_is_visible (XGraphNodeLayout * self) { return self->is_visible; }
void xgraph_node_layout_set_visible_s (XGraphNodeLayout * self, gboolean yesno)
{ self->is_visible = yesno; }
void xgraph_node_layout_set_visible (XGraphNodeLayout * self, gboolean yesno)
{
	if (self->is_visible != yesno) {
		xgraph_node_layout_set_visible_s(self, yesno);
		xgraph_layout_changed(xgraph_node_get_graph(self->node));
	}
}

/*!
 * Copies the associated node!
 */
void _xgraph_node_layout_copy (XGraphNodeLayout * dst, const XGraphNodeLayout * src)
{
	*dst = *src;
}

XGraphNode * _xgraph_node_copy (XGraphNode * self)
{
	XGraphNode * copy = g_new0 (XGraphNode,1);
	copy->id = self->id;
	copy->is_selected = self->is_selected;
	copy->is_selected_for_edge = self->is_selected_for_edge;
	_xgraph_node_layout_copy(&copy->layout, &self->layout);
	copy->layout.node = copy;
	copy->name = g_strdup (self->name);
	return copy;
}

LayoutPoint xgraph_node_layout_get_pos (const XGraphNodeLayout * self)
{
	LayoutPoint pt = {self->x, self->y};
	return pt;
}


void _xgraph_node_set_graph (XGraphNode * self, XGraph * gr) { self->graph = gr; }

gfloat xgraph_node_layout_get_x (XGraphNodeLayout * self) { return self->x; }
void xgraph_node_layout_set_x_s (XGraphNodeLayout * self, gfloat x) { self->x = x; }
void xgraph_node_layout_set_x (XGraphNodeLayout * self, gfloat x)
{
	if (self->x != x) {
		xgraph_node_layout_set_x_s (self,x);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

gfloat xgraph_node_layout_get_y (XGraphNodeLayout * self) { return self->y; }
void xgraph_node_layout_set_y_s (XGraphNodeLayout * self, gfloat y) { self->y = y; }
void xgraph_node_layout_set_y (XGraphNodeLayout * self, gfloat y)
{
	if (self->y != y) {
		xgraph_node_layout_set_y_s (self,y);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

void xgraph_node_layout_set_pos_s (XGraphNodeLayout * self, gfloat x, gfloat y)
{ self->x = x; self->y = y; }
void xgraph_node_layout_set_pos (XGraphNodeLayout * self, gfloat x, gfloat y)
{
	if (self->x != x || self->y != y) {
		xgraph_node_layout_set_pos_s (self, x,y);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

gboolean xgraph_node_layout_is_highlighted (XGraphNodeLayout * self) { return self->is_highlighted; }
void xgraph_node_layout_set_highlighted_s (XGraphNodeLayout * self, gboolean yesno) { self->is_highlighted = yesno; }
void xgraph_node_layout_set_highlighted (XGraphNodeLayout * self, gboolean yesno)
{
	if (self->is_highlighted != yesno) {
		xgraph_node_layout_set_highlighted_s (self, yesno);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

gboolean xgraph_node_layout_is_marked_first (XGraphNodeLayout * self) { return self->is_marked_first; }
gboolean xgraph_node_layout_is_marked_second (XGraphNodeLayout * self) { return self->is_marked_second; }

void xgraph_node_layout_set_marked_first_s (XGraphNodeLayout * self, gboolean yesno) { self->is_marked_first = yesno; }
void xgraph_node_layout_set_marked_second_s (XGraphNodeLayout * self, gboolean yesno) { self->is_marked_second = yesno; }
void xgraph_node_layout_set_marked_first (XGraphNodeLayout * self, gboolean yesno)
{
	if (self->is_marked_first != yesno) {
		xgraph_node_layout_set_marked_first_s (self, yesno);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}
void xgraph_node_layout_set_marked_second (XGraphNodeLayout * self, gboolean yesno)
{
	if (self->is_marked_second != yesno) {
		xgraph_node_layout_set_marked_second_s (self, yesno);
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

/*!
 * Frees the memory of the node. Neither removes the node from graph, nor
 * removes incident edges.
 */
void _xgraph_node_dtor (XGraphNode * self)
{
	if (self->graph)
		XGRAPH_OBSERVER_NOTIFY(self->graph,onDeleteNode,_1(self));
	g_free (self->name);
	g_free (self);
}

gint xgraph_node_layout_get_radius (XGraphNodeLayout * self) { return self->dim; }

/*!
 * Can be used for GSequence.
 */
gboolean _xgraph_node_id_data_cmp (const XGraphNode * a,
		const XGraphNode * b, gpointer data)
{
	return a->id - b->id;
}


void _xgraph_node_set_name_s (XGraphNode * self, const gchar * new_name)
{
	g_free (self->name);
	self->name = g_strdup (new_name);
}

void xgraph_node_set_name (XGraphNode * self, const gchar * new_name)
{
	_xgraph_node_set_name_s (self, new_name);
	xgraph_changed (self);
}

void xgraph_node_layout_reset_s (XGraphNodeLayout * self)
{
	self->dim = XGRAPH_NODE_RADIUS;
	self->is_highlighted = FALSE;
	self->is_marked_first = FALSE;
	self->is_marked_second = FALSE;
	self->is_visible = TRUE;
	self->x = 0;
	self->y = 0;
}

void xgraph_node_layout_reset (XGraphNodeLayout * self)
{
	xgraph_node_layout_reset_s (self);
	xgraph_layout_changed (xgraph_node_get_graph(self->node));
}

void _xgraph_node_layout_dtor (XGraphNodeLayout * self) { }

/*!
 * Emits nothing. Doesn't copy the associated node.
 */
void xgraph_node_apply_layout_s (XGraphNode * self,
		const XGraphNodeLayout * layout)
{
	_xgraph_node_layout_dtor (&self->layout);

	_xgraph_node_layout_copy (&self->layout, layout);
	self->layout.node = self; // Necessary.
}

/*!
 * Emits 'layout-changed'.
 */
void xgraph_node_apply_layout (XGraphNode * self,
		const XGraphNodeLayout * layout)
{
	xgraph_node_apply_layout_s (self, layout);
	xgraph_layout_changed (xgraph_node_get_graph(self));
}

gdouble	xgraph_node_get_distance_to_node (const XGraphNode * self, const XGraphNode * other)
{
	LayoutPoint pt1 = xgraph_node_layout_get_pos(&self->layout);
	LayoutPoint pt2 = xgraph_node_layout_get_pos (&other->layout);
	return layout_point_distance (&pt1,&pt2);
}

gdouble	xgraph_node_get_distance_to (const XGraphNode * self, const LayoutPoint * pt)
{
	LayoutPoint our_pt = xgraph_node_layout_get_pos(&self->layout);
	return layout_point_distance (&our_pt, pt);
}

/*!
 * Emits 'layout-changed'. Radius must be >= 0.
 */
void xgraph_node_layout_set_radius (XGraphNodeLayout * self, gint radius)
{
	if ( !self->dim != radius && radius >= 0) {
		self->dim = radius;
		xgraph_layout_changed (xgraph_node_get_graph(self->node));
	}
}

/*******************************************************************************
 *                             Graph Edge Iterator                             *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

XGraphEdgeIterator * xgraph_edge_iterator (XGraph * self)
{
	XGraphEdgeIterator * iter = g_new0(XGraphEdgeIterator,1);
	g_hash_table_iter_init(&iter->iter, self->edges);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}

gboolean xgraph_edge_iterator_is_valid (XGraphEdgeIterator * self)
{ return self->is_valid; }

XGraphEdge * xgraph_edge_iterator_get (XGraphEdgeIterator * self)
{ return self->cur; }

void xgraph_edge_iterator_next (XGraphEdgeIterator * self)
{ self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }

void xgraph_edge_iterator_destroy (XGraphEdgeIterator * self) { g_free (self); }

/*******************************************************************************
 *                         Graph Edge and Edge Layout                          *
 *                                                                             *
 *                              Fri, 12 Feb 2010                               *
 ******************************************************************************/

/*!
 * Destroy the edge and free its memory. It is supposed, that the edge does
 * not belong to a graph.
 */
void _xgraph_edge_dtor (XGraphEdge * self)
{
	XGraph * graph = xgraph_edge_get_graph(self);

	if (graph)
		XGRAPH_OBSERVER_NOTIFY(graph,onDeleteEdge,_1(self));

	xgraph_edge_path_dtor (&self->path);

	/* Free the shared memory if necessary. */
	if ( !self->way_back) {
		_xgraph_edge_shared_destroy (self->shared);
	}
	else {
		/* We don't adjust the direction of the path in the shared data. I.e.
		 * the direction of the remaining edge and the actual layout may
		 * differ */
		self->way_back->way_back = NULL;
	}

	g_free (self);
}


XGraphEdge * _xgraph_edge_ctor (XGraphNode * from, XGraphNode * to)
{
	XGraph * gr = xgraph_node_get_graph(from);

	if (gr != xgraph_node_get_graph(to)) return NULL;
	else {
		XGraphEdge * self = g_new0 (XGraphEdge, 1);
		XGraphEdge * way_back = xgraph_get_edge(gr, to, from);

		self->from = from;
		self->to = to;
		self->is_visible = TRUE;
		self->is_marked_first = FALSE;
		self->is_marked_second = FALSE;
		self->is_selected = FALSE;
		self->is_highlighted = FALSE;
		self->way_back = way_back;

		xgraph_edge_path_ctor (&self->path);

		/* If there is no edge for the converse, create a new shared data
		 * structure. Use the existing memory otherwise. */
		if ( !way_back) {
			self->shared = _xgraph_edge_shared_new ();
		}
		else {
			/* Copy the existing path in reverse order. */
			XGraphEdgePath * path_back = xgraph_edge_path_clone (&way_back->path);
			xgraph_edge_path_reverse (path_back);
			xgraph_edge_path_assign (&self->path, path_back);
			xgraph_edge_path_destroy (path_back);

			g_assert (way_back->shared);
			self->shared = way_back->shared;
			way_back->way_back = self;
		}

		return self;
	}
}

XGraph * xgraph_edge_get_graph (XGraphEdge * self) { return xgraph_node_get_graph (self->to); }
void xgraph_edge_destroy (XGraphEdge * self) { xgraph_delete_edge(xgraph_edge_get_graph(self), self); }
void xgraph_edge_destroy_s (XGraphEdge * self) { xgraph_delete_edge_s(xgraph_edge_get_graph(self), self); }

const XGraphEdgePath * _xgraph_assoc_layout_get_path (const AssocEdgeLayout * self)
{ return &self->edge->path; }

gboolean _xgraph_assoc_layout_is_highlighted (const AssocEdgeLayout * self)
{ return self->edge->is_highlighted; }

gboolean _xgraph_assoc_layout_is_marked_first (const AssocEdgeLayout * self)
{ return self->edge->is_marked_first; }

gboolean _xgraph_assoc_layout_is_marked_second (const AssocEdgeLayout * self)
{ return self->edge->is_marked_second; }

gboolean _xgraph_assoc_layout_is_simple (const AssocEdgeLayout * self)
{ return xgraph_edge_path_is_simple(&self->edge->path); }

gboolean _xgraph_assoc_layout_is_visible (const AssocEdgeLayout * self)
{ return self->edge->is_visible; }



XGraphEdgePath * xgraph_edge_path_ctor (XGraphEdgePath * self)
{
	g_queue_init (&self->pts);
	self->to = 0;
	self->from = 0;
	return self;
}

XGraphEdgePath * xgraph_edge_path_new ()
{ return xgraph_edge_path_ctor (g_new0 (XGraphEdgePath,1)); }


void xgraph_edge_path_dtor (XGraphEdgePath * self)
{
	GList * iter = self->pts.head;
	for ( ; iter ; iter = iter->next) g_free (iter->data);
	g_queue_clear (&self->pts);
}

void xgraph_edge_path_destroy (XGraphEdgePath * self)
{
	xgraph_edge_path_dtor (self);
	g_free (self);
}

void xgraph_edge_path_reset (XGraphEdgePath * self)
{
	GList * iter = self->pts.head;
	for ( ; iter ; iter = iter->next) g_free (iter->data);
	g_queue_clear (&self->pts);
}

void xgraph_edge_path_assign (XGraphEdgePath * self, const XGraphEdgePath * from)
{
	xgraph_edge_path_reset (self);
	if ( from->pts.length != 0) {
		GList * iter = from->pts.head;
		for ( ; iter ; iter = iter->next)
			g_queue_push_tail (&self->pts, layout_point_copy((LayoutPoint*)iter->data));
	}
}

XGraphEdgePath * xgraph_edge_path_clone (const XGraphEdgePath * self)
{
	XGraphEdgePath * copy = xgraph_edge_path_new ();
	xgraph_edge_path_assign (copy, self);
	return copy;
}

gboolean xgraph_edge_path_is_simple (const XGraphEdgePath * self)
{ return self->pts.length == 0; }


GQueue * xgraph_edge_path_get_points (XGraphEdgePath * self) { return &self->pts; }
gint xgraph_edge_path_get_from (XGraphEdgePath * self) { return self->from; }
gint xgraph_edge_path_get_to (XGraphEdgePath * self) { return self->to; }

void xgraph_edge_path_set_to (XGraphEdgePath * self, gint nodeName) { self->to = nodeName; }
void xgraph_edge_path_set_from (XGraphEdgePath * self, gint nodeName) { self->from = nodeName; }


void xgraph_edge_path_reverse (XGraphEdgePath * self)
{ g_queue_reverse (&self->pts); }


/*!
 * Emits 'layout-changed'.
 */
void _xgraph_assoc_layout_reset (AssocEdgeLayout * self)
{
	XGraphEdge * edge = self->edge;
	edge->is_visible = TRUE;
	edge->is_highlighted = FALSE;
	edge->is_marked_first = FALSE;
	edge->is_marked_second = FALSE;

	xgraph_edge_path_reset (&edge->path);
	xgraph_layout_changed (xgraph_edge_get_graph(edge));
}

/*!
 * Emits 'layout-changed' if the state really changed.
 */
void _xgraph_assoc_layout_set_highlighted (AssocEdgeLayout * self, gboolean yesno)
{
	XGraphEdge * edge = self->edge;
	if (yesno !=edge->is_highlighted) {
		edge->is_highlighted = yesno;
		xgraph_layout_changed (xgraph_edge_get_graph(edge));
	}
}

/*!
 * Emits 'layout-changed' if the state really changed.
 */
void _xgraph_assoc_layout_set_marked_first (AssocEdgeLayout * self, gboolean yesno)
{
	XGraphEdge * edge = self->edge;
	if (yesno != edge->is_marked_first) {
		edge->is_marked_first = yesno;
		xgraph_layout_changed (xgraph_edge_get_graph(edge));
	}
}

/*!
 * Emits 'layout-changed' if the state really changed.
 */
void _xgraph_assoc_layout_set_marked_second (AssocEdgeLayout * self, gboolean yesno)
{
	XGraphEdge * edge = self->edge;
	if (yesno != edge->is_marked_second) {
		edge->is_marked_second = yesno;
		xgraph_layout_changed (xgraph_edge_get_graph(edge));
	}
}


/*!
 * Emits 'layout-changed' even if nothing has changed.
 */
void _xgraph_assoc_layout_set_path (AssocEdgeLayout * self, const XGraphEdgePath * path)
{
	XGraphEdge * edge = self->edge;

	xgraph_edge_path_assign (&edge->path, path);
	if (edge->way_back) {
		/* Set the path for the other direction as well. */
		XGraphEdgePath * path_back = xgraph_edge_path_clone (path);
		xgraph_edge_path_reverse (path_back);
		xgraph_edge_path_assign (&edge->way_back->path, path_back);
		xgraph_edge_path_destroy (path_back);
	}
	xgraph_layout_changed (xgraph_edge_get_graph(edge));
}


/*!
 * Emits 'layout-changed'.
 */
void _xgraph_assoc_layout_assign (AssocEdgeLayout * self, const XGraphEdgeLayout * from)
{
	XGraphEdge * edge = self->edge;
	edge->is_highlighted   = xgraph_edge_layout_is_highlighted(from);
	edge->is_visible       = xgraph_edge_layout_is_visible(from);
	edge->is_marked_first  = xgraph_edge_layout_is_marked_first(from);
	edge->is_marked_second = xgraph_edge_layout_is_marked_second(from);

#if 1
	xgraph_edge_path_assign (&edge->path, xgraph_edge_layout_get_path (from));
#else
	/* The given edge layout may reference nodes which are not in the current
	 * graph. So it would be wrong to just copy these nodes. Instead, we try
	 * to map our edge's nodes. */
	xgraph_edge_path_assign (&edge->shared->path, xgraph_edge_layout_get_path (from));
	{
		LayoutPoint pt1 = xgraph_edge_path_get_start(&edge->shared->path);
		LayoutPoint pt2 = xgraph_edge_path_get_end(&edge->shared->path);
		gdouble d1 = xgraph_node_get_distance_to (edge->from, &pt1);
		gdouble d2 = xgraph_node_get_distance_to (edge->from, &pt2);
		printf ("Edge<-EdgeLayout:\n"
				"\tedge from  : %f, %f\n"
				"\tedge to    : %f, %f\n"
				"\tpath start : %f, %f\n"
				"\tpath end   : %f, %f\n", edge->from->layout.x, edge->from->layout.y,
				edge->to->layout.x, edge->to->layout.y, pt1.x, pt1.y, pt2.x, pt2.y);
		printf ("Distances: %lf, %lf, %lf, %lf\n", d1, d2, xgraph_node_get_distance_to (edge->to, &pt1),
				xgraph_node_get_distance_to (edge->to, &pt2));
		if (d1 < d2) {
			edge->shared->path.start = edge->from;
			edge->shared->path.end   = edge->to;
		}
		else {
			edge->shared->path.start = edge->to;
			edge->shared->path.end   = edge->from;
		}
	}
#endif

	xgraph_layout_changed (xgraph_edge_get_graph (edge));
}

void _xgraph_assoc_layout_destroy (AssocEdgeLayout * self) {  }

XGraphEdgeLayout * xgraph_edge_get_layout (XGraphEdge * self)
{
	static EdgeLayoutClass * c = NULL;
	if (! c) {
		c = g_new0 (EdgeLayoutClass,1);
		c->getPath         = _xgraph_assoc_layout_get_path;
		c->isHighlighted   = _xgraph_assoc_layout_is_highlighted;
		c->isMarkedFirst   = _xgraph_assoc_layout_is_marked_first;
		c->isMarkedSecond  = _xgraph_assoc_layout_is_marked_second;
		c->isSimple        = _xgraph_assoc_layout_is_simple;
		c->isVisible       = _xgraph_assoc_layout_is_visible;
		c->reset           = _xgraph_assoc_layout_reset;
		c->setHighlighted  = _xgraph_assoc_layout_set_highlighted;
		c->setMarkedFirst  = _xgraph_assoc_layout_set_marked_first;
		c->setMarkedSecond = _xgraph_assoc_layout_set_marked_second;
		c->setPath         = _xgraph_assoc_layout_set_path;
		c->assign		   = _xgraph_assoc_layout_assign;
		c->destroy 		   = _xgraph_assoc_layout_destroy;
	}

	if ( ! self->layout_proxy) {
		self->layout_proxy = g_new0 (AssocEdgeLayout,1);
		self->layout_proxy->c    = c;
		self->layout_proxy->edge = self;
	}

	return (XGraphEdgeLayout*)self->layout_proxy;
}


gboolean xgraph_edge_layout_is_visible (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->isVisible (self); }

/*!
 * Returns, if the edge has a simple path. I.e. is doesn't contain helper
 * pointer. Circles are simple, too.
 */
gboolean xgraph_edge_layout_is_simple (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->isSimple (self); }

gboolean xgraph_edge_layout_is_highlighted (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->isHighlighted (self); }

void xgraph_edge_layout_set_highlighted (gpointer self, gboolean yesno)
{ ((XGraphEdgeLayout*)self)->c->setHighlighted (self, yesno); }

gboolean xgraph_edge_layout_is_marked_first (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->isMarkedFirst (self); }

gboolean xgraph_edge_layout_is_marked_second (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->isMarkedSecond (self); }

void xgraph_edge_layout_set_marked_first (gpointer self, gboolean yesno)
{ ((XGraphEdgeLayout*)self)->c->setMarkedFirst (self, yesno); }

void xgraph_edge_layout_set_marked_second (gpointer self, gboolean yesno)
{ ((XGraphEdgeLayout*)self)->c->setMarkedSecond (self, yesno); }

const XGraphEdgePath * xgraph_edge_layout_get_path (gconstpointer self)
{ return ((XGraphEdgeLayout*)self)->c->getPath (self); }

void xgraph_edge_layout_set_path (gpointer self, const XGraphEdgePath * path)
{ ((XGraphEdgeLayout*)self)->c->setPath (self,path); }

void xgraph_edge_layout_assign (gpointer self, const XGraphEdgeLayout * from)
{ ((XGraphEdgeLayout*)self)->c->assign (self,from); }

void xgraph_edge_layout_reset (gpointer self)
{ ((XGraphEdgeLayout*)self)->c->reset (self); }

void xgraph_edge_layout_destroy (gpointer self)
{ ((XGraphEdgeLayout*)self)->c->destroy (self); }

gboolean _xgraph_indep_edge_layout_is_visible (const IndepEdgeLayout * self) { return self->is_visible; }
gboolean _xgraph_indep_edge_layout_is_highlighted (const IndepEdgeLayout * self) { return self->is_highlighted; }
gboolean _xgraph_indep_edge_layout_is_marked_first (const IndepEdgeLayout * self) { return self->is_marked_first; }
gboolean _xgraph_indep_edge_layout_is_marked_second (const IndepEdgeLayout * self) { return self->is_marked_second; }
gboolean _xgraph_indep_edge_layout_is_simple (const IndepEdgeLayout * self)
{ return xgraph_edge_path_is_simple (&self->path); }
void _xgraph_indep_edge_layout_set_visible (IndepEdgeLayout * self, gboolean yesno)
{ self->is_visible = yesno; }
void _xgraph_indep_edge_layout_set_highlighted(IndepEdgeLayout * self, gboolean yesno)
{ self->is_highlighted = yesno; }
void _xgraph_indep_edge_layout_set_marked_first (IndepEdgeLayout * self, gboolean yesno)
{ self->is_marked_first = yesno; }
void _xgraph_indep_edge_layout_set_marked_second (IndepEdgeLayout * self, gboolean yesno)
{ self->is_marked_second = yesno; }

const XGraphEdgePath * _xgraph_indep_edge_layout_get_path (const IndepEdgeLayout * self) { return &self->path; }
void _xgraph_indep_edge_layout_set_path (IndepEdgeLayout * self, const XGraphEdgePath * path)
{ xgraph_edge_path_assign (&self->path, path); }

void _xgraph_indep_edge_layout_dtor (IndepEdgeLayout * self)
{ xgraph_edge_path_dtor (&self->path); }

void _xgraph_indep_edge_layout_destroy (IndepEdgeLayout * self)
{
	_xgraph_indep_edge_layout_dtor (self);
	g_free (self);
}

void _xgraph_indep_edge_layout_reset (IndepEdgeLayout * self)
{
	self->is_highlighted = FALSE;
	self->is_marked_first = FALSE;
	self->is_marked_second = FALSE;
	self->is_visible = TRUE;
	xgraph_edge_path_reset(&self->path);
}

void _xgraph_indep_edge_layout_assign (IndepEdgeLayout * self, XGraphEdgeLayout * from)
{
	self->is_highlighted = xgraph_edge_layout_is_highlighted(from);
	self->is_marked_first = xgraph_edge_layout_is_marked_first(from);
	self->is_marked_second = xgraph_edge_layout_is_marked_second(from);
	self->is_visible = xgraph_edge_layout_is_visible(from);
	xgraph_edge_path_assign (&self->path, xgraph_edge_layout_get_path(from));
}

IndepEdgeLayout * xgraph_indep_edge_layout_ctor (IndepEdgeLayout * self)
{
	EdgeLayoutClass * c = NULL;
	if (! c) {
		c = g_new0 (EdgeLayoutClass,1);

		c->isVisible       = _xgraph_indep_edge_layout_is_visible;
		c->isHighlighted   = _xgraph_indep_edge_layout_is_highlighted;
		c->isMarkedFirst   = _xgraph_indep_edge_layout_is_marked_first;
		c->isMarkedSecond  = _xgraph_indep_edge_layout_is_marked_second;
		c->isSimple        = _xgraph_indep_edge_layout_is_simple;
		c->setHighlighted  = _xgraph_indep_edge_layout_set_highlighted;
		c->setMarkedFirst  = _xgraph_indep_edge_layout_set_marked_first;
		c->setMarkedSecond = _xgraph_indep_edge_layout_set_marked_second;
		c->setPath         = _xgraph_indep_edge_layout_set_path;
		c->getPath         = _xgraph_indep_edge_layout_get_path;
		c->reset		   = _xgraph_indep_edge_layout_reset;
		c->assign		   = _xgraph_indep_edge_layout_assign;
		c->destroy 		   = _xgraph_indep_edge_layout_destroy;
	}

	self->c = c;
	self->is_highlighted = FALSE;
	self->is_marked_first = FALSE;
	self->is_marked_second = FALSE;
	self->is_visible = TRUE;
	xgraph_edge_path_ctor (&self->path);

	return self;
}

XGraphEdgeLayout * xgraph_indep_edge_layout_new ()
{ return (XGraphEdgeLayout*) xgraph_indep_edge_layout_ctor (g_new0 (IndepEdgeLayout,1)); }

/*!
 * Emits 'layout-changed'.
 */
void xgraph_edge_apply_layout (XGraphEdge * self, const XGraphEdgeLayout * layout)
{ xgraph_edge_layout_assign (xgraph_edge_get_layout(self), layout); }

/*!
 * Copies the layout of one edges to another. Note that after the call, the new
 * layout does no refer to any edge.
 */
void _xgraph_edge_layout_copy (XGraphEdgeLayout * dst, const XGraphEdgeLayout * src)
{
	g_warning ("xgraph_edge_layout_copy: NOT IMPLEMENTED!\n");
}


XGraphEdgeShared * _xgraph_edge_shared_ctor (XGraphEdgeShared * self)
{
	//xgraph_edge_path_ctor(&self->path, NULL, NULL); return self;*/
	return self;
}

void _xgraph_edge_shared_dtor (XGraphEdgeShared * self)
{
	//xgraph_edge_path_dtor (&self->path);
}

XGraphEdgeShared * _xgraph_edge_shared_new ()
{ return _xgraph_edge_shared_ctor (g_new0 (XGraphEdgeShared, 1)); }

void _xgraph_edge_shared_destroy (XGraphEdgeShared * self)
{
	_xgraph_edge_shared_dtor (self);
	g_free (self);
}

XGraphEdgeShared * _xgraph_edge_shared_copy (const XGraphEdgeShared * self)
{
//#error THIS is a problem, because the nodes are stored!
	XGraphEdgeShared * copy = _xgraph_edge_shared_new ();
//	xgraph_edge_path_assign(&copy->path, &self->path);

	return copy;
}

gboolean xgraph_edge_is_selected (XGraphEdge * self) { return self->is_selected; }
void xgraph_edge_set_selected_s (XGraphEdge * self, gboolean yesno)
{ self->is_selected = yesno; }
void xgraph_edge_set_selected (XGraphEdge * self, gboolean yesno)
{
	if (self->is_selected != yesno) {
		xgraph_edge_set_selected_s (self, yesno);
		xgraph_layout_changed(xgraph_edge_get_graph(self));
	}
}

gint xgraph_edge_get_from_id (XGraphEdge * self) { return xgraph_node_get_id(self->from); }
gint xgraph_edge_get_to_id (XGraphEdge * self) { return xgraph_node_get_id(self->to); }
XGraphNode * xgraph_edge_get_from_node (XGraphEdge * self) { return self->from; }
XGraphNode * xgraph_edge_get_to_node (XGraphEdge * self) { return self->to; }

/* return {0,0,0,0} on error or if the path is empty. Returns a valid result if
 * it is simple. */
LayoutRect xgraph_edge_layout_get_bounding_box (const XGraphEdgeLayout * self)
{
	const XGraphEdgePath * path = xgraph_edge_layout_get_path(self);
	if ( xgraph_edge_path_is_simple(path)) {
		LayoutRect rc = {0,0,0,0};
		return rc;
	}
	else {
		LayoutRect rc = { G_MAXINT,G_MAXINT,G_MININT,G_MININT };

#define _X(x,y) { rc.left = MIN(rc.left, (x)); rc.top = MIN(rc.top, (y)); \
	rc.right = MAX(rc.right, (x)); rc.bottom = MAX(rc.bottom, (y)); }

		{
			GList * iter = path->pts.head;
			for ( ; iter ; iter = iter->next) {
				LayoutPoint * ppt = (LayoutPoint*) iter->data;
				_X(ppt->x, ppt->y);
			}
		}
#undef _X

		return rc;
	}
}

gboolean xgraph_edge_has_inverse (XGraphEdge * self) { return xgraph_edge_get_inverse (self) != NULL; }
XGraphEdge * xgraph_edge_get_inverse (XGraphEdge * self) { return self->way_back; }

/*******************************************************************************
 *                            DefaultLayoutService                             *
 *                                                                             *
 *                              Mon, 15 Feb 2010                               *
 ******************************************************************************/

typedef struct _DefaultGraphLayoutService
{
	gint nodeCount;
	gint radius;
	gdouble alpha;
	XGraph * gr;
} DefaultGraphLayoutService;

void _default_graph_layout_service_ctor (DefaultGraphLayoutService * self,
		XGraph * gr, gint radius)
{
	self->gr = gr;
	self->radius = radius;
	self->nodeCount = xgraph_get_node_count(gr);
	self->alpha = 2.0*G_PI / self->nodeCount;
}

void _default_graph_layout_service_layout_node (GraphLayoutService * service,
		XGraphNode * node)
{
	DefaultGraphLayoutService * self = (DefaultGraphLayoutService*) service->owner;
	XGraphNodeLayout * layout = xgraph_node_get_layout (node);
	gint i = atoi (xgraph_node_get_name(node));

	xgraph_block_notify(self->gr);

	if (i < 1) g_warning ("_default_graph_layout_service_layout_node: "
			"Unexpected node name: \"%s\".\n", xgraph_node_get_name(node));
	else {
		gint node_r = xgraph_node_layout_get_radius (layout);
		gfloat x = 2.0 * node_r + self->radius * (1.0 - sin(i*self->alpha));
		gfloat y = 2.0 * node_r + self->radius * (1.0 + cos(i*self->alpha));

		xgraph_node_layout_reset (layout);
		xgraph_node_layout_set_pos(layout, x,y);
	}

	xgraph_unblock_notify(self->gr);
}

void _default_graph_layout_service_layout_edge (GraphLayoutService * service,
		XGraphEdge * edge)
{
	DefaultGraphLayoutService * self = (DefaultGraphLayoutService*) service->owner;

	xgraph_block_notify(self->gr); {
		xgraph_edge_layout_reset (xgraph_edge_get_layout(edge));
	}xgraph_unblock_notify(self->gr);
}

void _default_graph_layout_service_destroy (GraphLayoutService * service)
{
	DefaultGraphLayoutService * self = (DefaultGraphLayoutService*) service->owner;
	g_free (self);
	g_free (service);
}

GraphLayoutService * default_graph_layout_service_new (XGraph * gr, gint radius)
{
	GraphLayoutService * service = g_new0(GraphLayoutService,1);
	DefaultGraphLayoutService * self = g_new0(DefaultGraphLayoutService,1);
	_default_graph_layout_service_ctor (self, gr, radius);
	service->owner = self;
	service->layoutNode = (GraphLayoutService_layoutNodeFunc) _default_graph_layout_service_layout_node;
	service->layoutEdge = (GraphLayoutService_layoutEdgeFunc) _default_graph_layout_service_layout_edge;
	service->destroy = (GraphLayoutService_destroyFunc) _default_graph_layout_service_destroy;

	return service;
}

