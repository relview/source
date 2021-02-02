/*
 * Edge.c
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

#include "Edge.h"

#include <math.h>
#include <assert.h>
#include "Vec2d.h"
#include "GraphWindowImpl.h" /* graph_window_display_edge, RADIUS */

/*! Determines whether the given point (xp,yp) is over the line segment
 * given as (xa,ya) and (xb,yb) with a thickness of 2 x marginWidth.
 * All coordinates are assumed absolute to a common origin and in
 * pixel coordinates.
 *
 * \author stb
 * \param xa X coordinate of the first point of the line segment.
 * \param ya Y coordinate of the first point of the line segment.
 * \param xb X coordinate of the second point of the line segment.
 * \param yb Y coordinate of the second point of the line segment.
 * \param xp The x coordinate of the point to test.
 * \param yp The y coordinate of the point to test.
 * \param marginWidth Half of the overall thickness of the line segment.
 * \return TRUE, if the given point is over the line segment, FALSE
 *         otherwise.
 *
 * \todo The current implementation uses trigonometric functions and
 *       therefore is quiet slow. Add a fast bounding box test, to test
 *       only those points with the precise method, if the point is in
 *       the bounding box.
 */
gboolean edge_is_over_raw (int xa, int ya, int xb, int yb,
                           int xp, int yp, int marginWidth)
{


  if (xa > xb) {
    return edge_is_over_raw (xb,yb,xa,ya,xp,yp,marginWidth);
  }
  else {
    gint top = MIN(ya,yb), bottom = MAX(ya,yb);

    /* cheap bounding box test */
    if (! rect_contains (xa - marginWidth, top - marginWidth,
                         xb + marginWidth, bottom + marginWidth,
                         xp,yp))
      return FALSE;
    else {
      vec2d a = {xa,ya}, b = {xb,yb}, p = {xp,yp}, o;
      float alpha;

      vec2d_sub (&b,&a);
      vec2d_sub (&p,&a);

      /* alpha = acos ( b * (0,-1) / ( |b| * |(0,-1)| ) ) */
      alpha = acos (-b.y / vec2d_len (&b));

      vec2d_rotate_ccw (&p, alpha, &o);

      return rect_contains (-marginWidth, 0, marginWidth, -vec2d_len (&b),
                            o.x, o.y);
    }
  }
}

/*! Returns the coordinate of a circle with the given radius to the given angle (math).
 * This routine is
 *
 * \author stb
 * \param radius The radius of the circle
 * \param angle The angle to get the coordinate for.
 * \return Returns the (rounded) coordinates as an GdkPoint object.
 */
GdkPoint circle_point (double radius, double angle)
{
  GdkPoint pt;

  pt.x = (gint) lrint (cos (angle) * radius);
  pt.y = (gint) lrint (sin (angle) * radius);

  return pt;
}

/*! Builds an sensor in shape of a ring. It's used to determine, whether
 * a given point is over an reflexive edge.
 *
 * \note Actually the ring is approximated by two polygons creates
 *       from discrete points of a circle.
 *
 * \author stb
 * \param marginWidth See edge_is_over_raw for details. */
GdkRegion * edge_build_reflexive_sensor (int radius, int marginWidth)
{
  GdkPoint polyInnerPoints[8], polyOuterPoints[8];
  double angle;
  int i;

  for (angle = .0, i = 0 ; angle < 2.0*M_PI ; angle += (2.0*M_PI)/8.0, i ++) {
    polyInnerPoints[i]
      = circle_point (radius/2.0 - marginWidth, angle);
    polyOuterPoints[i]
      = circle_point (radius/2.0 + marginWidth, angle);
  }

  {
    GdkRegion *polyInner = gdk_region_polygon (polyInnerPoints, 8, GDK_EVEN_ODD_RULE),
      *polyOuter = gdk_region_polygon (polyOuterPoints, 8, GDK_EVEN_ODD_RULE);

    gdk_region_subtract (polyOuter, polyInner);

    gdk_region_destroy (polyInner);

    return polyOuter;
  }
}

gboolean edge_is_over_reflexive (XGraphNode * node, int xp, int yp,
                                 int radius, int marginWidth)
{
  static GdkRegion * regionSensor = NULL;
  static GdkRegion * boundingBox = NULL;
  XGraphNodeLayout * layout = xgraph_node_get_layout (node);
  gint x_pos = xgraph_node_layout_get_x (layout);
  gint y_pos = xgraph_node_layout_get_y (layout);
  GdkPoint c = {x_pos, y_pos - radius/2.0};

  if (! regionSensor) {
    GdkRectangle rc;

    regionSensor = edge_build_reflexive_sensor (radius, marginWidth);
    gdk_region_get_clipbox (regionSensor, &rc);
    boundingBox = gdk_region_rectangle (&rc);
  }

  xp -= c.x;
  yp -= c.y;

  /* do the expensive test on the GdkRegion only, if the point is in the
   * bounding box. */
  if (gdk_region_point_in (boundingBox, xp, yp)) {
      if (gdk_region_point_in (regionSensor, xp, yp))
        return TRUE;
  }
  return FALSE;
}

void edge_path_destroy (EdgePath * path)
{
  free (path->points);
  free (path);
}

void edge_path_dump (EdgePath * path)
{
  EdgePathPoint * pts = path->points;
  gint n = path->pointCount;
  gint i;

  printf ("___edge:___\n"
          "  from : (%d,%d)\n"
          "    to : (%d,%d)\n"
          "  over : ",
          pts[0].x, pts[0].y, pts[n-1].x, pts[n-1].y);
  for (i = 1 ; i < n-1 ; i ++)
    printf ("(%d,%d)\n", pts[i].x, pts[i].y);
}

gint edge_path_get_segment_count (EdgePath * path)
{
  return path->pointCount - 1;
}

/* 0-indexed */
void edge_path_get_segment (EdgePath * path, gint i,
                            EdgePathPoint *from,
                            EdgePathPoint *to)
{
  if (i < edge_path_get_segment_count (path)) {
    *from = path->points[i];
    *to = path->points[i+1];
  }
}

/*!
 * Returns the index (0-index) of the first segment which belongs to the edge.
 * Returns 0, if there is no inverse edge. In that case the complete path
 * belongs to the given edge.
 */
gint edge_path_get_half_for_edge (EdgePath * path, XGraphEdge * edge)
{
	gint n = edge_path_get_segment_count(path);
	if (n % 2 == 0) return n >> 1; /* even */
	else {
		/* Odd. In this case we need a tie breaking rule to assign the single
		 * segment consistently to one edge. */
		n >>= 1;

		if ( !xgraph_edge_has_inverse(edge)) {
			g_warning ("edge_path_get_half_for_edge: No inverse edge.\n");
			return 0;
		}
		else {
			if (xgraph_edge_get_from_id(edge) > xgraph_edge_get_to_id(edge))
				return n+1;
			else return n;
		}
	}
}

/* result must be freed after use using egde_path_destroy().
 * also works for edges with (edge->path == NULL). In that case
 * the path will only contain the from and the to node. */
EdgePath * edge_path_build(XGraph * gr, XGraphEdge * edge)
{
	EdgePath * path = (EdgePath*) malloc(sizeof(EdgePath));
	const XGraphEdgePath * edge_path
		= xgraph_edge_layout_get_path(xgraph_edge_get_layout(edge));
	XGraphNode *edge_start = xgraph_edge_get_from_node(edge),
			*edge_end = xgraph_edge_get_to_node(edge);

	LayoutPoint start_pt = xgraph_node_layout_get_pos(xgraph_node_get_layout(edge_start));
	LayoutPoint end_pt = xgraph_node_layout_get_pos(xgraph_node_get_layout(edge_end));
	const GQueue/*<LayoutPoint*>*/ * pts = xgraph_edge_path_get_points(edge_path);
	const GList/*<LayoutPoint*>*/ * iter;
	int i, path_len; /* from, to */

	path_len = 2 /* from, to */+ pts->length;

	path->points = g_new0 (EdgePathPoint, path_len);

	/* First point is the from node. */
	path->points[0].x = start_pt.x;
	path->points[0].y = start_pt.y;

	for (iter = pts->head, i=1 ; iter != NULL ; iter = iter->next, ++i) {
		const LayoutPoint * pt = (LayoutPoint*) iter->data;
		path->points[i].x = pt->x;
		path->points[i].y = pt->y;
	}

	/* Last point is the to node. */
	path->points[path_len - 1].x = end_pt.x;
	path->points[path_len - 1].y = end_pt.y;
	path->pointCount = path_len;

	return path;
}

gboolean edge_is_over(XGraph * gr, XGraphEdge * edge, int xp, int yp)
{
	XGraphEdgeLayout * edge_path = xgraph_edge_get_layout(edge);
	XGraphNode * from = xgraph_edge_get_from_node(edge);
	XGraphNode * to = xgraph_edge_get_to_node(edge);

	if (from == to) {
		return edge_is_over_reflexive(from, xp, yp, EDGE_ARC_RADIUS,
				EDGE_ARC_MARGIN_WIDTH);
	}
	else if ( !xgraph_edge_layout_is_simple(edge_path)) /* non-simple edge with a path. */ {
		EdgePath * path = edge_path_build(gr, edge);
		EdgePathPoint ptFrom, ptTo;
		gint segmentCount = edge_path_get_segment_count(path);
		gint i, start_i;
		gboolean isOver = FALSE;

		if (xgraph_edge_has_inverse(edge))
			start_i = edge_path_get_half_for_edge(path, edge);
		else start_i = 0;

		/* the point is over the edge, if it is over any path segment. */
		for (i = start_i ; i < segmentCount; i++) {
			edge_path_get_segment(path, i, &ptFrom, &ptTo);
			if (edge_is_over_raw(ptFrom.x, ptFrom.y, ptTo.x, ptTo.y, xp, yp,
					EDGE_MARGIN_WIDTH)) {
				isOver = TRUE;
				break;
			}
		}

		edge_path_destroy(path);
		return isOver;
	}
	else /* simple edge */ {
		XGraphNodeLayout * from_layout = xgraph_node_get_layout(from);
		XGraphNodeLayout * to_layout = xgraph_node_get_layout(to);

		gfloat from_x_pos = xgraph_node_layout_get_x(from_layout);
		gfloat from_y_pos = xgraph_node_layout_get_y(from_layout);
		gfloat to_x_pos   = xgraph_node_layout_get_x(to_layout);
		gfloat to_y_pos   = xgraph_node_layout_get_y(to_layout);

		int xa = from_x_pos, ya = from_y_pos, xb = to_x_pos, yb = to_y_pos;

		/* if edge=(i,j) and there is also (j,i) in the graph, we user is over
		 * edge only if he's on the corresponding half of the edge. */
		if (xgraph_has_edge(gr, to, from)) {
			int dx = xb - xa, dy = yb - ya;

			xa += dx / 2;
			ya += dy / 2;
		}

		return edge_is_over_raw(xa, ya, xb, yb, xp, yp, EDGE_MARGIN_WIDTH);
	}
}

gboolean edge_sel_is_over (SelectionManager * sm,
                           Selectable sel, gint xp, gint yp)
{
	XGraphEdge * edge = (XGraphEdge*) sel;
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraph * gr = graph_window_get_graph(gw);

	return edge_is_over(gr, edge, xp, yp);
}

void edge_sel_highlight (SelectionManager * sm, Selectable sel)
{
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraphEdge * edge = (XGraphEdge*) sel;
	XGraph * gr = xgraph_edge_get_graph(edge);

	xgraph_block_notify(gr); {
		xgraph_edge_layout_set_highlighted(xgraph_edge_get_layout(edge), TRUE);
	}xgraph_unblock_notify(gr);

	graph_window_display_edge(gw, edge);
}

void edge_sel_unhighlight (SelectionManager * sm, Selectable sel)
{
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraphEdge * edge = (XGraphEdge*) sel;
	XGraph * gr = xgraph_edge_get_graph(edge);

	xgraph_block_notify(gr); {
		xgraph_edge_layout_set_highlighted(xgraph_edge_get_layout(edge), FALSE);
	}xgraph_unblock_notify(gr);

	graph_window_display_edge(gw, edge);
}

void edge_sel_select (SelectionManager * sm, Selectable sel)
{
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraphEdge * edge = (XGraphEdge*) sel;

	/* Don't notify observers. */
	xgraph_edge_set_selected_s(edge, TRUE);

	graph_window_display_edge(gw, edge);
}

void edge_sel_unselect (SelectionManager * sm, Selectable sel)
{
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraphEdge * edge = (XGraphEdge*) sel;

	/* Don't notify observers. */
	xgraph_edge_set_selected_s(edge, FALSE);

	graph_window_display_edge(gw, edge);
}

gpointer edge_sel_iter_begin (SelectionManager * sm, SelectionClass * class)
{
	GraphWindow * gw = (GraphWindow*) sm->getData(sm);
	XGraph * gr = graph_window_get_graph(gw);
	return (gpointer) xgraph_edge_iterator(gr);
}

void edge_sel_iter_next (gpointer * piter)
{
	xgraph_edge_iterator_next ((XGraphEdgeIterator*) *piter);
}

gboolean edge_sel_iter_has_next (gpointer iter)
{
	return xgraph_edge_iterator_is_valid((XGraphEdgeIterator*)iter);
}

Selectable edge_sel_iter_object (gpointer iter)
{
	return (Selectable)xgraph_edge_iterator_get((XGraphEdgeIterator*)iter);
}
