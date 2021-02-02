/*
 * Eps.c
 *
 *  Copyright (C) 1994-2011, University of Kiel, Germany
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

#include "Eps.h"
#include "Relview.h"
#include "Relation.h"
#include "Graph.h"
#include "Kure.h"
#include "label.h"
#include "RelationWindow.h"

//#define _EPS_VERBOSE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo/cairo.h>
#include <cairo/cairo-ps.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>


typedef IOHandler EpsHandler;

/* Physical dimension of the PostScript page (in points). */
#define PAGE_WIDTH_PHYSICAL 595
#define PAGE_HEIGHT_PHYSICAL 842

/* Actual dimension of the PostScript page (in points) that is used to draw
 * the relation. Fixes a bug with the BoundingBox of the EPS file. */
#define PAGE_WIDTH (PAGE_WIDTH_PHYSICAL - 10)
#define PAGE_HEIGHT (PAGE_HEIGHT_PHYSICAL - 10)

/* Size of the "bits" for a relation. Doesn't have a physical dimension. */
#define REL_RECT_SIZE 20

/* Size of the line width for the grid. TODO: Currently useless. You can
 * achieve the same effect by decreasing \ref REL_RECT_SIZE. */
#define REL_GRID_WIDTH 1


// test
static double _rel_width;
static double _rel_height;

/*!
 * Writes the given relation together with its labels into an EPS file with the
 * given filename.
 *
 * \author stb
 *
 * \param row_labels The labels for the rows. Can be NULL.
 * \param col_labels The labels for the column. Can be NULL.
 * \param gray Saturation for the bits set (e.g. 0.6). Must be in [0,1]. Unset
 *             bits are white (1.0) by default.
 * \param angle The rotation angle for the column labels (e.g. -PI/4). An angle
 *              of 0 means no rotation at all.
 * \param font_familiy E.g. "Sans" or "Sans-Serif". If unsure, use "Sans".
 * \param font_scale Enlarges the font it greater than 1.0, shrinks it
 *                   otherwise.
 */
static gboolean _write_eps_to_file (const gchar * filename, KureRel * impl,
		Label * rowLabel, Label * colLabel, gdouble gray /*[0,1]*/,
		gdouble angle, const gchar * font_family, double font_scale)
{
	cairo_surface_t * surface;
	cairo_t * cr;
	int delta = REL_RECT_SIZE; /* size of the rects. */
	int rows = kure_rel_get_rows_si(impl),
			cols = kure_rel_get_cols_si(impl);

	int xoff = 1, yoff = 1;
	int page_width, page_height;
	int line_width = REL_GRID_WIDTH;

	double factor;
	double rel_width = line_width + cols * (delta + line_width),
		   rel_height = line_width + rows * (delta + line_width);

    // Save RelSize
	_rel_width = rel_width;
	_rel_height = rel_height;

	page_width = PAGE_WIDTH;
	page_height = PAGE_HEIGHT;

	/* We save half of the page for the labels. This should be fair enough! */
	factor = MIN(1.0, page_width / rel_width / 2.5); // @mku: Half was not fair enough -> 2.5
	factor = MIN(factor, page_height / rel_height / 2.0);

#ifdef _EPS_VERBOSE
	printf ("_write_eps_to_file"
			"               delta : %d\n"
			"   page_width/height : %d / %d\n"
			"              factor : %lf\n"
			"    rel_width/height : %lf / %lf\n"
			"          line_width : %d\n"
			"              x/yoff : %d / %d\n"
			"               angle : %lf\n"
			"        font familiy : \"%s\"\n"
			"   font scale factor : %lf\n"
			"                gray : %lf\n",
			delta, page_width, page_height,
			factor, rel_width, rel_height, line_width, xoff, yoff,
			angle, font_family, font_scale, gray);
#endif

	surface = cairo_ps_surface_create (filename, PAGE_WIDTH_PHYSICAL, PAGE_HEIGHT_PHYSICAL);
	if ( !surface)
		return FALSE;
	cr = cairo_create (surface);
	cairo_ps_surface_set_eps (surface, TRUE);

	/* Move the relation to the right bottom of the page. This simplifies the
	 * positioning of the labels, because we don't have to know their size in
	 * advance. */
	cairo_translate (cr, page_width * 0.5, page_height * 0.5);

	/* We use scaling to fit the complete relation on the page. Another problem
	 * is to scale the page so much that also the labels fit on it. */
	cairo_scale (cr, factor, factor);

	cairo_ps_surface_dsc_begin_page_setup (surface);

	//cairo_set_line_width(cr, line_width);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	/* draw the grid cells column-wise */
	{
	  gint i, j;
	  GString * text = g_string_new ("");

		/* Draw the bits set */
		cairo_set_source_rgb(cr, gray, gray, gray);
		for (j = 0; j < cols; j ++) {
		  for (i = 0; i < rows; i ++) {
			if (kure_get_bit_si(impl, i,j,NULL)) {
			  double x = xoff + j*(delta+line_width) + line_width/2.0,
					  y = yoff + i*(delta+line_width) +line_width/2.0;

			  cairo_rectangle(cr, x, y, delta - line_width/4.0, delta - line_width/4.0);
			}
		  }
		}
		cairo_fill(cr);

		/* Draw the complete grid at once. */
		cairo_set_source_rgb(cr, 0, 0, 0);

		/* One cannot use 4 lines to draw the outermost border in EPS.
		 * This would cause problem with line dimensions. */
		cairo_rectangle(cr, xoff, yoff, cols * (line_width+delta),
				rows * (line_width+delta));

		for (j = 1; j <= cols; j ++) /* vertical grid lines */{
		  cairo_move_to(cr, xoff + j*(delta+line_width), yoff);
		  cairo_rel_line_to(cr, 0, rows * (line_width+delta));
		}
		for (i = 1; i <= rows; i ++) /* horizontal grid lines */{
		  cairo_move_to(cr, xoff, yoff + i*(delta+line_width));
		  cairo_rel_line_to(cr, cols * (line_width+delta), 0);
		}
		cairo_stroke(cr);


		/* Reset the color to black */
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_select_font_face (cr, font_family, CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size (cr, 12 * font_scale);

		if (rowLabel) {
			for (i = 0 ; i < rows ; i ++) {
				const gchar * l = label_get_nth (rowLabel, i+1, text);

				if (l) {
					cairo_text_extents_t fe;
					cairo_text_extents(cr, l, &fe);
					cairo_move_to (cr, xoff-fe.width-fe.x_bearing-10/*margin*/,
							yoff + line_width + i*(delta+line_width)
							+ fe.height + (delta - fe.height) / 2);
					cairo_show_text (cr, l);
				}
			}
		}

		if (colLabel) {
			for (j = 0 ; j < cols ; j ++) {
				const gchar * l = label_get_nth (colLabel, j + 1, text);

				if (l) {
					cairo_text_extents_t fe;
					cairo_text_extents(cr, l, &fe);
					cairo_move_to (cr, xoff + j*(delta+line_width) + fe.height
							+ (delta - fe.height) / 2.0 - (delta / 10.0),
							-3 /*margin*/);
					cairo_rotate(cr, angle);
					cairo_show_text (cr, l);
					cairo_rotate(cr, -angle); // rotate back
				}
			}
		}

		g_string_free (text, TRUE);
	}

	cairo_surface_show_page (surface);

    cairo_destroy (cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy (surface);

    return TRUE;
}



#define EPS_NODES_RADIUS 10
#define EPS_EDGES_ARROW_ANGLE 0.50
#define EPS_EDGES_ARROW_LENGTH 5.0

static int width;
static int height;
static int offset = 55; // zum Bewegen der Zeichnung des Graphen vom Rand
static int max_y; /* max_y, da dir Koordinatensysteme X und postscript unterschiedlich */

static char graph_epfs_header_1 [] =
"%!PS-Adobe-2.0 EPSF-2.0\n"
"%%Title: relview file\n"
"%%Creator: relview-8.2\n"
"%%CreationDate: today\n";

static char graph_epfs_header_2 [] =
"%%Pages: 0-1\n"
"%%DocumentFonts: Times-Roman\n";

static char graph_epfs_header_3 [] =
"/DrawLine %% ( n n n n n -- )\n"
"{ setlinewidth moveto lineto stroke} def\n"
"\n"
"/DrawEdgeCircle %% ( n n n -- )\n"
"{ setlinewidth Radius 1.5 mul add Radius 1.5 mul 310 230 arc stroke } def\n"
"\n"
"/DrawCircleArrow %% ( n n n n n --)\n"
"{ setlinewidth Radius 3 mul add moveto rlineto stroke } def\n"
"\n"
"/DrawCircle %% ( n n n -- )\n"
"{ setlinewidth Radius 0 360 arc stroke} def\n"
"\n"
"\n"
"/DrawInversCircle %% ( n n n -- )\n"
"{ gsave 0.0 setgray setlinewidth Radius 0 360 arc fill} def\n"
"\n"
"/DrawQuad %% ( n n n -- )\n"
"{ setlinewidth moveto dup 0 exch rlineto dup 0 rlineto neg 0 exch rlineto closepath stroke } def\n"
"\n"
"\n"
"/DrawInversQuad %% ( n n n -- )\n"
"{ gsave 0.0 setgray setlinewidth moveto dup 0 exch rlineto dup 0 rlineto neg 0 exch rlineto closepath fill } def\n"
"\n"
"/Compute_XY %% ( n f -- n )\n"
"{ Radius mul add } def\n"
"\n"
"/CharHeight %% ( -- n )\n"
"{ (8) stringwidth exch pop } def\n"
"\n"
"/CharWidth %% ( -- n )\n"
"{ (8) stringwidth pop 2 div } def\n"
"\n"
"/Coord %% ( n n n -- n n )\n"
"{ CharWidth mul sub exch Radius 4 div  sub } def\n"
"\n"
"/dashed %% ( )\n"
"{ [4 4] 0 setdash } def\n"
"\n"
"/notdashed %% ( )\n"
"{ [] 0 setdash } def\n"
"\n"
"/Arrow %% ( n n n n n n -- )\n"
"{ notdashed moveto lineto lineto stroke } def\n"
"\n"
"/Times-Roman findfont Radius  scalefont setfont\n"
"\n"
"0 setgray\n"
"0 setlinejoin 10 setmiterlimit\n"
"1.000 1.000 scale\n";


/****************************************************************************/
/* NAME: eps_nodes                                                          */
/* FUNKTION: Schreibt die Zeichenoperationen der Knoten in ein eps-file     */
/* UEBERGABEPARAMETER: nodelistptr (auf die Knotenliste) und FILE *         */
/* RUECKGABEWERT: keiner                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 22.09.95                                                             */
/* LETZTE AENDERUNG AM: 25.09.95                                            */
/*                      07-MAR-2008 STB: Labeling enhancements              */
/****************************************************************************/
static void _eps_nodes (XGraph * gr, Label * label, FILE * fp)
{
	int linewidth = 1;
	GString * text = g_string_new ("");

	XGRAPH_FOREACH_NODE(gr, cur, iter, {
		XGraphNodeLayout * layout = xgraph_node_get_layout(cur);
		gint x = xgraph_node_layout_get_x(layout) + offset;
		gint y = max_y - xgraph_node_layout_get_y(layout) - offset;
		const gchar * node_name = xgraph_node_get_name(cur);
		gint node_nr = atoi (node_name); // 1-indexed

		if (label) {
			const char * labelName = label_get_nth (label, node_nr, text);
			if (labelName) {
		        fprintf (fp, "newpath %d Radius 1.5 mul add %d "
		                 "Radius 1.5 mul sub moveto (%s) show\n",
		                 x, y, labelName);
			}
		}

		if (xgraph_node_layout_is_marked_first(layout)
				&& xgraph_node_layout_is_marked_second(layout)) {
			fprintf(fp,
			         "newpath Radius 2 mul %d Radius sub %d Radius sub %d  DrawInversQuad\n",
			         x, y, linewidth + 1);
			fprintf(fp, "newpath %d %d %d Coord moveto 1.0 setgray (%s) show grestore \n",
				 y, x, (int) strlen (node_name), node_name);
		}
		else if (xgraph_node_layout_is_marked_first(layout)) {
			fprintf (fp, "newpath %d %d %d DrawInversCircle \n", x, y, linewidth);

			fprintf (fp, "%d %d %d Coord moveto 1.0 setgray (%s) show grestore\n",
			               y, x, (int) strlen (node_name), node_name);
		}
		else if (xgraph_node_layout_is_marked_second(layout)) {
			fprintf (fp, "newpath Radius 2 mul %d Radius sub %d Radius sub %d  DrawQuad\n",
			         x, y, linewidth + 1);
			fprintf (fp, "newpath %d %d %d Coord moveto (%s) show\n",
			               y, x, (int) strlen (node_name), node_name);
		}
		else {
			fprintf (fp, "newpath %d %d %d DrawCircle %d %d %d Coord moveto (%s) show\n",
			 x, y, linewidth, y, x, (int) strlen (node_name), node_name);
		}
	});

	g_string_free (text, TRUE);
}

/****************************************************************************/
/* NAME: eps_single_edge                                                    */
/* FUNKTION: schreibt die Zeichenoperationen einer Kanten in ein eps-file   */
/* UEBERGABEPARAMETER: Graph * (der graph), FILE * (auf das File)           */
/* RUECKGABEWERT: keiner                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 27.06.95                                                             */
/* LETZTE AENDERUNG AM: 05.03.1999                                          */
/* (Ergaenzung der bool-Parameters first_part, fuers erste Teilstueck)      */
/****************************************************************************/

/*!
 * \param from_is_helper If FALSE, the line does not start exactly at 'from' but
 *                       is moved a little towards 'to'.
 * \param to_is_helper If FALSE an arrow is drawn.
 */
static void _eps_single_edge (XGraphEdge * edge, XGraphEdgeLayout * layout, const LayoutPoint * from,
		const LayoutPoint * to, gboolean is_circle, gboolean from_is_helper,
		gboolean to_is_helper, gboolean first_part, FILE * fp)
{
	/* Multiplier for the distance between a node and the segment
	 * ending/starting at it. Actual distance is
	 * 		node_distance_factor * EPS_NODES_RADIUS.
	 */
	const gdouble node_distance_factor = 1.1;

	char dash_command [2] [20] = {"notdashed", "dashed"};

	double from_x = from->x + offset, from_y = (max_y - from->y) - offset;
	double to_x = to->x + offset, to_y = (max_y - to->y) - offset;

	/* Distance between to and from */
	double c = sqrt (pow(to_y - from_y, 2) + pow(to_x - from_x, 2));
	double alpha;

	double delta_x_from, delta_y_from;
	double delta_x_to, delta_y_to;

	gboolean is_dashed = FALSE;
	gint line_width = 1;

	double dx,dy;

	XGraphNode * from_node = xgraph_edge_get_from_node(edge),
			*to_node = xgraph_edge_get_to_node(edge);
	const gchar * from_name = xgraph_node_get_name(from_node),
			*to_name = xgraph_node_get_name(to_node);

	if (c != .0)
		alpha = asin (fabs (to_y - from_y) / c);
	else alpha = .0;

#if defined _EPS_VERBOSE
#define DUMP(var,spec) printf ("%20s : "spec"\n", #var, var)
	DUMP(from->x,"%f");
	DUMP(from->y,"%f");
	DUMP(to->x,"%f");
	DUMP(to->y,"%f");
	DUMP(is_circle, "%d");
	DUMP(from_is_helper,"%d");
	DUMP(to_is_helper,"%d");
	DUMP(first_part,"%d");

	DUMP(from_x, "%lf");
	DUMP(from_y, "%lf");
	DUMP(to_x, "%lf");
	DUMP(to_y, "%lf");

	DUMP(c, "%lf");
	DUMP(alpha, "%lf");
	DUMP(delta_x_from, "%lf");
	DUMP(delta_y_from, "%lf");
	DUMP(delta_x_to, "%lf");
	DUMP(delta_y_to, "%lf");
	DUMP(from_name, "\"%s\"");
	DUMP(to_name, "\"%s\"");
#endif

	if (to_y <= from_y) {
		if (to_x < from_x) alpha += M_PI;
		else if (to_x > from_x) alpha = 2 * M_PI - alpha;
		else if (to_x == from_x) alpha = 3 * M_PI_2;
	}
	else {
		if (to_x < from_x) alpha = M_PI - alpha;
		else if (to_x > from_x) { /* nothing */ }
		else if (to_x == from_x) { alpha = M_PI_2; }
	}

	if (from_is_helper) {
		delta_x_from = delta_y_from = 0;
	} else {
		delta_x_from = node_distance_factor * EPS_NODES_RADIUS * cos (alpha);
		delta_y_from = node_distance_factor * EPS_NODES_RADIUS * sin (alpha);
	}

	if (to_is_helper) {
		delta_x_to = delta_y_to = 0;
	} else {
		delta_x_to = node_distance_factor * EPS_NODES_RADIUS * cos (alpha);
		delta_y_to = node_distance_factor * EPS_NODES_RADIUS * sin (alpha);
	}

	/* Set the line width and the dashing w.r.t the edge's layout. */
	if (xgraph_edge_layout_is_marked_first(layout)
			&& xgraph_edge_layout_is_marked_second(layout)) {
		is_dashed = TRUE;
		line_width = 4;
	}
	else if (xgraph_edge_layout_is_marked_first(layout)) {
		line_width = 4;
	}
	else if (xgraph_edge_layout_is_marked_second(layout)) {
		is_dashed = TRUE;
	}

	if (is_circle)
	{
		fprintf(fp, "newpath %d %d %d DrawEdgeCircle %% (%s) -> (%s)\n",
				(int) from_x, (int) to_y, line_width, from_name,
				to_name);

		dx = EPS_EDGES_ARROW_LENGTH * cos(M_PI + EPS_EDGES_ARROW_ANGLE);
		dy = EPS_EDGES_ARROW_LENGTH * sin(M_PI + EPS_EDGES_ARROW_ANGLE);

		fprintf(fp, "newpath %d %d %d %d %d DrawCircleArrow\n", (int) dx,
				(int) dy, (int) (from_x - 0.5 * dx), (int) from_y, line_width);

		dx = EPS_EDGES_ARROW_LENGTH * cos(M_PI - EPS_EDGES_ARROW_ANGLE);
		dy = EPS_EDGES_ARROW_LENGTH * sin(M_PI - EPS_EDGES_ARROW_ANGLE);

		fprintf(fp, "newpath %d %d %d %d %d DrawCircleArrow\n", (int) dx,
				(int) dy, (int) (from_x - 0.5 * dx), (int) from_y, line_width);
	}
	else {
		/* If it's a new edge, start a new path in the EPS as well. */
		if (first_part) {
			fprintf(fp, "newpath %d setlinewidth %s %d %d moveto ", line_width,
					dash_command[is_dashed?1:0], (int) (from_x + delta_x_from),
					(int) (from_y + delta_y_from));

		}

		/* In case of a segment, just draw the line segment. Otherwise, also
		 * draw the arrow. */
		if ( to_is_helper ) {
			fprintf (fp, " %d %d lineto ", (int) (to_x), (int) (to_y));
		} else {
			gdouble arrow_start_x, arrow_start_y, arrow_top_x, arrow_top_y,
			arrow_end_x, arrow_end_y;

			dx = EPS_EDGES_ARROW_LENGTH * cos (alpha + M_PI + EPS_EDGES_ARROW_ANGLE);
			dy = EPS_EDGES_ARROW_LENGTH * sin (alpha + M_PI + EPS_EDGES_ARROW_ANGLE);

			arrow_start_x = to_x - delta_x_to + dx;
			arrow_start_y = to_y-delta_y_to + dy;

			arrow_top_x = to_x - delta_x_to;
			arrow_top_y = to_y - delta_y_to;

			dx = EPS_EDGES_ARROW_LENGTH * cos (alpha + M_PI - EPS_EDGES_ARROW_ANGLE);
			dy = EPS_EDGES_ARROW_LENGTH * sin (alpha + M_PI - EPS_EDGES_ARROW_ANGLE);

			arrow_end_x = to_x - delta_x_to + dx;
			arrow_end_y = to_y - delta_y_to + dy;

			fprintf(fp, "%d %d lineto stroke %% -> (%s)\n", (int) (to_x
					- delta_x_to), (int) (to_y - delta_y_to), to_name);

			fprintf(fp, "newpath %d %d %d %d %d %d Arrow \n", (int)arrow_start_x,
					(int)arrow_start_y, (int)arrow_top_x, (int)arrow_top_y,
					(int)arrow_end_x, (int)arrow_end_y);
		}
	}
}


/****************************************************************************/
/* NAME: eps_edges                                                          */
/* FUNKTION: Schreibt die Zeichenoperationen fuer Kanten in ein eps-file    */
/* UEBERGABEPARAMETER: Graph * (der graph), char * (Name des Files)         */
/* RUECKGABEWERT: KEINER                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 25.09.95                                                             */
/* LETZTE AENDERUNG AM: 25.09.95                                            */
/****************************************************************************/
static void _eps_edges (XGraph * graph, FILE * fp)
{
	XGraphEdgeIterator * iter = xgraph_edge_iterator(graph);
	for ( ; xgraph_edge_iterator_is_valid(iter) ; xgraph_edge_iterator_next(iter)) {
		XGraphEdge * cur = xgraph_edge_iterator_get(iter);

		// first_part is a technical detail in the PS file. The first part of
		// any edge(-path) has to be drawn with first_part set to TRUE.
		gboolean first_part = TRUE;
		XGraphEdge * back_edge = xgraph_edge_get_inverse(cur);
		gboolean two_way = (back_edge != NULL);
		XGraphEdgeLayout * layout = xgraph_edge_get_layout(cur);
		gboolean is_circle = (xgraph_edge_get_from_node(cur) == xgraph_edge_get_to_node(cur));
		XGraphNode * from_node = xgraph_edge_get_from_node(cur),
				*to_node = xgraph_edge_get_to_node(cur);
		LayoutPoint from_pt = xgraph_node_layout_get_pos(xgraph_node_get_layout(from_node)),
				to_pt = xgraph_node_layout_get_pos(xgraph_node_get_layout(to_node));

		gboolean toIsHelper = xgraph_node_is_helper(to_node);
  		gboolean fromIsHelper = xgraph_node_is_helper(from_node);

		/* Simple edge:
		 *     	FromNode --- ToNode
		 * Non-simple edge:
		 * 		FromNode --- pts[0] --- ... --- pts[n] --- ToNode.
		 * Hence, there are n+1 line segments for this kind of edges.
		 */

		if (is_circle) {
			_eps_single_edge (cur, layout, &from_pt,	&to_pt, TRUE,
					FALSE/*Start exactly at from?*/,
					FALSE/*No end arrow?*/,
					TRUE/*Start new edge*/, fp);
		}
		else if (xgraph_edge_layout_is_simple(layout)) {
			_eps_single_edge (cur, layout, &from_pt,	&to_pt, FALSE,
					FALSE/*Start exactly at from?*/,
					FALSE/*No end arrow?*/,
					TRUE/*Start new edge*/, fp);
		}
		else /* at least two line segments. */ {
			XGraphEdgePath * path = (XGraphEdgePath*)xgraph_edge_layout_get_path(layout);
			GQueue/*<LayoutPoint*>*/ * pts = xgraph_edge_path_get_points(path);
			GList/*<LayoutPoint*>*/ * ptiter = pts->head;
			gint j;

			gint segCount = pts->length+1;
			struct line_segment_t {
				LayoutPoint from, to;
			} * segs = g_new0(struct line_segment_t, segCount), *segIter = segs,
			*lastSeg = segs + (segCount-1);

			segs[0].from = from_pt;

			for (j = 0 ; j < segCount-1 ; ++j, ptiter = ptiter->next) {
				segs[j].to = *(LayoutPoint*)ptiter->data;

				segs[j+1].from = *(LayoutPoint*)ptiter->data;
			}
			assert (segCount-1 == j);
			segs[segCount-1].to = to_pt;

			if (two_way) {
				/* If the edge is bidirectional then each direction is represented
				 * by a separate edge. To avoid drawing a segment multiple times we
				 * have to determine which direction draw which part of the line. */
				if (from_node < to_node) {

					/* From Node <--- pts[0] (--- ... not covered here) */
					_eps_single_edge (cur, layout, (LayoutPoint*)pts->tail->data, &to_pt,
							FALSE/*circle?*/,
							TRUE/*Start exactly at from?*/,
							FALSE/*No end arrow?*/,
							TRUE, fp);
					g_free (segs);
					continue;
				}
				else {
					/* (...) pts[0] --- ... --- pts[n] --- ToNode */
				}
			}
			else {
				_eps_single_edge (cur, layout, &from_pt, (LayoutPoint*)pts->head->data,
											FALSE/*circle?*/,
											FALSE/*Start exactly at from?*/,
											TRUE/*No end arrow?*/, TRUE, fp);
				first_part = FALSE;
			}

			++segIter; // Skip the first part.

			/* Draw all edges except the last one. The last one is special
			 * because it has an arrow. */
			for ( ; segIter != lastSeg ; ++segIter) {
				_eps_single_edge (cur, layout, &segIter->from, &segIter->to, is_circle,
						TRUE, TRUE, first_part, fp);
				first_part = FALSE;
			}

			/* Draw the last segment. */
			_eps_single_edge (cur, layout, &lastSeg->from, &lastSeg->to,
										FALSE/*circle?*/,
										TRUE/*Start exactly at from?*/,
										FALSE/*No end arrow?*/,
										first_part/*start new PostScript line?*/, fp);
			g_free (segs);
		}
	}
	xgraph_edge_iterator_destroy(iter);
}


/****************************************************************************/
/* NAME: graph_eps_file                                                     */
/* FUNKTION: schreibt die Zeichenoperationen fuer den graph in ein eps-file */
/* UEBERGABEPARAMETER: Graph * (der graph), char * (Name des Files)         */
/* RUECKGABEWERT: keiner                                                    */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 04.07.95                                                             */
/* LETZTE AENDERUNG AM: 04.07.95 (Zusammenfassung nodes_ u. edegs_eps_file) */
/*                      07-MAR-2008 STB: Labeling enhancements              */
/****************************************************************************/
static gboolean _graph_eps_file (XGraph * gr, const gchar * name)
{

	FILE * fp;
	GdkRectangle rc = xgraph_get_display_rect(gr);
	// Offset is used to fully draw the selfedges
	max_y = rc.y + rc.height + (2*offset);
	width = rc.width + (2*offset);
	height = rc.height + (2*offset);

	/* If a node is drawn at the bottom then it has to stay inside the visible
	 * area. */
	max_y += EPS_NODES_RADIUS;

	fp = fopen (name, "w");
	if (fp) {
		Relview * rv = rv_get_instance ();
		RelManager * manager = rv_get_rel_manager (rv);
		Rel * rel = rel_manager_get_by_name (manager, xgraph_get_name(gr));
		Label * label = NULL;

		if (rel) {
			RelationViewport * viewport	= relation_window_get_viewport(relation_window_get_instance());
			LabelAssoc assoc = rv_label_assoc_get (rv, rel, viewport);
			if (assoc.labels[0] == assoc.labels[1])
				label = assoc.labels[0];
		}

		fprintf(fp, "%s\n", graph_epfs_header_1);
		fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n%%%%EndComments\n", width, height);
		fprintf(fp, "%s\n", graph_epfs_header_2);
		fprintf(fp, "/Radius{ %d } def\n", EPS_NODES_RADIUS);
		fprintf(fp, "%s\n", graph_epfs_header_3);

		_eps_nodes (gr, label, fp);
		_eps_edges (gr, fp);

		fprintf (fp, "showpage\n");
		fclose (fp);
	}

	return TRUE;
}


/*!
 * Implements IOHandler::saveSingleRel. See \ref eps_get_handler.
 */
static gboolean _eps_handler_save_rel (EpsHandler * self,
		const gchar * filename,	Rel * rel, GError ** perr)
{
	KureRel * impl = rel_get_impl (rel);

	if ( ! kure_rel_prod_fits_si(impl)) {
		g_set_error (perr, rv_error_domain(), 0, "Unable write EPS file. "
				"Relation is too big.");
		return FALSE;
	}
	else {
		gboolean ret;
		Relview * rv = rv_get_instance ();
		RelationViewport * viewport	= relation_window_get_viewport(relation_window_get_instance());
		LabelAssoc assoc = rv_label_assoc_get (rv, rel, viewport);

		ret = _write_eps_to_file(filename, impl, assoc.labels[0], assoc.labels[1],
				0.6 /*gray*/, -M_PI/4.0 /*angle*/, "Sans-Serif", 1.0);
		return ret;
	}
}

/*!
 * Implements IOHandler::saveSingleGraph. See \ref eps_get_handler.
 */
static gboolean _eps_handler_save_graph (EpsHandler * self,
		const gchar * filename,	XGraph * graph, GError ** perr)
{
	return _graph_eps_file (graph, filename);
}

static void _eps_handler_destroy (IOHandler * self)
{
	g_free (self);
}

IOHandler * eps_get_handler ()
{
	IOHandler * ret = g_new0 (IOHandler,1);
	static IOHandlerClass * c = NULL;
	if (!c) {
		c = g_new0 (IOHandlerClass,1);

		c->saveSingleRel = _eps_handler_save_rel;
		c->saveSingleGraph = _eps_handler_save_graph;
		c->destroy = _eps_handler_destroy;

		c->name = g_strdup ("Encapsulated Postscript");
		c->extensions = g_strdup ("eps");
		c->description = g_strdup ("Can write relations and "
				"graphs to EPS files.");
	}

	ret->c = c;
	return ret;
}


/*

	TEST SECTION

*/

char *remove_ext (char* mystr, char dot, char sep) {
    char *retstr, *lastdot, *lastsep;

    // Error checks and allocate string.
    if (mystr == NULL)
        return NULL;
    if ((retstr = malloc (strlen (mystr) + 1)) == NULL)
        return NULL;

    // Make a copy and find the relevant characters.
    strcpy (retstr, mystr);
    lastdot = strrchr (retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

    // If it has an extension separator.

    if (lastdot != NULL) {
        // and it's before the extenstion separator.
        if (lastsep != NULL) {
            if (lastsep < lastdot) {
                // then remove it.
                *lastdot = '\0';
            }
        } else {
            // Has extension separator with no path separator.
            *lastdot = '\0';
        }
    }
    // Return the modified string.
    return retstr;
}

static gboolean _pdf_convert (const gchar * filename, const gchar * filenameEPS) {
	// Convert to PDF
    int parent = getpid();
    int pid = fork();

    if (pid == -1) {
        return FALSE;
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
       // Convert EPS to PDF
       //execl("/usr/bin/ps2pdf", "ps2pdf", filenameEPS, filename, NULL);
       execl("/usr/bin/epstopdf", "epstopdf", filenameEPS, NULL); // Requires epstools
    }
    // Remove the temp EPS-File
    remove(filenameEPS);
    return TRUE;
}


static gboolean _pdf_handler_save_graph (EpsHandler * self,
		const gchar * filename,	XGraph * graph, GError ** perr) {
	gchar * filenameEPS =  remove_ext (filename, '.', '/');

	// Build EPS File and check if the build was successfull
	if (_graph_eps_file (graph, filenameEPS))
		return  _pdf_convert(filename,filenameEPS);

	return FALSE;
}

static gboolean _pdf_handler_save_rel (EpsHandler * self,
		const gchar * filename,	XGraph * graph, GError ** perr) {
	gchar * filenameEPS =  remove_ext (filename, '.', '/'); //g_strconcat(remove_ext (filename, '.', '/'), ".eps");

	// Build EPS File and check if the build was successfull
	if (_eps_handler_save_rel(self,filenameEPS,graph,perr)) {
		return  _pdf_convert(filename,filenameEPS);
	}

	return FALSE;
}


IOHandler * pdf_get_handler ()
{
	IOHandler * ret = g_new0 (IOHandler,1);
	static IOHandlerClass * c = NULL;
	if (!c) {
		c = g_new0 (IOHandlerClass,1);

		c->saveSingleRel = _pdf_handler_save_rel;
		c->saveSingleGraph = _pdf_handler_save_graph;
		c->destroy = _eps_handler_destroy;

		c->name = g_strdup ("Portable Document Format");
		c->extensions = g_strdup ("pdf");
		c->description = g_strdup ("Can write relations and "
				"graphs to PDF files.");
	}

	ret->c = c;
	return ret;
}
