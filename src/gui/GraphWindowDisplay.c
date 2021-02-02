/*!
 * \file GraphWindowDisplay.c
 *
 * Drawing a graph is split into two multiple parts and concepts. Black edges(*)
 * are drawn separately for efficiency reasons. Previous version have drawn
 * all nodes and edges, if an edge was highlighted, e.g. if the user moved
 * over it with the mouse. Now these elements are handled as some kind of
 * overlay and are drawn immediately on an expose event (see graph_window_
 * expose). The speedup result from the assumption, that only a few elements
 * have to be drawn this way, what is true under normal circumstances. The
 * black edges (i.e. the static part) will only be redrawn if the layout
 * or the window geometry changes, or something similar happens. These event
 * are relatively rarely.
 *
 * (*) Regardless of their appearance (bold, dashed).
 */

#include "GraphWindowImpl.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sys/param.h>
#include <gtk/gtk.h>
#include "global.h"
#include "Relation.h"
#include "Graph.h"
#include "GraphImpl.h" // XGRAPH_NODE_RADIUS
#include "utilities.h"
#include "label.h"
#include "RelationWindow.h"
#include "input_string_gtk.h"
#include "mark_edgesnodes_gtk.h"
#include "DirWindow.h"
#include "prefs.h"
#include "Edge.h" /* EdgePath */


#define PFEILLAENGE 10   /* Laenge des Schenkels eines Pfeils */
#define WINKEL 0.40      /* Winkel zwischen Kante und Schenkel des Pfeils */


typedef enum _DrawArrowFlags
{
  NODE_START = 0x1,
  NODE_END = 0x2,
  ARROW_END = 0xa, /* includes NODE_END */
  HALF = 0x10 /* not possible with ARROW_START, OR NODE_START */
} DrawArrowFlags;

static void   clear_graph_pixmap         (GraphWindow * gw);
static void   display_node               (cairo_t * cr, XGraphNode * n, int state);
static void   display_edge_circle        (GraphWindow * gw, cairo_t * cr, XGraphNode * n, int state);
static void   display_edge               (GraphWindow * gw, cairo_t * cr, XGraphEdge * edge, int state, gboolean draw_colors);
static void   label_graph                (GraphWindow * gw, Label * label);
static void   display_graph              (GraphWindow * gw);
static void   invalidateVisibleRectGW    (GraphWindow * gw, int redraw);
static void display_edge_path (GraphWindow * gw, cairo_t * cr, XGraphEdge * edge, int state);
static void draw_edge_path(cairo_t * cr, XGraphEdge * edge, EdgePath * path, gint node_radius,
    gboolean two_way);
static void draw_norm_arrow_ex(cairo_t * cr, int len, double angle,
    gint node_radius, DrawArrowFlags flags);
static void draw_arrow_ex(cairo_t * cr, gint x1, gint y1, gint x2, gint y2,
    gint node_radius, DrawArrowFlags flags);
static void path_rounded_rect(cairo_t * cr, int x, int y, int w, int h, int r);
static void draw_normal_node(cairo_t * cr, int x, int y, int radius,
    int lineWidth, double r, double g, double b);
static void draw_marked_1_node(cairo_t * cr, int x, int y, int radius,
    double r, double g, double b);
static void draw_marked_2_node(cairo_t * cr, int x, int y, int radius,
    double r, double g, double b);
static void draw_marked_1_and_2_node(cairo_t * cr, int x, int y, int radius,
    double r, double g, double b);
static void draw_selected_for_edge_node(cairo_t * cr, int x, int y, int radius,
    double r, double g, double b);
static void _graph_window_display_layer(GraphWindow * gw, int originx, int originy, int x, int y,
    int top, int bottom);
void _graph_window_draw_message (GraphWindow * gw, gchar * text);


/* draw a normalized arrow with the given length and the given
 * angle (math. & rad.) */
void draw_norm_arrow_ex (cairo_t * cr, int len, double angle,
                                gint node_radius, DrawArrowFlags flags)
{
  int dashCount = cairo_get_dash_count (cr);
  double half_len = len / 2.0;
  double left = (flags & NODE_START) ? (half_len - node_radius) : half_len;
  double right = (flags & NODE_END) ? (half_len - node_radius) : half_len;

  cairo_rotate (cr, angle);
  cairo_translate (cr, half_len, 0);

  cairo_move_to (cr, -left, 0);
  cairo_line_to (cr, right, 0);

  if (flags & ARROW_END) {

    if (dashCount > 0) {
      /* disable the dash for the arrow end */
      double dashOffset;
      double *dashes = (double*) malloc (dashCount * sizeof (double));
      cairo_get_dash (cr, dashes, &dashOffset);
      cairo_stroke (cr);

      cairo_set_dash (cr, NULL, 0, .0);

      cairo_move_to (cr, right, 0);
      cairo_rel_line_to (cr, -8, -5);
      cairo_move_to (cr, right, 0);
      cairo_rel_line_to (cr, -8, 5);

      cairo_stroke (cr);

      /* reset the dash settings */
      if (dashCount > 0)
        cairo_set_dash (cr, dashes, dashCount, dashOffset);
    }
    else {
      cairo_rel_line_to (cr, -8, -5);
      cairo_move_to (cr, right, 0);
      cairo_rel_line_to (cr, -8, 5);

      cairo_stroke (cr);
    }
  }
  else {
    cairo_stroke (cr);
  }
}


void draw_arrow_ex (cairo_t * cr, gint x1, gint y1,
                           gint x2, gint y2,
                           gint node_radius,
                           DrawArrowFlags flags)
{
  gint x = x2 - x1;
  gint y = y2 - y1;
  double a = x1, b = y1;

  // berechne den Abstand der Punkte.
  double len = sqrt (x*x + y*y);

  // berechne den Winkel zum verschobenen zweiten Punkt.
  double angle = acos (x / len);

  // da fuer geg. x zwei Winkel im Kreis moeglich sind, muessen
  // wir zusaetzlich y heranziehen um zu entscheiden, welchen
  // Winkel wir wollen.
  if (y < 0)
    angle -= 2 * (angle - M_PI);

  if (flags & HALF) {
      a = x1 + x/2.0;
      b = y1 + y/2.0;
  }

  cairo_save (cr);

  /* Causes trouble with the outermost cairo_translate. In some way
   * it neutralizes the effect. */
  //cairo_device_to_user (cr, &a, &b);
  cairo_translate (cr, a, b);

  if (flags & HALF)
    draw_norm_arrow_ex (cr, len / 2.0, angle, node_radius, flags & ~NODE_START);
  else
    draw_norm_arrow_ex (cr, len, angle, node_radius, flags);

  /* Neccessary for edge pathes! For edges without a path cairo_restore
   * would do the job. */
  //cairo_identity_matrix (cr);
  cairo_restore (cr);
}

void path_rounded_rect (cairo_t * cr, int x, int y, int w, int h, int r)
{
  // http://www.cairographics.org/cookbook/roundedrectangles/

  /*     "Draw a rounded rectangle"
   *     #   A****BQ *
   *     #  H      C *
   *     #  *      * *
   *     #  G      D *
   *     #   F****E  */

  cairo_move_to  (cr, x+r,y);                     // Move to A
  cairo_line_to  (cr, x+w-r,y);                   // Straight line to B
  cairo_curve_to (cr, x+w,y,x+w,y,x+w,y+r);       // Curve to C, Control points are both at Q
  cairo_line_to  (cr, x+w,y+h-r);                 // Move to D
  cairo_curve_to (cr, x+w,y+h,x+w,y+h,x+w-r,y+h); // Curve to E
  cairo_line_to  (cr, x+r,y+h);                   // Line to F
  cairo_curve_to (cr, x,y+h,x,y+h,x,y+h-r);       // Curve to G
  cairo_line_to  (cr, x,y+r);                     // Line to H
  cairo_curve_to (cr, x,y,x,y,x+r,y);             // Curve to A
}

/****************************************************************************/
/*       NAME : clear_pixmap                                                */
/*    PURPOSE : clear the backpixmap and draw the grid if needed            */
/*    CREATED : 11-NOV-1994 PS                                              */
/*   MODIFIED : 11-MAY-2000 WL: GTK+ port                                   */
/*              16-MAY-2000 WL: First grid lines moved from 0 to delta_grid */
/*              25-MAY-2000 WL: Removed parameters width and height         */
/****************************************************************************/
void clear_graph_pixmap (GraphWindow * gw)
{
  gint width, height;
  gdk_window_get_size (gw->backpixmap, & width, & height);

  {
    cairo_t * cr = gw->crOffset;
    assert (cr != NULL);

    cairo_save (cr); {
      /* clean the offset (back) buffer */
      cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); /* white */
      cairo_paint (cr);
    }
    cairo_restore (cr);

    /* draw the grid */
    if (gw->useGrid) {
      gint x,y;
      gint delta = gw->delta_grid;
      double dashlen = 5.0;

      for (x = delta ; x < width ; x += delta) {
        cairo_move_to (cr, x, 0);
        cairo_rel_line_to (cr, 0, height);
      }

      for (y = delta ; y < height ; y += delta) {
        cairo_move_to (cr, 0, y);
        cairo_rel_line_to (cr, width, 0);
      }

      cairo_save (cr); {
        /* Disable anti-aliasing for the grid. This could cause to wide lines
         * for it. */
        cairo_antialias_t aa = cairo_get_antialias (cr);
        cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

        cairo_set_dash (cr, &dashlen, 1, 2.5/*offset*/);
        cairo_set_line_width (cr, 1);
        cairo_stroke (cr);

        cairo_set_antialias (cr, aa);
      }
      cairo_restore (cr);
    }
  }
}

void draw_normal_node (cairo_t * cr, int x, int y,
                              int radius, int lineWidth,
                              double r, double g, double b)
{
  cairo_save (cr); /* save the old color */

  cairo_arc (cr, x + radius, y + radius, radius, 0, 2*M_PI);
  cairo_set_source_rgb (cr, r,g,b);
  cairo_set_line_width (cr, lineWidth);
  cairo_stroke (cr);

  cairo_restore (cr); /* restore the old color */
}

void draw_marked_1_node (cairo_t * cr, int x, int y,
                                int radius,
                                double r, double g, double b)
{
  cairo_save (cr);

  cairo_arc (cr, x + radius, y + radius, radius, 0, 2*M_PI);
  cairo_set_source_rgb (cr, r,g,b);
  cairo_fill (cr);

  cairo_restore (cr);
}

void draw_marked_2_node (cairo_t * cr, int x, int y,
                                int radius,
                                double r, double g, double b)
{
  cairo_save (cr);

  path_rounded_rect (cr, x, y, 2*radius, 2*radius, 10);
  cairo_set_source_rgb (cr, r,g,b);
  cairo_set_line_width (cr, 3);
  cairo_stroke (cr);

  cairo_restore (cr);
}

void draw_marked_1_and_2_node (cairo_t * cr, int x, int y,
                                      int radius,
                                      double r, double g, double b)
{
  cairo_save (cr);

  cairo_set_source_rgb (cr, r,g,b);
  path_rounded_rect (cr, x+1, y+1, 2*radius-2, 2*radius-2, 10);
  cairo_fill (cr);

  path_rounded_rect (cr, x-1, y-1, 2*radius+2, 2*radius+2, 10);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  cairo_restore (cr);
}

void draw_selected_for_edge_node (cairo_t * cr, int x, int y,
                                         int radius,
                                         double r, double g, double b)
{
  int i, j;
  double pi_div_4 = M_PI / 4.0;
  int off = 5;
  struct {
    int xoff, yoff;
  } off_iter [] = {{off,off}, {-off,off},
                   {-off,-off}, {off,-off}};

  cairo_set_line_width (cr, 3);

  x += radius;
  y += radius;

  cairo_arc (cr, x, y, radius, .0, pi_div_4);
  for (i = 1, j = 0 ; j < 4 ; i += 2, j ++) {
    cairo_rel_line_to (cr, off_iter[j].xoff, off_iter[j].yoff);
    cairo_rel_move_to (cr, -off_iter[j].xoff, -off_iter[j].yoff);
    cairo_arc (cr, x, y, radius, pi_div_4 * i, pi_div_4 * (i+2));
  }

  cairo_stroke (cr);
}

/*! Draw the given node onto the given GraphWindow-object.
 *
 * \author stb, ps, wl
 * \param gw The destination GraphWindow.
 * \param n The node to draw.
 * \param state Must be PM. In previous versions of this routine `state`
 *              has determined whether to draw to the window directly, or
 *              to the pixbuf (PM).
 */
void display_node(cairo_t * cr, XGraphNode * n, int state)
{
  //cairo_t * cr = gw->crOffset;
  double textR = .0, textG = .0, textB = .0;
  double nodeR = .0, nodeG = .0, nodeB = .0;
  int radius;
  int lineWidth = 1;
  XGraphNodeLayout * layout = NULL;

  assert (cr != NULL);

  /* valid node?, no help node? */
  if (!n)
    return;
  else if (xgraph_node_is_helper(n))
    return;

  layout = xgraph_node_get_layout(n);
  radius = xgraph_node_layout_get_radius(layout);

#if 0
  if (xgraph_node_layout_is_marked_third(layout)) {
    printf(__FILE__" (%d): NODE_MARKED_THIRD currently unused.\n", __LINE__);
  }
#endif

  /* TODO: For compatibility only! */
#if 0
  n->flags &= ~(NODE_MARKED_FIRST | NODE_MARKED_SECOND);
  n->flags &= ~NODE_SELECTED_FOR_EDGE;

  if (n->state == SELECTED)
  n->flags |= NODE_MARKED_FIRST;
  else if (n->state == SELECTED_SECOND)
  n->flags |= NODE_MARKED_SECOND;
  else if (n->state == SELECTED_BOTH)
  n->flags |= NODE_MARKED_FIRST | NODE_MARKED_SECOND;
  else if (n->state == SELECTED_FOR_EDGE)
  n->flags |= NODE_SELECTED_FOR_EDGE;
#endif

  /* Just draw visible nodes. */
  if (xgraph_node_layout_is_visible(layout)) {

    /* Determine the appropriate color for the node. This depends on if the
     * node is selected and if it is highlighted. */
    /* determine the approriate node color */
    if (xgraph_node_is_selected(n)) {
      nodeR = 1.0;
      lineWidth = 3;
    }
    if (xgraph_node_layout_is_highlighted(layout)) {
      nodeB = 1.0;
      lineWidth = 3;
    }

    /* If the node is marked the text will be drawn in white
     * for a better contrast. */
    if (xgraph_node_layout_is_marked_first(layout)
        && !xgraph_node_is_selected_for_edge(n)) {
      textR = textG = textB = 1.0;
    }
    //if ((NODE_HAS_FLAG (n->flags, NODE_MARKED_FIRST)
    //  || NODE_HAS_ALL_FLAGS (n->flags, NODE_MARKED_FIRST | NODE_MARKED_SECOND))
    //    &&
    //     ! NODE_HAS_FLAG (n->flags, NODE_SELECTED_FOR_EDGE))

    /* draw node and text inside */

    assert (state == PM);
    if (state == PM) {
      gint x_pos = xgraph_node_layout_get_x(layout);
      gint y_pos = xgraph_node_layout_get_y(layout);

      /* the left and upper corner of the node's bounding box. NOT it's center pointer */
      gint dstX = x_pos - radius, dstY = y_pos - radius;

      if (xgraph_node_is_selected_for_edge(n))
        draw_selected_for_edge_node(cr, dstX, dstY, radius, nodeR,
            nodeG, nodeB);
      else {
        if (xgraph_node_layout_is_marked_first(layout)
            && xgraph_node_layout_is_marked_second(layout))
          draw_marked_1_and_2_node(cr, dstX, dstY, radius, nodeR,
              nodeG, nodeB);
        else if (xgraph_node_layout_is_marked_first(layout))
          draw_marked_1_node(cr, dstX, dstY, radius, nodeR, nodeG,
              nodeB);
        else if (xgraph_node_layout_is_marked_second(layout))
          draw_marked_2_node(cr, dstX, dstY, radius, nodeR, nodeG,
              nodeB);
        else {
          draw_normal_node(cr, dstX, dstY, radius, lineWidth, nodeR,
              nodeG, nodeB);
        }
      }

      /* draw the node name, which is it number in most cases. */
      {
        cairo_text_extents_t te;
        int tx, ty;
        const gchar * text = xgraph_node_get_name(n);

        cairo_save(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 0);
        cairo_set_font_size(cr, 9);

        /* compute the individual text position from the rectangle needed by the
         * the node name. */
        cairo_text_extents(cr, text, &te);
        tx = x_pos - te.width / 2;
        ty = y_pos + te.height / 2;

        cairo_set_source_rgb(cr, textR, textG, textB);

        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, text);
        cairo_new_path(cr);

        cairo_restore(cr);
      }
    }
  } /* node visible */
}


/****************************************************************************/
/*       NAME : display_edge_circle                                         */
/*    PURPOSE : draws a graph edge from one node to itself                  */
/* PARAMETERS : * from, * to (source and dest node), state (PM/GW)          */
/*    CREATED : 16-AUG-1995 PS                                              */
/*   MODIFIED : 11-MAY-2000 WL: GTK+ port                                   */
/****************************************************************************/
void display_edge_circle(GraphWindow * gw, cairo_t * cr, XGraphNode * n,
    int state)
{
  XGraphNodeLayout * layout = xgraph_node_get_layout(n);

  gint x_pos = xgraph_node_layout_get_x(layout);
  gint y_pos = xgraph_node_layout_get_y(layout);

  /* IMPORTANT: also adjust the sensor routines on change! */

  /* RADIUS is not the radius. It's the width and height in the
   * non-cairo version. */

  double dx = PFEILLAENGE * cos(M_PI + WINKEL);
  double dy = PFEILLAENGE * sin(M_PI + WINKEL);
  gint radius = RADIUS / 2;

  assert (cr != NULL);

  cairo_arc(cr, x_pos, y_pos - RADIUS / 2.0, /* x,y */
      radius, /* radius */
      (135) * M_PI / 180.0, (45) * M_PI / 180.0 /* angles */);
  cairo_move_to(cr, x_pos - dx / 2, y_pos - RADIUS);
  cairo_line_to(cr, x_pos + dx / 2, y_pos - RADIUS - dy);

  cairo_move_to(cr, x_pos - dx / 2, y_pos - RADIUS);
  cairo_line_to(cr, x_pos + dx / 2, y_pos - RADIUS + dy);

  cairo_stroke(cr);
}


void draw_edge_path (cairo_t * cr,
           XGraphEdge * edge,
                     EdgePath * path,
                     gint node_radius,
                     gboolean two_way)
{
  int i;
  EdgePathPoint * points = path->points;
  gint n = path->pointCount;

  /* The path starts at the 'from' node and ends at the 'to' node. Thus,
   * we have to draw the last half of it for correct highlighting. */

  if ( !two_way) {
    /* We draw the first segment by hand. */
    draw_arrow_ex(cr, points[0].x, points[0].y, points[1].x, points[1].y,
        node_radius, NODE_START);
  }
  else { /* skip the first segment from the 'from' node */ }

  if (n >= 4) {
    gint start_i;

    if (two_way) start_i = edge_path_get_half_for_edge (path, edge);
    else start_i = 1;

    /* If the number of segments is odd, we have a tie with the last
     * segment. */

    cairo_move_to(cr, points[start_i].x, points[start_i].y);
    for (i = start_i+1; i < n-1 ; i++) {
      cairo_line_to(cr, points[i].x, points[i].y);
    }
    cairo_stroke(cr);
  }

  /* We draw the last segment by hand. This will contain the final arrow. */
  draw_arrow_ex(cr, points[n - 2].x, points[n - 2].y, points[n - 1].x,
      points[n - 1].y, node_radius, ARROW_END);

  return;
}

void display_edge_path (GraphWindow * gw, cairo_t * cr, XGraphEdge * edge, int state)
{
  XGraph * gr = gw->graph;
  EdgePath * path;

  XGraphEdge * edge_back = xgraph_edge_get_inverse(edge);

  /* Is there also a way back? */
  gboolean two_way = (edge_back != NULL);

  /* This will be the same for both directions. */
  path = edge_path_build (gr, edge);

  draw_edge_path (cr, edge, path,
                  12 /* node_radius */,
                  two_way);

  edge_path_destroy (path);

  return;
}

/****************************************************************************/
/*       NAME : display_edge                                                */
/*    PURPOSE : draws a graph edge                                          */
/* PARAMETERS : * gr (the graph), * edge, state (draw in pixmap or window)  */
/*    CREATED : 10-AUG-1995 PS                                              */
/*   MODIFIED : 10-MAY-2000 WL: GTK+ port                                   */
/****************************************************************************/
void display_edge(GraphWindow * gw, cairo_t * cr, XGraphEdge * edge, int state,
    gboolean draw_colors)
{
  XGraph * gr = gw->graph;
  XGraphNode * from = xgraph_edge_get_from_node(edge);
  XGraphNode * to = xgraph_edge_get_to_node(edge);
  XGraphEdgeLayout * layout = xgraph_edge_get_layout(edge);

  if (!from || !to)
    return;

  cairo_save(cr);

#if 0
#warning For compatibility-reasons with the edge.state field.
  {
    edge->flags &= ~EDGE_MARKED_FIRST;
    edge->flags &= ~EDGE_MARKED_SECOND;

    if (SELECTED == edge->state)
    edge->flags |= EDGE_MARKED_FIRST;
    else if (SELECTED_SECOND == edge->state)
    edge->flags |= EDGE_MARKED_SECOND;
    else if (SELECTED_BOTH == edge->state)
    edge->flags |= EDGE_MARKED_FIRST | EDGE_MARKED_SECOND;
  }
#endif

  /* Just draw visible edges. */
  if (xgraph_edge_layout_is_visible(layout)) {
    gint lineWidth = 1;
    double r = .0, g = .0, b = .0; /* black */
    gboolean useDash = FALSE;

    cairo_save(cr);

    if (draw_colors && xgraph_edge_is_selected(edge)) {
      r = 1.0; /* red */
      lineWidth = 2;
    }
    if (draw_colors && xgraph_edge_layout_is_highlighted(layout)) {
      b = 1.0; /* blue */
      lineWidth = 2;
    }
    if (xgraph_edge_layout_is_marked_first(layout))
      lineWidth = 3;
    if (xgraph_edge_layout_is_marked_second(layout))
      useDash = TRUE;

    cairo_set_line_width(cr, lineWidth);
    cairo_set_source_rgb(cr, r, g, b);
    if (useDash) {
      double dashlen = 5.0;
      cairo_set_dash(cr, &dashlen, 1, 2.5 /*offset*/);
    }


    if (! xgraph_edge_layout_is_simple(layout) && from != to) {
      display_edge_path(gw, cr, edge, state);
    } else /* draw a single edge */{
      /* it's a reflexive edge, or not? */
      if (from == to) {
        display_edge_circle(gw, cr, from, state);
      } else {
        /* Is there an edge back as well? */
        gboolean two_way = xgraph_has_edge(gr, to, from);

        XGraphNodeLayout * fromLayout = xgraph_node_get_layout (from);
        XGraphNodeLayout * toLayout = xgraph_node_get_layout (to);

        gint from_x_pos = xgraph_node_layout_get_x (fromLayout);
        gint from_y_pos = xgraph_node_layout_get_y (fromLayout);
        gint to_x_pos   = xgraph_node_layout_get_x (toLayout);
        gint to_y_pos   = xgraph_node_layout_get_y (toLayout);
        gint radius     = xgraph_node_layout_get_radius(fromLayout);
        if (radius != xgraph_node_layout_get_radius(toLayout))
          printf ("display_edge: Nodes have different radii. Ignored.\n");

        radius ++; // Manual adjustment

        if (two_way) /* draw half of the edge */{
          draw_arrow_ex(cr, from_x_pos, from_y_pos, to_x_pos,
              to_y_pos, radius /* node radius */, ARROW_END | HALF);
        } else {
          draw_arrow_ex(cr, from_x_pos, from_y_pos, to_x_pos,
              to_y_pos, radius /* node radius */, ARROW_END
                  | NODE_START);
        }
      }
    }

    cairo_restore(cr);
  }
}


/*! Labels the given graph, with the given label.
 *
 * \author wl,stb
 * \date 17-JUN-1996
 * \param gw The graph window.
 * \param label The label to use. Must not be NULL.
 */
void label_graph(GraphWindow * gw, Label * label)
{
  LabelIter * iter = label_iterator (label);
  GString * text = g_string_new ("");
  int nodeCount;
  XGraph * gr = gw->graph;
  int radius = 11;
  int left;

  nodeCount = xgraph_get_node_count(gr);
  if (0 == nodeCount)
    return;
  left = nodeCount;

  for ( ; label_iter_is_valid(iter) && left > 0; label_iter_next(iter), --left) {
    gint screenNo = label_iter_number(iter);
    XGraphNode * node = xgraph_get_node_by_name(gr, screenNo);

    if (node) {
      cairo_t * cr = gw->crOffset;
      cairo_text_extents_t te;
      int tx, ty;

      XGraphNodeLayout * layout = xgraph_node_get_layout (node);
      gint x_pos = xgraph_node_layout_get_x (layout);
      gint y_pos = xgraph_node_layout_get_y (layout);

      label_iter_name (iter, text);

      cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 0);
      cairo_set_font_size(cr, 9);

      /* compute the individual text position from the rectangle needed by the
       * the node name. */
      cairo_text_extents(cr, text->str, &te);
      tx = x_pos + radius + 5 - te.width / 2;
      ty = y_pos + radius + 5 + te.height / 2;

      cairo_move_to(cr, tx, ty);
      cairo_show_text(cr, text->str);

      cairo_identity_matrix(cr);
      cairo_new_path(cr);
    }
  }

  label_iter_destroy (iter);
  g_string_free (text, TRUE);
}

/*!
 * */
void _graph_window_display_layer(GraphWindow * gw, int originx, int originy,
    int x, int y, int top, int bottom) {
  if (! graph_window_has_graph(gw)) return;
  else {
    /* we draw directly on the window. */
    cairo_t * cr = gdk_cairo_create(GTK_WIDGET(gw->drawingarea)->window);
    XGraph * gr = gw->graph;

    cairo_set_source_rgb(cr, .0, .0, .0); /* black */
    /* Use anti-aliasing, or not. */
    if (prefs_get_int("settings", "use_anti_aliasing", FALSE))
      cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    else
      cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

    /* The order of translate, clip and scale is mandatory, because the clipping
     * rect. depends on the viewport position, but must not be scaled. */

    cairo_rectangle(cr, x, y, top, bottom);
    cairo_clip(cr);

    cairo_translate(cr, -originx, -originy);

    cairo_scale(cr, gw->zoom, gw->zoom);

    /* Must also be  scaled. */
    cairo_set_line_width(cr, 1);

    /* Display all nodes. */
    XGRAPH_FOREACH_NODE(gr,cur,iter,{
      display_node(cr, cur, PM);
    });

    /* Display all selected or highlighted edges. */
    XGRAPH_FOREACH_EDGE(gr,cur,iter,{
      XGraphEdgeLayout * layout = xgraph_edge_get_layout(cur);
      if (xgraph_edge_is_selected(cur) || xgraph_edge_layout_is_highlighted(layout))
        display_edge (gw, cr, cur, PM, TRUE);
    });

    cairo_destroy(cr);
  }
}

/****************************************************************************/
/*       NAME : display_graph                                               */
/*    PURPOSE : draws a graph (nodes, edges)                                */
/* PARAMETERS : * gl (the graph)                                            */
/*    CREATED : 1995 PS                                                     */
/*   MODIFIED : 11-MAY-2000 WL: GTK+ port                                   */
/*              09-MAR-2008 STB: Labeling enhancements.                     */
/****************************************************************************/
void display_graph(GraphWindow * gw)
{
 XGraph * gr = gw->graph;

 clear_graph_pixmap(gw);

  /* Display all edges. */
  XGRAPH_FOREACH_EDGE(gr,cur,iter,{
    XGraphEdgeLayout * layout = xgraph_edge_get_layout(cur);
    display_edge(gw, gw->crOffset, cur, PM, FALSE /*no highlights*/);
  });

  /* We label the graph if it has a label. */
  if (gw->label)
    label_graph(gw, gw->label);
}


/****************************************************************************/
/*       NAME : invalidateVisibleRectGW                                     */
/*    PURPOSE : If redraw is TRUE, the backpixmap is cleared, the grid is   */
/*              drawn (if (grid_on)), the graph is drawn.                   */
/*              Then an expose event is request to copy the backpixmap to   */
/*              the screen.                                                 */
/* PARAMETERS : redraw (whether the backpixmap should be updated)           */
/*    CREATED : 15-MAY-2000 WL                                              */
/****************************************************************************/
#include <gdk-pixbuf/gdk-pixbuf.h>

static void invalidateVisibleRectGW(GraphWindow * gw, int redraw)
{
  GdkRectangle visrect;
  double zoom = graph_window_get_zoom(gw);

  if (NULL == gw->crOffset) {
    /* Call graph_window_redraw to create an offset buffer to draw in. */
    graph_window_redraw(gw, NEW);
    return; /* We'll be called back. */
  }

  visrect = graph_window_get_visible_rect(gw);
  visrect.x = visrect.y = 0;

  if (redraw)
    display_graph(gw);

  /* do ZOOM if neccessary (case zoom level < 1.0) */
  if (.0 < zoom && zoom < 1.0) {
    XGraph * gr = graph_window_get_graph(gw);
    GdkRectangle graphrect = xgraph_get_display_rect(gr);
    gint width = MAX(visrect.width / gw->zoom + 0.5, graphrect.width);
    gint height = MAX(visrect.height / gw->zoom + 0.5, graphrect.height);

    GdkPixbuf * pixbuf = gdk_pixbuf_get_from_drawable(NULL, gw->backpixmap,
        NULL, /*colormap*/
        0, 0, /*src*/
        0, 0, /*dest*/
        width, height);

    GdkPixbuf
        * pixbufScaled =
            gdk_pixbuf_scale_simple(pixbuf, gw->zoom
                * gdk_pixbuf_get_width(pixbuf) + 0.5, gw->zoom
                * gdk_pixbuf_get_height(pixbuf) + 0.5,
                GDK_INTERP_HYPER /* GDK_INTERP_BILINEAR */);

    g_object_unref(G_OBJECT(pixbuf));

    gdk_pixbuf_render_to_drawable(pixbufScaled, gw->backpixmap,
        GTK_WIDGET(gw->drawingarea)->style->white_gc, 0, 0,/*src*/
        0, 0,/*dest*/
        -1, -1,/*width,height*/
        GDK_RGB_DITHER_NONE, 0, 0);

    g_object_unref(G_OBJECT(pixbufScaled));

    gtk_widget_draw(gw->drawingarea, &visrect); /* emits an expose event */
  } else /* don't zoom */ {
    gtk_widget_queue_draw_area(gw->drawingarea, visrect.x, visrect.y,
        visrect.width, visrect.height);
  }
}

/****************************************************************************/
/*       NAME : redraw_graph                                                */
/*    PURPOSE : refreshes the complete graph window                         */
/* PARAMETERS : * graph (the current graph), state (OLD/NEW)                */
/*    CREATED : 08-FEB-1995 PS                                              */
/*   MODIFIED : 24-JUN-1995 PS: OLD/NEW                                     */
/*            : 13-MAY-2000 WL: GTK+ port                                   */
/****************************************************************************/

void graph_window_redraw(GraphWindow * gw, GraphWindowRedrawType type)
{

  if (!graph_window_is_visible(gw))
    return;
  else if (! graph_window_has_graph(gw)) {
    /* Draw something explaining to the screen, if there is no graph. */
    if (gw->crOffset) {
      clear_graph_pixmap(gw);
      gtk_widget_queue_draw (gw->drawingarea);
    }
  }
  else {
    GdkRectangle visrect, graphrect;
    GtkAdjustment * adjust;
    XGraph * gr = graph_window_get_graph(gw);
    gint width, height;

    graphrect = xgraph_get_display_rect(gr);

    if (xgraph_is_correspondence(gr))
      gw->editable = FALSE;
    else
      gw->editable = TRUE;

    /* (maybe) update anti-aliasing policy */
    if (gw->crOffset) {
      if (prefs_get_int("settings", "use_anti_aliasing", FALSE))
        cairo_set_antialias(gw->crOffset, CAIRO_ANTIALIAS_DEFAULT);
      else
        cairo_set_antialias(gw->crOffset, CAIRO_ANTIALIAS_NONE);
    }

    if (gw->cr) {
      if (prefs_get_int("settings", "use_anti_aliasing", FALSE))
        cairo_set_antialias(gw->cr, CAIRO_ANTIALIAS_DEFAULT);
      else
        cairo_set_antialias(gw->cr, CAIRO_ANTIALIAS_NONE);
    }

    visrect = graph_window_get_visible_rect(gw);

    width = MAX(visrect.width / gw->zoom + 0.5, graphrect.width);
    height = MAX(visrect.height / gw->zoom + 0.5, graphrect.height);


    /* Create new backing pixmap. On failure resize graph to fit in window
     and try again. The backpixmap is then of window size and should be
     creatable. */
    {
      gint offsetWidth, offsetHeight;

      if (gw->backpixmap)
        gdk_drawable_get_size(GDK_DRAWABLE(gw->backpixmap), &offsetWidth,
            &offsetHeight);

      /* if there is no offset buffer, or the required size has changed, destroy the
       * previous offset buffer (if there is one), and create a new one. */
      if ((gw->backpixmap && (offsetWidth != width || offsetHeight != height))
          || !gw->backpixmap) {

        if (gw->backpixmap) {
          gdk_pixmap_unref(gw->backpixmap);
          gw->backpixmap = NULL;
          cairo_destroy(gw->crOffset);
        }

        gw->backpixmap = gdk_pixmap_new(gw->drawingarea->window, width,
            height, -1);
        if (!gw->backpixmap) {
          printf("Xmalloc ERROR (maybe the graph's dimension is to big) "
            "=> rescaling graph\n");
          xgraph_fill_area (gr, graph_window_get_display_width(gw),
              graph_window_get_display_height(gw));

          graphrect = xgraph_get_display_rect(gr);
          width = graphrect.width;
          height = graphrect.height;
          gw->backpixmap = gdk_pixmap_new(gw->drawingarea->window, width,
              height, -1);

          assert (gw->backpixmap != NULL); /* we give up */
        }

        {
          cairo_t *cr = gw->crOffset = gdk_cairo_create(gw->backpixmap);

          cairo_set_source_rgb(cr, .0, .0, .0); /* black */
          /* Use anti-aliasing, or not. */
          if (prefs_get_int("settings", "use_anti_aliasing", FALSE))
            cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
          else
            cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
          cairo_set_line_width(cr, 1);
          }
        }
    }


    /* adjust scrollbar values */
    adjust = gtk_range_get_adjustment(GTK_RANGE (gw->hscroll));
    adjust->upper = MAX(visrect.width, graphrect.width * gw->zoom);
    adjust->page_size = visrect.width;
    adjust->step_increment = 2;
    adjust->page_increment = adjust->page_size;
    gtk_adjustment_changed(adjust);

    adjust = gtk_range_get_adjustment(GTK_RANGE (gw->vscroll));
    adjust->upper = MAX(visrect.height, graphrect.height * gw->zoom);
    adjust->page_size = visrect.height;
    adjust->step_increment = 2;
    adjust->page_increment = adjust->page_size;
    gtk_adjustment_changed(adjust);

    if (type == NEW) { /* graph has been drawn "from scratch", zero scrollbars */
      adjust = gtk_range_get_adjustment(GTK_RANGE (gw->hscroll));
      adjust->value = 0;
      gtk_adjustment_changed(adjust);
      adjust = gtk_range_get_adjustment(GTK_RANGE (gw->vscroll));
      adjust->value = 0;
      gtk_adjustment_changed(adjust);
    }
    invalidateVisibleRectGW(gw, TRUE);
    /* draw graph to pixmap and copy pixmap to screen */

  }
}


/*!
 * See graph_window_display_edge. */
void graph_window_display_node (GraphWindow * gw, XGraphNode * node)
{ return; }

/*!
 * No longer necessary. Dynamic elements, as highlighted edges
 * and nodes will now be drawn on expose events. So triggering
 * such an event will do the job (See gtk_widget_queue_draw). */
void graph_window_display_edge (GraphWindow * gw, XGraphEdge * edge)
{ return; }

void graph_window_invalidate_display (GraphWindow * gw, gboolean displayGraph)
{
  invalidateVisibleRectGW (gw, displayGraph);
}

/* updates the portion of the window, without redrawing the display. It's simply
 * like a flip operation. */
void graph_window_expose (GraphWindow * gw, GdkEventExpose * event)
{
  // Get GraphNode Limit
  int graphNodeLimit = prefs_get_int("settings", "graphNode_display_limit", 2 << 26);

  // GraphNodeLimit reached, display Message
  if(xgraph_get_node_count(graph_window_get_graph(gw)) > graphNodeLimit) {
    clear_graph_pixmap(gw);

    char errMessage[100];
    sprintf(errMessage,"This Graph can not be displayed, because the node limit of %i was reached.", graphNodeLimit);

    _graph_window_draw_message(gw,errMessage);
  }

  if (NULL == gw->backpixmap) {
    graph_window_redraw (gw, NEW);
  }
  else {

    GdkRectangle *rcs;
    gint rcCount = 0;
    gint i;
    GdkPoint origin = graph_window_get_visible_origin (gw);
    GtkWidget *da /*drawingarea*/ = GTK_WIDGET(gw->drawingarea);

    gdk_region_get_rectangles (event->region, &rcs, &rcCount);

    for (i = 0 ; i < rcCount ; i ++) {
#if 0
      printf ("exposing rect (backpixmap: %p): %d:%d,  %d x %d\n", gw->backpixmap,
          rcs[i].x, rcs[i].y, rcs[i].width, rcs[i].height);
      printf ("origin is %d:%d\n", origin.x, origin.y);
#endif

      gdk_draw_pixmap (da->window,
                       da->style->fg_gc [GTK_WIDGET_STATE (da)],
                       gw->backpixmap,
                       origin.x + rcs[i].x, origin.y + rcs[i].y,
                       rcs[i].x, rcs[i].y,
                       rcs[i].width, rcs[i].height);

      if(xgraph_get_node_count(graph_window_get_graph(gw)) > graphNodeLimit)
        continue;

      /* draw the more versatile stuff. */
      _graph_window_display_layer
        (gw, origin.x, origin.y, rcs[i].x, rcs[i].y,
          rcs[i].width, rcs[i].height);

    }

    g_free (rcs);
  }
}

/*
 * Draw message to GraphWindo
 */
void _graph_window_draw_message (GraphWindow * gw, gchar * text) {

  GdkRectangle visrect;





  cairo_t * cr = gw->crOffset;
  cairo_text_extents_t te;
  int tx, ty;

  visrect = graph_window_get_visible_rect(gw);


  visrect.x = visrect.y = 0;

  if (cr == NULL){
      gw->backpixmap = gdk_pixmap_new(gw->drawingarea->window, 1000.0,
                           1000.0, -1 /* depth */);

      gw->crOffset = gdk_cairo_create(gw->backpixmap);
      cr = gw->crOffset;

  }

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, 0);
  cairo_set_font_size(cr, 12);



  cairo_text_extents(cr, text, &te);
  tx = visrect.width / 2 - te.width / 2;
  ty = visrect.height / 2 - te.height / 2;

  cairo_move_to(cr, tx, ty);
  cairo_show_text(cr,  text);

  cairo_identity_matrix(cr);
  cairo_new_path(cr);

}
