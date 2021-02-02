/*
 * GraphUtils.c
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

#include "Graph.h"
#include "GraphImpl.h"
#include "Relation.h"
#include "RelationProxyAdapter.h"
#include <math.h>

/*!
 * Create the default graph with 32 nodes, arranges in a circle without any
 * edge.
 *
 * \author stb
 */
XGraph * xgraph_create_default (const gchar * name, gint radius)
{
	XGraph * self = xgraph_new (name);
	gint n = 32;
	gint i;
	gdouble alpha = 2.0 * G_PI / n;

	for ( i = 1 ; i <= n ; i ++) {
		XGraphNode * node = xgraph_create_node_s(self);
		XGraphNodeLayout * layout = xgraph_node_get_layout(node);
		gint node_r = xgraph_node_layout_get_radius (layout);
		gfloat x,y;

		x = 2.0 * node_r + radius * (1.0 - sin(i*alpha));
		y = 2.0 * node_r + radius * (1.0 + cos(i*alpha));
		xgraph_node_layout_set_pos_s(layout,x,y);

	}

	xgraph_changed(self);

	g_assert (xgraph_get_node_count(self) == n);

	return self;
}


/*!
 * Resets the given graph's layout to the default layout. That is, all nodes
 * are arranges in a circle. Edge's layouts are removed as well.
 */
void xgraph_default_layout (XGraph * self, gint radius)
{

	gint n = xgraph_get_node_count(self);
	gdouble alpha = 2.0 * G_PI / n;

	//xgraph_block_notify (self);

	/* Avoid 'changed' events while the graph is laid out. */
	xgraph_block_notify (self);

	/* Reset the graph layout. */
	xgraph_reset_layout (self);

	XGRAPH_FOREACH_EDGE(self,edge,iter,{
		gint from = atoi (xgraph_node_get_name(xgraph_edge_get_from_node(edge)));
        gint to   = atoi (xgraph_node_get_name(xgraph_edge_get_to_node(edge)));
		xgraph_edge_layout_reset(xgraph_edge_get_layout(edge));
	});

	XGRAPH_FOREACH_NODE(self,node,iter,{
		gint i = atoi(xgraph_node_get_name(node));
		XGraphNodeLayout * layout = xgraph_node_get_layout(node);
		gint node_r = xgraph_node_layout_get_radius (layout);

		gfloat x = 2.0 * node_r + radius * (1.0 - sin(i*alpha));
		gfloat y = 2.0 * node_r + radius * (1.0 + cos(i*alpha));
		xgraph_node_layout_set_pos_s(layout,x,y);
	});

	xgraph_unblock_notify (self);
	xgraph_layout_changed (self);
	xgraph_changed (self);
}


/*!
 * Rearranges the nodes of the graph, so they fill the specified area without
 * loosing their layout structure. Emits 'layout-changed'.
 *
 * \author stb
 */
void xgraph_fill_area (XGraph * self, gint width, gint height)
{
	GdkRectangle rc = xgraph_get_display_rect(self);
	gdouble factor_x = width / (double) rc.width;
	gdouble factor_y = height / (double) rc.height;

	xgraph_block_notify (self);

	XGRAPH_FOREACH_NODE(self,cur,iter,{
		/* The node's position has to be moved towards the origin and to
		 * be streched to fill the whole window. */
		XGraphNodeLayout * layout = xgraph_node_get_layout(cur);
		LayoutPoint pt = xgraph_node_layout_get_pos (layout);
		pt.x = (pt.x - rc.x) * factor_x;
		pt.y = (pt.y - rc.y) * factor_y;
		xgraph_node_layout_set_pos (layout, pt.x, pt.y);
	});

	/* Adjust the edges' paths. */
	XGRAPH_FOREACH_EDGE(self, cur, edgeIter, {
		GQueue/*<LayoutPoint*>*/ * pts
			= xgraph_edge_path_get_points(
					xgraph_edge_layout_get_path(xgraph_edge_get_layout(cur)));
		GList * iter = pts->head;
		for ( ; iter ; iter = iter->next) {
			LayoutPoint * pt = (LayoutPoint*) iter->data;
			pt->x = (pt->x - rc.x) * factor_x;
			pt->y = (pt->y - rc.y) * factor_y;
		}
	});

	xgraph_unblock_notify (self);
	xgraph_layout_changed (self);
}

/*!
 * Flips the graph's layout horizontally. Emits 'layout-changed'.
 */
void xgraph_flip_display_horz (XGraph * self)
{
	gint left, top, right, bottom;
	xgraph_get_display_extent (self, &left,&top,&right,&bottom);

	xgraph_block_notify (self); {

		XGRAPH_FOREACH_NODE(self,cur,iter,{
			XGraphNodeLayout * layout = xgraph_node_get_layout (cur);
			LayoutPoint pt = xgraph_node_layout_get_pos (layout);

			pt.x = right - (pt.x - left);
			xgraph_node_layout_set_pos (layout, pt.x, pt.y);
		});

		XGRAPH_FOREACH_EDGE(self,cur,edgeIter,{
			GQueue/*<LayoutPoint*>*/ * pts
						= xgraph_edge_path_get_points(
								xgraph_edge_layout_get_path(xgraph_edge_get_layout(cur)));
			GList * iter = pts->head;
			for ( ; iter ; iter = iter->next) {
				LayoutPoint * pt = (LayoutPoint*) iter->data;
				pt->x = right - (pt->x - left);
			}
		});
	}
	xgraph_unblock_notify (self);
	xgraph_layout_changed (self);
}

/*!
 * Flips the graph's layout vertically. Emits 'layout-changed'.
 */
void xgraph_flip_display_vert (XGraph * self)
{
	gint left, top, right, bottom;
	xgraph_get_display_extent (self, &left,&top,&right,&bottom);

	xgraph_block_notify (self); {

		XGRAPH_FOREACH_NODE(self,cur,iter,{
			XGraphNodeLayout * layout = xgraph_node_get_layout (cur);
			LayoutPoint pt = xgraph_node_layout_get_pos (layout);

			//pt.x = right - (pt.x - left);
			pt.y = bottom - (pt.y - top);
			xgraph_node_layout_set_pos (layout, pt.x, pt.y);
		});

		XGRAPH_FOREACH_EDGE(self,cur,edgeIter,{
			GQueue/*<LayoutPoint*>*/ * pts
						= xgraph_edge_path_get_points(
								xgraph_edge_layout_get_path(xgraph_edge_get_layout(cur)));
			GList * iter = pts->head;
			for ( ; iter ; iter = iter->next) {
				LayoutPoint * pt = (LayoutPoint*) iter->data;
				pt->y = bottom - (pt->y - top);
			}
		});
	}
	xgraph_unblock_notify (self);
	xgraph_layout_changed (self);
}

/*!
 * Update the given graph with respect to the given relation. I.e. add/remove
 * new or unnecessary nodes and add/remove new and/or unnecessary edges. The
 * layout of present edges is kept. New edges get a simple layout while nodes
 * are layout'ed using the default graph layout service
 * \ref default_graph_layout_service_new .
 *
 * Emits 'changed' on success; even if the result is the same.
 */
void xgraph_update_from_rel (XGraph * self, Rel * rel)
{
	KureRel * impl = rel_get_impl(rel);
	if ( !kure_is_hom(impl, NULL) || !kure_rel_fits_si(impl)) {
		g_warning ("xgraph_update_from_rel: Relation is either not "
				"quadratic or too big.");
		return;
	}
	else {
		RelationProxy * proxy = relation_proxy_adapter_new(rel);
		XGraph * copy = xgraph_copy(self, "tmp");
		GHashTable/*<XGraphNode*,XGraphNode*>*/ * map;
		gint n = (gint) kure_rel_get_rows_si(impl);
		XGraphNode ** nodes = g_new (XGraphNode*,n);
		GraphLayoutService * layouter
			= default_graph_layout_service_new(self, DEFAULT_GRAPH_RADIUS);
		gint i,j;

		xgraph_block_notify(copy);
		xgraph_block_notify(self);

		xgraph_clear (self);

		for (i = 0 ; i < n ; i ++) {
			nodes[i] = xgraph_create_node (self);
			g_assert (nodes[i]);
		}

		xgraph_apply_layout_service (self, layouter);

		map = xgraph_mapping (self, copy);
#define RHO(node) g_hash_table_lookup(map,(node))

		/* We have to layout all nodes before we layout any edge. This is
		 * necessary, because the edges require final positioning of the nodes
		 * in advance if paths are used. */
		for (i = 0 ; i < n ; i ++) {
			XGraphNode * fromRho = RHO(nodes[i]);
			if (fromRho)
				xgraph_node_apply_layout (nodes[i], xgraph_node_get_layout (fromRho));
		}

		for (i = 0 ; i < n ; i ++) {
			XGraphNode * fromRho = RHO(nodes[i]);
			/* Layout the node */
			if (! fromRho) continue;

			for (j = 0 ; j < n ; j ++) {
				XGraphNode * toRho = RHO(nodes[j]);

				if (proxy->getBit (proxy,j,i)) {
					XGraphEdge * edge = xgraph_create_edge(self,nodes[i],nodes[j]);
					g_warn_if_fail(edge != NULL);
					if (toRho && fromRho && edge) {
						XGraphEdge * edgeRho = xgraph_get_edge (copy, fromRho, toRho);
						if (edgeRho) {
							/* Nodes must have same positions in both graphs. */
							xgraph_edge_apply_layout (edge, xgraph_edge_get_layout(edgeRho));
						}
					}
				}
			}
		}

#undef RHO

		xgraph_unblock_notify(copy);
		xgraph_unblock_notify(self);

		g_hash_table_destroy (map);
		layouter->destroy (layouter);
		proxy->destroy (proxy);
		xgraph_destroy(copy);

		//xgraph_layout_changed (self);
		xgraph_changed (self);
	}
}
