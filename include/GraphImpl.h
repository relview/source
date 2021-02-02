/*
 * GraphImpl.h
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

#ifndef GRAPHIMPL_H_
#define GRAPHIMPL_H_

#include "Graph.h"
#include "Observer.h" // _0, _1, ...

#define XGRAPH_NODE_RADIUS 11

struct _XGraphManager /* implements XGraphObserver */ {

	GHashTable/*<gchar*,Graph*>*/ * 	graphs;
	GSList/*<GraphManagerObserver*>*/ * observers;

	gint block_notify; /*!< If >0, no 'changed' signals are emitted. */
	XGraphObserver graph_observer;
};

struct _XGraphLayout {
	gboolean is_visible;
};

struct _XGraph {
	XGraphManager * manager;
	gchar * name;

	gboolean is_hidden; /*!< This has nothing to do with it's visibility.
						 *   Hidden means something like 'non-user-generated'. */

	gboolean is_correspondence;

	/* Maps the internal (and constant during life time) IDs to the nodes.
	 * Trees cannot be used here, because GLib doesn't support iterators on
	 * them. */
	GHashTable/*<gint,XGraphNode*>*/ * nodes;

	/* Maps pairs (first,second) of (internal) node IDs to an edge. */
	GHashTable/*<gint[2]>, XGraphEdge*>*/ * edges;

	XGraphLayout layout;

	gint next_internal_id; /*!< Next ID for a new node. Incremental,
						    * beginning at 1. */

	gint block_notify; /*!< Block 'changed' and 'layout-changed'
						* notifications, if >0. */

	GSList * observers;
};

struct _XGraphNodeLayout {
	XGraphNode * node;

	gboolean is_helper;
	gboolean is_visible;
	gboolean is_highlighted;
	gboolean is_marked_first;
	gboolean is_marked_second;
	gfloat x,y;

	gint dim; /*!< Dimension. Used for the graph width and height.  */
};

struct _XGraphNode {
	gchar * name;
	gint id;

	XGraph * graph;

	gboolean is_selected;
	gboolean is_selected_for_edge;

	XGraphNodeLayout layout;
};

#if 1
struct _XGraphEdgePath
{
	gint from;
	gint to;
	GQueue/*<LayoutPoint*>*/ pts;
};
#endif

typedef struct _EdgeLayoutClass
{
#define THIS XGraphEdgeLayout*

	gboolean (*isVisible) (const THIS);
	gboolean (*isSimple) (const THIS);
	gboolean (*isHighlighted) (const THIS);
	gboolean (*isMarkedFirst) (const THIS);
	gboolean (*isMarkedSecond) (const THIS);
	const XGraphEdgePath * (*getPath) (const THIS);
	void (*setHighlighted) (THIS,gboolean);
	void (*setMarkedFirst) (THIS,gboolean);
	void (*setMarkedSecond) (THIS,gboolean);
	void (*reset) (THIS);
	void (*setPath) (THIS,const XGraphEdgePath*);
	void (*assign) (THIS, const XGraphEdgeLayout*);
	void (*destroy) (THIS);
	//XGraphEdgeLayout* (*duplicate) (THIS)
#undef THIS
} EdgeLayoutClass;

/*abstract*/ struct _XGraphEdgeLayout
{
	EdgeLayoutClass * c;
};

/*!
 * An edge's layout may share its path with its converse edge, but each edge
 * has its own visibility flag. Thus, it's unusual to store a path with each
 * edge. Sometimes a user would like to have an edge layout independently of
 * any edge as a single object. We abstract the layout functionality so we can
 * implement both cases separately.
 */
typedef struct _IndepEdgeLayout IndepEdgeLayout;
typedef struct _AssocEdgeLayout AssocEdgeLayout;

struct _IndepEdgeLayout
{
	EdgeLayoutClass * c;
	gboolean is_visible;
	gboolean is_highlighted;
	gboolean is_marked_first;
	gboolean is_marked_second;

	XGraphEdgePath path;
};

struct _AssocEdgeLayout /* implements XGraphEdgeLayout */
{
	EdgeLayoutClass * c;
	XGraphEdge * edge;
};


struct _XGraphEdgeShared /* implements XGraphEdgeLayout */
{
	gint dummy;
};

struct _XGraphEdge {
	XGraphNode *from, *to;
	XGraphEdge *way_back;
	XGraphEdgeShared * shared;
	AssocEdgeLayout * layout_proxy;

	gboolean is_selected;

	gboolean is_visible;
	gboolean is_highlighted;
	gboolean is_marked_first;
	gboolean is_marked_second;

	XGraphEdgePath path;
};

// NOTE: Changing the graph manager invalidates the iterator!
struct _XGraphIterator {
	gboolean is_valid;
	XGraph * cur;
	GHashTableIter iter;
};

struct _XGraphNodeIterator {
	gboolean is_valid;
	XGraphNode * cur;
	GHashTableIter iter;
};

struct _XGraphEdgeIterator {
	gboolean is_valid;
	XGraphEdge * cur;
	GHashTableIter iter;
};


#define XGRAPH_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,XGraphManagerObserver,obj,func, __VA_ARGS__)

#define XGRAPH_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,XGraphObserver,obj,func, __VA_ARGS__)

#endif /* GRAPHIMPL_H_ */
