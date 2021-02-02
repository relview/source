#include "Node.h"

#include "Edge.h"
#include "Vec2d.h"
#include "GraphWindow.h" /* graph_window_display_node */
#include <math.h> // M_PI

/*! Builds a circular GdkRegion object, which can be used to determine whether
 * a point (e.g. the mouse) is over a node.
 *
 * \note Actually the circle is a polygon build from discrete circle points.
 *
 * \author stb
 * \param radius The radius of the circle in pixels.
 * \return The GdkRegion object.
 */
GdkRegion * node_build_sensor (double radius)
{
  GdkPoint polyPoints[8];
  double angle;
  int i;

  for (angle = .0, i = 0 ; angle < 2.0*M_PI ; angle += (2.0*M_PI)/8.0, i ++) {
    polyPoints[i] = circle_point (radius, angle);
  }

  return gdk_region_polygon (polyPoints, 8, GDK_EVEN_ODD_RULE);
}

/*! Tests whether the given point (in pixels) if over the given node. The
 * routine uses the node's coordinates stored in the Node object.
 *
 * \author stb
 * \param node The node to test.
 * \param xp The x absolute coordinate in pixels of the point to test.
 * \param yp The y absolute coordinate in pixels of the point to test.
 * \return TRUE, if the given point (xp,yp) if over the Node node. FALSE
 *         otherwise.
 */
gboolean node_is_over (XGraphNode * node, int xp, int yp)
{
	if (xgraph_node_is_helper(node)) {
    /* the user can neither highlight nor select such a node. */
    return FALSE;
  }
  else {
	XGraphNodeLayout * layout = xgraph_node_get_layout (node);
    static GdkRegion * regionSensor = NULL;
    int radius = 12; /* TODO: Where from! */

    if (! regionSensor)
      regionSensor = node_build_sensor (radius);

    xp -= xgraph_node_layout_get_x (layout);
    yp -= xgraph_node_layout_get_y (layout);

    /* bounding box test first. */
    if (rect_contains (-radius, -radius, +radius, +radius, xp, yp)) {
      return gdk_region_point_in (regionSensor, xp, yp);
    }
    else
      return FALSE;
  }
}

/*! called, when the selection manager want to test, if the mouse
 * is over the given node. */
gboolean node_sel_is_over (SelectionManager * sm, Selectable sel, gint xp, gint yp)
{
  XGraphNode * node = (XGraphNode*) sel;

  return node_is_over (node, xp, yp);
}

/*! called, when the selection manager want to highlight the node. */
void node_sel_highlight (SelectionManager * sm, Selectable sel)
{
  GraphWindow * gw = (GraphWindow*) sm->getData (sm);
  XGraphNode * node = (XGraphNode*) sel;

  xgraph_node_layout_set_highlighted_s (xgraph_node_get_layout(node), TRUE);

#warning Knoten wird uebergezeichnet. Vorher gezeichneter Knoten wird nicht geloescht.
  graph_window_display_node (gw, node);
}

/*! called, when the selection manager want to unhighlight the node. */
void node_sel_unhighlight (SelectionManager * sm, Selectable sel)
{
  GraphWindow * gw = (GraphWindow*) sm->getData (sm);
  XGraphNode * node = (XGraphNode*) sel;

  xgraph_node_layout_set_highlighted_s (xgraph_node_get_layout(node), FALSE);

  graph_window_display_node (gw, node);
}

/*! called, when the selection manager want to select the node. */
void node_sel_select (SelectionManager * sm, Selectable sel)
{
  GraphWindow * gw = (GraphWindow*) sm->getData (sm);
  XGraphNode * node = (XGraphNode*) sel;

  xgraph_node_set_selected(node, TRUE);

  graph_window_display_node (gw, node);
}

/*! called, when the selection manager want to unselect to node. */
void node_sel_unselect (SelectionManager * sm, Selectable sel)
{
  GraphWindow * gw = (GraphWindow*) sm->getData (sm);
  XGraphNode * node = (XGraphNode*) sel;

  xgraph_node_set_selected(node, FALSE);

  graph_window_display_node (gw, node);
}

gpointer node_sel_iter_begin (SelectionManager * sm, SelectionClass * class)
{
  GraphWindow * gw = (GraphWindow*) sm->getData (sm);
  XGraph * gr = graph_window_get_graph (gw);

  XGraphNodeIterator * iter = xgraph_node_iterator (gr);
  return (gpointer) iter;
}

void node_sel_iter_next (gpointer * piter)
{
	XGraphNodeIterator * iter = (XGraphNodeIterator*) *piter;
	xgraph_node_iterator_next(iter);
}

gboolean node_sel_iter_has_next (gpointer iter)
{ return xgraph_node_iterator_is_valid ((XGraphNodeIterator*)iter); }

Selectable node_sel_iter_object (gpointer iter)
{ return (Selectable)xgraph_node_iterator_get((XGraphNodeIterator*)iter); }



#ifdef DnD
void node_handle_drag_begin (Dragable * o, const DragManager * m)
{
  GdkPoint * porigin = (void*) malloc (sizeof (GdkPoint));
  Node * node = (Node*)o;

  porigin->x = node->x_pos;
  porigin->y = node->y_pos;

  m->setUserData (m, porigin);
}

void node_handle_drag_motion (Dragable * o, const DragManager * m)
{
  GdkPoint delta;
  GdkPoint * porigin = (GdkPoint*) m->getUserData(m);
  const GdkPoint * pstart = m->getStartPos(m), * pcur = m->getCurrentPos(m);

  delta.x = pcur->x - pstart->x;
  delta.y = pcur->y - pstart->y;

#define MOVE_SELECTION

#if defined MOVE_SELECTION
  /* bewege die gesamte Auswahl und den aktuellen Knoten, auch wenn dieser
   * nicht in der Auswahl ist. Achte darauf, dass die Objekt in der Auswahl
   * auch Knoten sind, und nicht etwas Kanten. */
  {
    Node * dndNode = (Node*)o;
    SelectionClass * selClass;
    GList * l = g_selManager->getSelection (g_selManager, &selClass);

    if (l != NULL &&
        selClass->data == (void*)GRAPH_SELECTION_TYPE_NODES) {

      {
        GList * iter = l;

        for ( ; iter ; iter = g_list_next (iter)) {
          Node * node = (Node*)iter->data;
          GdkPoint lambda;

          /* see below */
          if (node == dndNode)
            continue;

          lambda.x = node->x_pos - dndNode->x_pos;
          lambda.y = node->y_pos - dndNode->y_pos;

          node->x_pos = (porigin->x + delta.x) + lambda.x;
          node->y_pos = (porigin->y + delta.y) + lambda.y;
        }
      }

      /* must be computed after the selected nodes are computed, since
        * the it's position is needed to compute the absolute position
        * of the selected nodes. */
      {
        dndNode->x_pos = porigin->x + delta.x;
        dndNode->y_pos = porigin->y + delta.y;
      }
    }
    else {
      dndNode->x_pos = porigin->x + delta.x;
      dndNode->y_pos = porigin->y + delta.y;
    }

    g_list_free (l);
  }
#else
  {
    Node * node = (Node*)o;

    node->x_pos = porigin->x + delta.x;
    node->y_pos = porigin->y + delta.y;
  }
#endif

#undef MOVE_SELECTION
}

void node_handle_drop (Dragable * o, const DragManager * m)
{
  free (m->getUserData(m));
}

#endif
