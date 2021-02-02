/*
 * graphwindow_gtk.c
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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sys/param.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <string.h>

#include "Relview.h"
#include "global.h"
#include "Relation.h"
#include "Graph.h"
#include "utilities.h"
#include "label.h"
#include "msg_boxes.h"
#include "RelationWindow.h"
#include "input_string_gtk.h"
#include "mark_edgesnodes_gtk.h"
#include "DirWindow.h"
#include "prefs.h"
#include "input_string_gtk.h"

#include "SelectionManager.h"

#include "Vec2d.h"
#include "Edge.h"
#include "Node.h"


static void _graph_window_block_observer (GraphWindow * self)
{ self->block_observer = TRUE; }
static void _graph_window_unblock_observer (GraphWindow * self)
{ self->block_observer = FALSE; }


#define FOREACH_G_LIST(list,type,var,iter,code) { \
	GList * (iter) = (list); \
	for ( ; (iter) ; (iter) = (iter)->next) { \
		type (cur) = (type) (iter)->data; \
		code; \
	} \
}

#if 0
/* Werte fuer die Graph-Algorithmen */
/* min_rank, min_order fuer (TREE, DAG) */
/* spanning_.. fuer (SPRINGEMBEDDER)      */

extern int min_rank;
extern int min_order;
extern int spanning_width;
extern int spanning_height;
#endif

/* external functions */

extern int name_type(char *); /* from *.lex */

GtkWidget * create_popupmenuNodeSelection();
GtkWidget * create_popupmenuEdgeSelection();

extern GtkWidget * popupmenuNodeSelection;
extern GtkWidget * popupmenuEdgeSelection;


/* the relation must exist. */
void rel_replace_with_temp (const gchar * name, Rel * tmpRel)
{
	RelManager * manager = rv_get_rel_manager(rv_get_instance());
	Rel * oldRel = rel_manager_get_by_name(manager, name);

	g_assert (oldRel != NULL && tmpRel);

	/* delete the existing relation, with the desired name,
	* if there is one. (See above.) */
	rel_manager_delete_by_name (manager, name);
	rel_rename (tmpRel, name);
}

#include "Relview.h" /* rv_ask_rel_name() */


/* call redraw_rel (NULL); after use and set the global rel variable to the
 * result if wanted. */
Rel * new_or_replace_relation(mpz_t rows, mpz_t cols,
                              gchar * captionStr, gchar * descrStr,
                              gchar /*in (only!)*/* nameStr, gchar * existsStr)
{
	GString *caption = g_string_new(captionStr), *descr =
			g_string_new(descrStr), *name = g_string_new(nameStr);
	gboolean canceled = FALSE;
	gboolean ret = rv_ask_rel_name(caption, descr, name, TRUE);
	RelManager * manager = rv_get_rel_manager(rv_get_instance());
	Rel * localRel = NULL;

	if (ret)
	{
		while (0 != strcmp("$", name->str) && rel_manager_exists(manager, name->str))
		{
			/* relation already exists. Overwrite or try again? */

			int respId;
			GtkWidget * dialog = gtk_message_dialog_new(
					GTK_WINDOW (graph_window_get_widget(graph_window_get_instance())), 0 /*flags*/,
					GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					"A relation with name \"%s\" already exists.", name->str);
			gtk_dialog_add_buttons(GTK_DIALOG (dialog), "Try again", 1,
					"Overwrite", 2, NULL);
			gtk_message_dialog_format_secondary_text(
					GTK_MESSAGE_DIALOG (dialog),
					"Press \"Try again\" to enter another relation name, "
						"\"Overwrite\" to overwrite the current relation with name "
						"\"%s\" or press \"Cancel\" to do nothing.", name->str);

			gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
			respId = gtk_dialog_run(GTK_DIALOG (dialog));
			gtk_widget_destroy(dialog);

			if (respId == 1)
			{
				ret = rv_ask_rel_name(caption, descr, name, TRUE);

				if (!ret)
				{
					canceled = TRUE;
					break;
				}
				continue;
			}
			else if (respId == GTK_RESPONSE_CANCEL)
			{
				canceled = TRUE;
				break;
			}
			else if (respId == 2) /* overwrite */
			{
				break;
			}
			else
			{
				assert (respId == 1 || respId == GTK_RESPONSE_CANCEL
						|| respId == 2);
			}
		}
	}
	else { canceled = TRUE; }

	if (!canceled)
	{
		/* Overwrite a current relation with the same name if necessary. */
		rel_manager_delete_by_name (manager, name->str);
		localRel = rel_manager_create_rel(manager, name->str, rows, cols);
	}

	g_string_free(name, TRUE);
	g_string_free(descr, TRUE);
	g_string_free(caption, TRUE);

	return canceled ? NULL : localRel;
}


void menuitemCreateVectorFromNodeSelection_activate(GtkMenuItem * menuitem,
		gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);
	mpz_t rows, cols;
	Rel * vec = NULL;
    RelationWindow * rw = relation_window_get_instance();

	mpz_init_set_si (cols, 1);
	mpz_init_set_si (rows, xgraph_get_node_count(gr));

	vec = new_or_replace_relation(rows, cols, "Enter vector name",
			"Vector name:", "", /* default name */
			"Relation already exists.");

	mpz_clear (cols);
	mpz_clear (rows);

	if (NULL == vec)
		return;

	/* fill the newly created vector */
	{
		SelectionManager * selManager = graph_window_get_selection_manager(gw);
		const GList * l = selManager->getSelection(selManager, NULL);
		const GList * iter = l;
		KureRel * impl = rel_get_impl (vec);

		for (; iter; iter = iter->next) {
			/* Maybe the node numbers are shown in the graph window and are not the
			 * internal node numbers. */
			XGraphNode * node = (XGraphNode*) iter->data;
			gint nodeNum = atoi(xgraph_node_get_name(node));

			kure_set_bit_si(impl, TRUE, nodeNum-1, 0);
		}
	}

	rel_changed(vec);
	relation_window_set_relation(rw, vec);
}

/*! */
void menuitemCreateRelationFromEdgeSelection_activate(GtkMenuItem * menuitem,
		gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);
	mpz_t n;
	Rel * localRel = NULL;
	RelationWindow * rw = relation_window_get_instance();

	mpz_init_set_si(n, xgraph_get_node_count(gr));

	localRel = new_or_replace_relation(n, n, "Enter relation name",
			"Relation name:", "", /* default name */
			"Relation already exists.");

	mpz_clear(n);

	if (NULL == localRel)
		return;

	/* fill the newly created relation */
	{
		SelectionManager * selManager = graph_window_get_selection_manager(gw);
		const GList * l = selManager->getSelection(selManager, NULL);
		const GList * iter = l;
		KureRel * impl = rel_get_impl(localRel);

		for (; iter; iter = iter->next)
		{
			/* Use the node numbers shown in the graph window instead of their ids. */
			XGraphEdge * edge = (XGraphEdge*) iter->data;
			XGraphNode *from = xgraph_get_node_by_id(gr,
					xgraph_edge_get_from_id(edge)), *to =
					xgraph_get_node_by_id(gr, xgraph_edge_get_to_id(edge));
			int row = atoi(xgraph_node_get_name(from)) - 1, col = atoi(
					xgraph_node_get_name(to)) - 1;

			kure_set_bit_si(impl, TRUE, row, col);
		}
	}

	rel_changed (localRel);
	relation_window_set_relation(rw, localRel);
}


/****************************************************************************/
/*       NAME : graph_window_init                                           */
/*    PURPOSE : initializes all module global variables                     */
/*    CREATED : 03-MAR-1995 PS                                              */
/*   MODIFIED : 09-MAY-2000 WL: GTK+ port                                   */
/****************************************************************************/

/****************************************************************************/
/*       NAME : graph_window_destroy                                        */
/*    PURPOSE : frees all alocated resources                                */
/*    CREATED : 13-JUN-2000 WL                                              */
/****************************************************************************/
void graph_window_destroy(GraphWindow * gw)
{
  if (!gw)
    return;

  if (gw->selManager)
    selection_manager_destroy(gw->selManager);

  /* destroy popup menu */
  if (gw->popupmenu) {
    gtk_object_destroy(GTK_OBJECT (gw->popupmenu));
  }

  /* destroy graph window */
  if (gw->window) {
    gtk_object_destroy(GTK_OBJECT (gw->window));
  }

  /* destroy backpixmap */
  if (gw->backpixmap) {
    gdk_pixmap_unref (gw->backpixmap);
    cairo_destroy(gw->crOffset);
  }

  free(gw);
}

/****************************************************************************/
/*       NAME : set_graph_delta_grid                                        */
/*    PURPOSE : sets the grid line distance                                 */
/* PARAMETERS : delta (the distance)                                        */
/*    CREATED : 25-NOV-1996 PS                                              */
/****************************************************************************/
/* void set_graph_delta_grid (int delta) */
/* { */
/*   delta_grid = delta; */
/* } */

/****************************************************************************/
/*       NAME : get_graph_delta_grid                                        */
/*    PURPOSE : returns the grid line distance                              */
/*    CREATED : 25-NOV-1996 PS                                              */
/****************************************************************************/
/* int get_graph_delta_grid () */
/* { */
/*   return (delta_grid); */
/* } */

/****************************************************************************/
/*       NAME : set_graph_grid                                              */
/*    PURPOSE : turns the grid on or off                                    */
/* PARAMETERS : TRUE/FALSE => on/off                                        */
/*    CREATED : 25-NOV-1996 PS                                              */
/****************************************************************************/
/* void set_graph_grid (int state) */
/* { */
/*   grid_on = state; */
/* } */

/****************************************************************************/
/*       NAME : get_graph_grid                                              */
/*    PURPOSE : returns the grid state                                      */
/*    CREATED : 25-NOV-1996 PS                                              */
/****************************************************************************/
/* int get_graph_grid () */
/* { */
/*   return (grid_on); */
/* } */


void menuitemUpdateRelGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));

	if ( !graph_window_has_graph (gw)) return;
	else if ( !graph_window_is_editable (gw)) {
		rv_user_error ("Not allowed", "Graph cannot be changed.");
		return;
	}
	else {
		RelManager * rm = rv_get_rel_manager(rv_get_instance());
		XGraph * gr = graph_window_get_graph(gw);
		const gchar * name = xgraph_get_name(gr);
		Rel * old_rel = rel_manager_get_by_name(rm, name), *rel = NULL;

		/* If a relation with the given name already exists, ask the user
		 * whether we shall overwrite the relation. */
		if (old_rel) {
			if (error_and_wait(error_msg[overwrite_rel]) != NOTICE_YES)
				return;
		}

		rel = rel_new_from_xgraph (rel_manager_get_context(rm), gr);
		if ( !rel) {
			rv_user_error("Unable to update relation", error_msg[create_rel]);
			return;
		}
		else {
			if (old_rel) {
				rel_assign (old_rel, rel);
				rel_destroy (rel);
				rel = old_rel;

				rel_changed (rel);
			}
			else {
				rel_manager_insert (rm, rel);
			}

			relation_window_set_relation(relation_window_get_instance(), rel);
		}
	}
}


static gchar * _default_filename (gpointer obj)
{
	XGraph * gr = (XGraph*)obj;
	const gchar * name = xgraph_get_name(gr);
	if (g_str_equal (name, "$")) return g_strdup ("dollar");
	else return g_strdup (name);
}

static gboolean _export (IOHandler * handler, const gchar * filename, gpointer obj, GError ** perr)
{
	return io_handler_save_single_graph (handler, filename, (XGraph*)obj, perr);
}

void menuitemExportGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);
	if (! gr) return;
	else {
		rv_user_export_object (IO_HANDLER_ACTION_SAVE_SINGLE_GRAPH, "Export Graph",
				gr, _default_filename, _export);
	}
}

GList/*<GtkFileFilter*>*/ * _file_filters_from_io_handlers (GList/*<IOHandler*>*/ * handlers)
{
	GList * ret = NULL;
	GList * iter = handlers;

	{
		GtkFileFilter * filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, "All files (*)");
		gtk_file_filter_add_pattern (filter, "*");
		ret = g_list_prepend (ret, filter);
	}

	for ( ; iter ; iter = iter->next) {
		IOHandler * cur = IO_HANDLER(iter->data);
		GtkFileFilter * filter = gtk_file_filter_new ();
		GString * filter_name = g_string_new ("");
		GList/*<gchar*>*/ * exts = io_handler_get_extensions_as_list (cur), *jter;

		g_string_append_printf(filter_name, "%s (", io_handler_get_name(cur));

		for (jter = exts ; jter ; jter = jter->next) {
			gchar * cur_ext = (gchar*) jter->data;
			gchar * pat = g_strdup_printf ("*.%s", cur_ext);

			g_string_append (filter_name, pat);
			gtk_file_filter_add_pattern (filter, pat);

			g_free (cur_ext);
			g_free (pat);

			if (jter->next) g_string_append (filter_name, ", ");
		}
		g_list_free (exts);
		gtk_file_filter_set_name (filter, filter_name->str);
		g_string_free (filter_name, TRUE);

		ret = g_list_prepend (ret, filter);
	}

	return g_list_reverse (ret);
}

static gboolean _import (IOHandler * handler, const gchar * filename, GError ** perr)
{
	return io_handler_load_graphs (handler, filename, NULL, NULL, perr);
}

void menuitemImportGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	rv_user_import_object (IO_HANDLER_ACTION_LOAD_GRAPHS, "Import Graph", _import);
}

/*****************************************************************************/
/*       NAME: bipart                                                        */
/*    PURPOSE: create bipartite graph (previous graph is deleted)            */
/*    CREATED: 29-AUG-1995 PS                                                */
/*   MODIFIED: 16-JUN-1998 NN: Changed if-condition whether graph existed    */
/*                             from rel->name to graph->name. Brought up a   */
/*                             seg fault at selected graphs.                 */
/*             18-MAY-2000 WL: GTK+ port                                     */
/*****************************************************************************/
void bipart(GraphWindow * gw)
{
#if 1
	g_warning("bipart: NOT IMPLEMENTED!\n");
#else
  Graph * gr = graph_window_get_graph(gw);
  graphlistptr gl;
  rellistptr tmp_rl;
  Rel * rel;

  if (gr) {
    if (graph_exist(graph_root, gr->name)) {
      if (error_and_wait(error_msg [overwrite_graph]) == NOTICE_NO)
        retur;
      else {
        tmp_rl = graph_to_rel(gr);
        if (tmp_rl) {
          graph_root = del_graph(graph_root, gr->name);
          rel = & (tmp_rl->rel);
          gl = graph_create_bipart(rel, 50);
          graph_root = append_graph(graph_root, gl);
          gr = & (gl->graph);
          graph_window_set_graph(gw, gr);
          graph_window_redraw(gw, NEW);
        } else
          printf("GRAPH_TO_REL failed\n");
      }
    } else
      printf("Graph does not exist\n");
  } else
    printf("Graph is NULL\n");
#endif
}

#if 1
gchar * _state(GdkEventButton * e)
{
  return g_strconcat((e->state & GDK_SHIFT_MASK) ? "GDK_SHIFT_MASK " : "",
                      (e->state & GDK_LOCK_MASK) ? "GDK_LOCK_MASK " : "",
                      (e->state & GDK_CONTROL_MASK) ? "GDK_CONTROL_MASK " : "",
                      (e->state & GDK_MOD1_MASK) ? "GDK_MOD1_MASK " : "",
                      (e->state & GDK_MOD2_MASK) ? "GDK_MOD2_MASK " : "",
                      (e->state & GDK_MOD3_MASK) ? "GDK_MOD3_MASK " : "",
                      (e->state & GDK_MOD4_MASK) ? "GDK_MOD4_MASK " : "",
                      (e->state & GDK_MOD5_MASK) ? "GDK_MOD5_MASK " : "",
                      (e->state & GDK_BUTTON1_MASK) ? "GDK_BUTTON1_MASK " : "",
                      (e->state & GDK_BUTTON2_MASK) ? "GDK_BUTTON2_MASK " : "",
                      (e->state & GDK_BUTTON3_MASK) ? "GDK_BUTTON3_MASK " : "",
                      (e->state & GDK_BUTTON4_MASK) ? "GDK_BUTTON4_MASK " : "",
                      (e->state & GDK_BUTTON5_MASK) ? "GDK_BUTTON5_MASK " : "",
                      (e->state & GDK_RELEASE_MASK) ? "GDK_RELEASE_MASK " : "",
                      (e->state & GDK_MODIFIER_MASK) ? "GDK_MODIFIER_MASK "
                                                     : "",
                     NULL);
}

void _print_event_button_info(GdkEventButton * e)
{
  gchar * state = _state(e);

  fprintf(stderr, "%s\n", state);

  fprintf(stderr, "_mouse button event:___\n"
  "\ttype: %s\n"
  "\twindow: %p\n"
  "\tsend_event: %s\n"
  "\ttime: %u [msecs]\n"
  "\t(x,y): (%.3f, %.3f)\n"
  //"\taxes (x,y): (%.3f, %.3f)\n"
  "\tstate: %s\n"
  "\tbutton: %d\n"
  "\tdevice: %p\n"
  "\troot: (%.3f, %.3f)\n",
  (e->type == GDK_BUTTON_PRESS) ? "GDK_BUTTON_PRESS" :
  (e->type == GDK_2BUTTON_PRESS) ? "GDK_2BUTTON_PRESS" :
  (e->type == GDK_3BUTTON_PRESS) ? "GDK_3BUTTON_PRESS" :
  (e->type == GDK_BUTTON_RELEASE) ? "GDK_BUTTON_RELEASE" : "(unknown)",
  e->window,
  e->send_event ? "TRUE" : "FALSE",
  e->time,
  e->x, e->y,
  //e->axes[0], e->axes[1], /* causes a segfault! */
  state,
  e->button,
  e->device,
  e->x_root, e->y_root);

  g_free(state);
}
#endif

gboolean drawingarea_LeftAndCtrlButtonPress(GtkWidget * widget,
                                            GdkEventButton * event,
                                            gpointer user_data)
{
  return FALSE;
}

gboolean drawingarea_LeftButtonPress(GtkWidget * widget,
                                     GdkEventButton * event, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(widget);
  XGraph * gr = graph_window_get_graph(gw);

  if (graph_window_get_zoom(gw) != 1.0) {
    graph_window_reset_zoom(gw);
    graph_window_redraw(gw, OLD);
    return FALSE;
  }

  /* If the graph is not valid stop here. */
  if ( !gr)
    return FALSE;

  if (event->state & GDK_CONTROL_MASK) {
    return drawingarea_LeftAndCtrlButtonPress(widget, event, user_data);
  }

  if (! xgraph_is_correspondence (gr)) {

    GdkPoint pt = graph_window_mouse_to_viewport(gw, event->x, event->y);
    XGraphNode * node = graph_window_node_from_position(gw, pt.x, pt.y);

    /* NOTE: the user will never be over a (selected )help node, so
     *       graph_window_node_from_position will never return a (selected)
     *       help node. */

    if (graph_window_has_selected_node(gw)) {
      XGraphNode * fromNode = graph_window_get_selected_node(gw);

      _graph_window_block_observer(gw);
      if (node) {
    	  /* create an edge from the previous selected node to the current one */
    	  if (! xgraph_has_edge(gr, fromNode, node)) {
    		  xgraph_create_edge_s(gr, fromNode, node);
    	  }
      } else {
		  /* move the previous selected node to the current position */
		  graph_window_move_node_to(gw, fromNode, pt.x, pt.y);
	  }

      graph_window_unselect_node(gw);
      _graph_window_unblock_observer(gw);

      xgraph_layout_changed(gr);
    }
    else /* no current selection */{
      if (node) {
        /* select to current node */
        graph_window_set_selected_node(gw, node);
      } else {
        /* create a new node at the current position */
        graph_window_create_node(gw, pt.x, pt.y);
      }
    }
  }
  else { ; }

  //graph_window_redraw(gw, OLD);

  return FALSE;
}

gboolean drawingarea_MiddleButtonPress(GtkWidget * widget,
                                       GdkEventButton * event,
                                       gpointer user_data)
{
  GraphWindow * gw = graph_window_get(widget);
  XGraph * gr = graph_window_get_graph(gw);

  if (graph_window_get_zoom(gw) != 1.0)
    return FALSE;

  /* If the graph is not valid stop here. */
  if ( !gr)
    return FALSE;

  if (! xgraph_is_correspondence (gr)) {
    SelectionManager * selManager = graph_window_get_selection_manager(gw);
    GdkPoint pt = graph_window_mouse_to_viewport(gw, event->x, event->y);
    XGraphNode * node = graph_window_node_from_position(gw, pt.x, pt.y);
    XGraphEdge * edge = graph_window_edge_from_position(gw, pt.x, pt.y);

    /* NOTE: graph_window_node_from_position will never return a (selected)
     *       help node. */

    /* The node/edge will be unreferenced by the GraphObserver. */
#if 1
    if (node) {
    	xgraph_delete_node (gr, node);
    }
    else if (edge) {
    	xgraph_delete_edge (gr, edge);
    }
    else {
    	/* Nothing happened. */
    }
#else
    if (node) /* delete node */ {
      int nodeState = node->state;

      del_node(gr, node->number);
      del_help_nodes(gr);

      /* renumber the node's names (node.name) [NOT their internal
       * IDs (node.number)] */
      renumber_graph(gr);

      if ((get_last_node(gr) == 0)
      /* special case: last node deleted */
      || (nodeState == SELECTED_HELP)) {
        /* or selected help node */
        rel_graph_modified = FALSE;
        init_rel_dir();
      } else {
        if ( !rel_graph_modified)
          init_rel_dir();
        rel_graph_modified = TRUE;
        gr->state = MODIFIED;
      }
    } else if (edge) /* delete edge */{
      del_edge(gr, edge->from, edge->to);
      del_help_nodes(gr); /* delete all help nodes that are not
       * attached to an edge */
      if ( !rel_graph_modified)
        init_rel_dir();
      rel_graph_modified = TRUE;
      gr->state = MODIFIED;
    }

    graph_window_redraw(gw, OLD);
#endif
  } /* CORRESPONDENCE? */

  return FALSE;
}



/*!
 * Called, if a graph was selected from the menu of all graphs.
 */
void menuitemSetGraphGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * self = graph_window_get(GTK_WIDGET(menuitem));
	const gchar * name = gtk_menu_item_get_label (menuitem);
	if (name) {
		graph_window_set_graph(self, xgraph_manager_get_by_name(
				xgraph_manager_get_instance(), name));
	}
	else g_warning ("menuitemSetGraphGW_activate: No name given.\n");
}


/*!
 * Called, if the used selected a recent graph name from the popup menu. Sets
 * the current graph in the window.
 */
void menuitemMostRecentGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * self = graph_window_get(GTK_WIDGET(menuitem));
	const gchar * name = gtk_menu_item_get_label (menuitem);
	if (name) {
		graph_window_set_graph(self, xgraph_manager_get_by_name(
				xgraph_manager_get_instance(), name));
	}
	else g_warning ("menuitemMostRecentGW_activate: No name given.\n");
}



gboolean drawingarea_RightButtonPress(GtkWidget * widget,
                                      GdkEventButton * event, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(widget);

  // Get GraphNode Limit and Flag if menu should be disabled
  int graphNodeLimit = prefs_get_int("settings", "graphNode_display_limit", 2 << 26);
  int windowDisabled = prefs_get_int("settings", "disable_windows_on_limit", 2 << 26);

  // GraphNodeLimit reached, do not display the Menu
  if((xgraph_get_node_count(graph_window_get_graph(gw)) > graphNodeLimit) && windowDisabled)
  	return;

  /* If there is currently no graph in the window, then we show a s0e menu
   * which just offers the opportunity to create a new graph. */
  if (! graph_window_has_graph(gw)) {
	  if (! (event->state & GDK_CONTROL_MASK)) /* no ctrl pressed */ {
		  if (!gw->popupmenuEmpty) {
				gw->popupmenuEmpty
						= GTK_WIDGET(gtk_builder_get_object(rv_get_gtk_builder(
							rv_get_instance()), "popupMenuGraphWindowEmpty"));
				g_assert (gw->popupmenuEmpty != NULL);

				gtk_menu_attach_to_widget(GTK_MENU(gw->popupmenuEmpty), widget,
						NULL);
			}

		  /* Show the popup. */
		  gtk_menu_popup(GTK_MENU(gw->popupmenuEmpty), NULL, NULL, NULL, NULL,
					event->button, event->time);
	  } /* ctrl pressed? */

	  return FALSE;
  }
  else /* window not empty */ {
#if 1
  if (! (event->state & GDK_CONTROL_MASK)) /* ctrl not pressed */ {
	  gboolean initial = FALSE;
	  GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());
	  XGraph * gr = graph_window_get_graph(gw);

	  if (! gw->popupmenu) {
		  initial = TRUE;
		  gw->popupmenu
			  = GTK_WIDGET(gtk_builder_get_object (builder, "popupMenuGraphWindow"));
		  g_assert (gw->popupmenu != NULL);
	  }

	  if (initial) {
		  gtk_menu_attach_to_widget(GTK_MENU(gw->popupmenu), widget, NULL);
	  } /* initial? */

	  /* Update the recent graphs listing. */
	  {
		  gint n_show = prefs_get_int("settings", "graph_window_show_n_most_recent", 3);
		  GList/*<const gchar*>*/ * recent = _graph_window_get_most_recent (gw,n_show);
		  gint n_avail = g_list_length(recent);

		  /* Show some recent graph names in the menu. Menu items have to be
		   * added and removed if necessary. */
		  {
			  GtkWidget * begin = GTK_WIDGET(gtk_builder_get_object (rv_get_gtk_builder(rv_get_instance()),
					  "menuitemMostRecentGW"));
			  GtkWidget * end = GTK_WIDGET(gtk_builder_get_object (rv_get_gtk_builder(rv_get_instance()),
					  "menuitemMoreGW"));
			  GList/*<GtkWidget*>*/ * items = gtk_container_get_children(GTK_CONTAINER(gw->popupmenu));
			  gint begin_pos = g_list_index (items, begin);
			  gint end_pos = g_list_index (items, end);
			  gint delta = n_avail - (end_pos - begin_pos);
			  gint i;
			  GList * itemIter = NULL;

			  /* There a currently end_pos - begin_pos menu items for the most
			   * recent graph names. If delta is positive, we have to add delta
			   * items and we have to remove delta items, if delta is
			   * negative. */
			  if (delta > 0) {
				  for ( i = 0 ; i < delta ; ++ i) {
					  GtkWidget * mi = gtk_menu_item_new_with_label ("");
					  gtk_menu_shell_insert (GTK_MENU_SHELL (gw->popupmenu), mi, end_pos + i);
					  gtk_widget_show (mi);

					  /* All items for the most recent graphs share the same
					   * signal handler for activate. The individual graph name
					   * is available through the item's label. */
					  g_signal_connect ((gpointer) mi, "activate",
					                    G_CALLBACK (menuitemMostRecentGW_activate), NULL);
				  }
			  }
			  else if (delta < 0) {
				  GList * iter = g_list_nth (items, end_pos + delta);
				  for ( i = 0 ; i < -delta ; ++ i) {
					  GList * next = iter->next;
					  /* The widget is removed automatically from it container,
					   * i.e. menu. See \ref gtk_container_remove.
					   *
					   * We must never destroy the widget on position begin_pos.
					   * It has the name to obtain our starting position. */
					  gtk_widget_destroy (GTK_WIDGET(iter->data));
					  iter = next;
				  }
			  }
			  else { /* 0 == delta */ }

			  /* Set the labels for the menu items. */
			  itemIter = g_list_nth(gtk_container_get_children(
							GTK_CONTAINER(gw->popupmenu)), begin_pos);
			  FOREACH_G_LIST(recent,const gchar*,cur,recentIter,{
				  gtk_menu_item_set_label (GTK_MENU_ITEM(itemIter->data), cur);
				  itemIter = itemIter->next;
			  });
		  }

		  g_list_free (recent);
	  }

	  /* Update the listing of all graphs. There is at least one graph in the
	   * manager, because this is a prerequisite to be here. */
	  {
		  GtkWidget * more = GTK_WIDGET(gtk_builder_get_object (builder, "menuitemMoreGW"));
		  GtkWidget * submenu = gtk_menu_new ();
		  GList/*<const gchar*>*/ * names
			  = xgraph_manager_get_names (xgraph_manager_get_instance ());
		  g_assert (names != NULL);
		  names = g_list_sort(names, (GCompareFunc) g_strcasecmp);

		  FOREACH_G_LIST(names,const gchar*,cur,iter,{
				  GtkWidget * mi = gtk_menu_item_new_with_label (cur);
				  gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
				  g_signal_connect ((gpointer) mi, "activate",
				                    G_CALLBACK (menuitemSetGraphGW_activate), NULL);
				  gtk_widget_show (mi);
		  });

		  /* Set the new sub menu. This destroys the current submenu. */
		  gtk_menu_item_set_submenu(GTK_MENU_ITEM(more), submenu);
	  }

	  /* Show menu option in case of a selection. */
	  {
		SelectionManager * sm = graph_window_get_selection_manager(gw);
		GtkWidget * miSep = GTK_WIDGET(gtk_builder_get_object(builder, "menuitemSelSepGW"));
		GtkWidget * miSelToVec = GTK_WIDGET(gtk_builder_get_object(builder, "menuitemSelToVecGW"));
		GtkWidget * miSelToRel = GTK_WIDGET(gtk_builder_get_object(builder, "menuitemSelToRelGW"));

		if (sm->hasSelection(sm)) {
		  const SelectionClass * selClass = sm->getSelectionClass(sm);
		  glong selType = (glong) selClass->data;

		  gtk_widget_show(miSep);

		  if (selType == GRAPH_SELECTION_TYPE_NODES) {
			gtk_widget_show(miSelToVec);
			gtk_widget_hide(miSelToRel);
		  } else if (selType == GRAPH_SELECTION_TYPE_EDGES) {
			gtk_widget_hide(miSelToVec);
			gtk_widget_show(miSelToRel);
		  } else {
			fprintf(stderr, "Unknown selection class: %ld\n", selType);
			exit(1);
		  }
		} else {
		  gtk_widget_hide(miSep);
		  gtk_widget_hide(miSelToVec);
		  gtk_widget_hide(miSelToRel);
		}
	  }

	  /* The user may create a relation from a graph only, if the graph has at
	   * least one node. */
	  {
		  GtkWidget * item = GTK_WIDGET(gtk_builder_get_object(builder, "menuitemToRelationGW"));
		  g_assert (item != NULL);

		  gtk_widget_set_sensitive(item, ! xgraph_is_empty (gr));
	  }

	  /* Set the visibility of the user defined menu entries. */
	  {
		  GList/*<MenuEntry*>*/ * ents = menu_domain_get_entries(gw->menu_domain);
		  GList * iter = ents;

		  for ( ; iter ; iter = iter->next) {
			  MenuEntry * ent = (MenuEntry*) iter->data;

			  gtk_widget_set_sensitive (menu_entry_get_menuitem(ent),
					  menu_entry_is_enabled(ent));
		  }

		  g_list_free(ents);
	  }

	  gtk_menu_popup(GTK_MENU(gw->popupmenu), NULL, NULL, NULL, NULL,
	  		  event->button, event->time);
  }
  else /* ctrl pressed */ { ; }

  return FALSE;
#else
  if (! (event->state & GDK_CONTROL_MASK)) /* no control pressed */ {
#define MI(name) (GTK_MENU_ITEM(lookup_menuitem (GTK_MENU (gw->popupmenu), (name))))
#define MIW(name) GTK_WIDGET(MI(name))
    gboolean initial = FALSE;
    gboolean isplanar = FALSE;
    gboolean isempty = FALSE;
    GtkWidget * layoutMenu = NULL;

    if ( !gw->popupmenu) {
      initial = TRUE;
      gw->popupmenu = create_popupMenuGraphDefault();
    }

    layoutMenu = gtk_menu_item_get_submenu(MI("menuitem12"));
    if (!layoutMenu) {
      printf("fix drawingarea_RightButtonPress(...)\n");
      assert (layoutMenu != NULL);
    }

    if (initial) {
#if USE_GRAPHVIZ_LAYOUT_SERVICE
      {
        /* Add the items for Graphviz */

#define ADD_MI(menu,label,handler) { \
  GtkWidget * mi = gtk_menu_item_new_with_label (label); \
  gtk_menu_shell_append (GTK_MENU_SHELL ((menu)), mi); \
  gtk_widget_show (mi); \
  g_signal_connect(G_OBJECT(mi), "activate", \
                   G_CALLBACK(handler), (gpointer) gw); }

        ADD_MI(layoutMenu, "dot", _menuitemDotLayout_activate);
        ADD_MI(layoutMenu, "neato", _menuitemNeatoLayout_activate);
        ADD_MI(layoutMenu, "twopi", _menuitemTwopiLayout_activate);
        ADD_MI(layoutMenu, "circo", _menuitemCircoLayout_activate);
        ADD_MI(layoutMenu, "fdp", _menuitemFdpLayout_activate);

        /* Add an import/export options to the top-level menu. */
        ADD_MI(gw->popupmenu, "DOT export", _menuitemDotExport_activate);
        ADD_MI(gw->popupmenu, "DOT plain import", _menuitemImportDotPlainLayout_activate);

#undef ADD_MI

      }
#endif /* USE_GRAPHVIZ_LAYOUT_SERVICE? */

      gtk_menu_attach_to_widget(GTK_MENU(gw->popupmenu),
      widget, NULL);
    } /* initial? */

    /* Show menu option in case of a selection. */
    {
      SelectionManager * sm = graph_window_get_selection_manager(gw);
      GtkWidget * miSep = MIW("menuitemSelSep"), * miSelToVec = MIW("menuitemSelToVec"),
        * miSelToRel= MIW("menuitemSelToRel");

      if (sm->hasSelection(sm)) {
        const SelectionClass * selClass = sm->getSelectionClass(sm);
        glong selType = (glong) selClass->data;

        gtk_widget_show(miSep);

        if (selType == GRAPH_SELECTION_TYPE_NODES) {
          gtk_widget_show(miSelToVec);
          gtk_widget_hide(miSelToRel);
        } else if (selType == GRAPH_SELECTION_TYPE_EDGES) {
          gtk_widget_hide(miSelToVec);
          gtk_widget_show(miSelToRel);
        } else {
          fprintf(stderr, "Unknown selection class: %ld\n", selType);
          exit(1);
        }
      } else {
        gtk_widget_hide(miSep);
        gtk_widget_hide(miSelToVec);
        gtk_widget_hide(miSelToRel);
      }
    }

    layoutMenu = gtk_menu_item_get_submenu(MI("menuitem12"));
      if (!layoutMenu) {
        printf("fix drawingarea_RightButtonPress(...)\n");
        assert (layoutMenu != NULL);
      }


    isempty = xgraph_is_empty(gr);
    /* Disable all options, if the graph is empty. */
    {
      GList * childIter = gtk_container_get_children(GTK_CONTAINER (layoutMenu));
      while (childIter) {
        GtkWidget * mi = GTK_WIDGET (childIter->data);
        gtk_widget_set_sensitive (mi, isempty);
        childIter = childIter->next;
      }
    }

#if 0 // Currently not implemented. These option become a part of the plugin
    // system and thus shouldn't be considered afterwards.
    gtk_widget_set_sensitive(MIW("menuitemForest"), graph_forest_test (gr));

    isplanar = graph_planar_test(gr);
    gtk_widget_set_sensitive(MIW("menuitemPlanarTriangular"), isplanar);
    gtk_widget_set_sensitive(MIW("menuitemPlanarTrimix"), isplanar);
    gtk_widget_set_sensitive(MIW("menuitemPlanarMixmod"), isplanar);

    gtk_widget_set_sensitive(MIW("menuitemOrthogonal"),
    graph_orthogonal_test (gr));
    gtk_widget_set_sensitive(MIW("menuitemOrthogonalFast"),
    graph_orthogonal_test (gr));
#endif

    gtk_menu_popup(GTK_MENU(gw->popupmenu), NULL, NULL, NULL, NULL,
    event->button, event->time);
#undef MI
  }
  else /* ctrl pressed */ { ; }

  return FALSE;
#endif
  }
}

gboolean drawingarea_ButtonRelease(GtkWidget * widget, GdkEventButton * event,
                                   gpointer user_data)
{
  GraphWindow * gw = graph_window_get(widget);
  XGraph * gr = graph_window_get_graph(gw);

  if (graph_window_get_zoom(gw) != 1.0) {
    return FALSE;
  }

  /* If the graph is not valid stop here. */
  if ( !gr)
    return FALSE;

  if (event->state & GDK_CONTROL_MASK && event->button == 1) /* left and control */{
    GdkPoint pt = graph_window_mouse_to_viewport(gw, event->x, event->y);
    SelectionManager * selManager = graph_window_get_selection_manager(gw);

    if (xgraph_is_correspondence (gr))
      return FALSE;

    event->x = pt.x;
    event->y = pt.y;

    if (selManager->mouseLeftButtonPressed(selManager, event))
      graph_window_redraw(gw, OLD);

    if (selManager->hasSelection(selManager)) {
      gtk_label_set_text(GTK_LABEL (gw->labelTooltip),
      "Click inside the free area, with CTRL "
      "pressed, to deselect nodes or edges.");
    } else {
      gtk_label_set_text(GTK_LABEL (gw->labelTooltip), "");
    }
  }

#if 0
  _print_event_button_info (event);
#endif

  return FALSE;
}

/*****************************************************************************/
/*       NAME: drawingarea_ButtonPress (graph_event_proc, 2000.02.07, P.S.)  */
/*    PURPOSE: callback, mouse button click on drawingareaGW                 */
/*             button1: sets/selects nodes or edges                          */
/*             button2: delets selected nodes and edges                      */
/*             button3: displays context menu                                */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 12-MAY-2000 WL                                                */
/*   MODIFIED: 12-JUL-2000 WL: added code to enable/disable graphdrawing     */
/*                             menu items                                    */
/*****************************************************************************/
gboolean drawingarea_ButtonPress(GtkWidget * widget, GdkEventButton * event,
                                 gpointer user_data)
{
#if 1
#  if 0
  _print_event_button_info (event);
#  endif

  if (event->button == 1) {
    return drawingarea_LeftButtonPress(widget, event, user_data);
  } else if (event->button == 2) /* middle */{
    return drawingarea_MiddleButtonPress(widget, event, user_data);
  } else if (event->button == 3 /*right*/) {
    return drawingarea_RightButtonPress(widget, event, user_data);
  }

  return FALSE;

#else
  int n_count;
  int node_state;
  static int n_number [2];
  static int n_state [2]; /* save number and state */
  char title [4];
  int xpos;
  int ypos;
  int delta_grid;
  int isplanar;
  int isempty;
  GdkPoint origin;

  nodelistptr nl = NULL;
  edgelistptr el = NULL;
  intlistptr il;

  Edge * edge;
  Edge * tmp_edge;
  Node * from_node;
  Node * to_node;
  Node * node;

  int num;
  int node_width = DEFAULT_DIM;

  /* Get the amount of nodes being selected for an edge. */
  n_count = get_edgemarked_nodes (graph);

  /* Get the positions of the scrollbar sliders. */
  origin = getCanvasOrigin (hscrollbarGW, vscrollbarGW);

  printf ("canvas origin: %d x %d\n", origin.x, origin.y);

  /* Mouse position in virtual canvas coordinates */
  xpos = event->x + origin.x;
  ypos = event->y + origin.y;

  switch (event->button) {

    case 1: /* left mouse button */

    if (graph->state == CORRESPONDENCE)
    break; /* nothing to do (what is CORRESPONDENCE?) */

    switch (n_count) {

      case 0: /* n_count == 0 */
      node = test_if_node_nearby (graph, xpos, ypos);
      if (!node) {
        /* new node */
        if ((xpos == 0) || (ypos == 0))
        break;
        num = get_last_node (graph);
        sprintf (title, "%d", num + 1);
        if (get_graph_grid ()) {
          /* align to grid? */
          delta_grid = get_graph_delta_grid ();
          xpos = ((xpos + delta_grid / 2) / delta_grid) * delta_grid;
          ypos = ((ypos + delta_grid / 2) / delta_grid) * delta_grid;
        }
        /* create node, add it to the graph, and display it */
        nl = make_node (title, xpos, ypos, node_width, VISIBLE);
        if (nl) {
          append_node (graph, nl);
          display_node (& (nl->node), PM);
          if (!rel_graph_modified)
          init_rel_dir ();
          rel_graph_modified = TRUE;
          graph->state = MODIFIED;
        }
      } else {
        /* nearby node found */
        n_state [n_count] = node->state;
        n_number [n_count] = node->number;
        if (node->state != HELP)
        /* select edge node */
        node->state = SELECTED_FOR_EDGE;
        else
        /* select help node */
        node->state = SELECTED_HELP;
        display_node (node, PM);
      }
      break;

      case 1: /* n_count == 1 */
      node = test_if_node_nearby (graph, xpos, ypos);
      if (!node) {
        /* move node */
        if (get_graph_grid ()) {
          /* align to grid? */
          delta_grid = get_graph_delta_grid ();
          xpos = ((xpos + delta_grid / 2) / delta_grid) * delta_grid;
          ypos = ((ypos + delta_grid / 2) / delta_grid) * delta_grid;
        }
        /* New node coordinates => node is moved. Where is it displayed? */
        set_node_positions (graph, n_number [0], xpos, ypos);
        set_node_state (graph, n_number [0], n_state [0]);
      } else {
        unmark_graph_edges_complete (graph);
        unmark_graph_nodes_complete (graph);
        if (node->state != HELP) {
          /* edge */
          n_number [n_count] = node->number;
          n_state [n_count] = node->state;
          from_node = get_node (graph, n_number [0]);
          to_node = get_node (graph, n_number [1]);
          /* does the edge exist already? */
          if (test_if_edge_exist (graph, n_number [0], n_number [1])) {
            /* yes */
            edge = get_edge (graph, n_number [0], n_number [1]);
            /* select edge */
            if (edge) {
              edge->state = SELECTED;
              display_edge (graph, edge, PM);
            }
          } else {
            /* no => new edge */
            if (from_node) {
              if ((from_node->state != SELECTED_HELP)
                  && (from_node->state != HELP)) {
                el = make_edge ("", n_number [0], n_number [1], VISIBLE);
                if (el) {
                  append_edge (graph, el);
                  edge = get_edge (graph, n_number [0], n_number [1]);
                  if (edge) {
                    tmp_edge = get_edge (graph, n_number [1], n_number [0]);
                    if (tmp_edge) {
                      il = tmp_edge->path;
                      while (il) {
                        prepend_edge_pathnode (edge, il->number);
                        il = il->next;
                      }
                    }
                    display_edge (graph, edge, PM);
                    if (!rel_graph_modified)
                    init_rel_dir ();
                    rel_graph_modified = TRUE;
                    graph->state = MODIFIED;
                  }
                }
              }
            }
          }
        } /* state != HELP */

        set_node_state (graph, n_number [0], n_state [0]);
        if (n_number [0] != n_number [1])
        set_node_state (graph, n_number [1], n_state [1]);
      }
      break;

      default: /* n_count > 2 ==> reset complete operation */
      unmark_graph_edges_complete (graph);
      unmark_graph_nodes_complete (graph);
      break;
    } /* end switch (n_count) */

    redraw_graph (graph, OLD);
    break;

    case 2: /* middle mouse button */
    if (graph->state != CORRESPONDENCE) {
      switch (n_count) {
        case 0: /* delete edge */
        if (test_if_edge_exist (graph, n_number [0], n_number [1])) {
          del_edge (graph, n_number [0], n_number [1]);
          del_help_nodes (graph); /* delete all help nodes that are not
           attached to an edge */
          if (!rel_graph_modified)
          init_rel_dir ();
          rel_graph_modified = TRUE;
          graph->state = MODIFIED;
        }
        break;
        case 1: /* delete node */
        if ((node = get_node (graph, n_number [0]))) {
          node_state = node->state;
          del_node (graph, n_number [0]);
          del_help_nodes (graph);
          /* delete all help nodes that are not
           attached to an edge */
          renumber_graph (graph);
          if ((get_last_node (graph) == 0)
              /* special case: last node deleted */
              || (node_state == SELECTED_HELP)) {
            /* or selected help node */
            rel_graph_modified = FALSE;
            init_rel_dir ();
          } else {
            if (!rel_graph_modified)
            init_rel_dir ();
            rel_graph_modified = TRUE;
            graph->state = MODIFIED;
          }
        }
        break;
        default:
        unmark_graph_edges_complete (graph);
        unmark_graph_nodes_complete (graph);
        break;
      }
    }
    redraw_graph (graph, NEW);
    break;

    case 3: /* right mouse button: show popup menu */

    if (!gw->popupmenu)
    gw->popupmenu = create_popupmenuGraph ();

    isempty = get_last_node (graph);
    gtk_widget_set_sensitive (menuitemSwapUpperLowerGW, isempty);
    gtk_widget_set_sensitive (menuitemSpringFast, isempty);
    gtk_widget_set_sensitive (menuitemSpringSlowGW, isempty);
    gtk_widget_set_sensitive (menuitemLayer, isempty);
    gtk_widget_set_sensitive (menuitemCorrespondenceGW, isempty);

    gtk_widget_set_sensitive (menuitemForestGW, graph_forest_test (gr));

    isplanar = graph_planar_test ();
    gtk_widget_set_sensitive (menuitemPlanarTriangularGW, isplanar);
    gtk_widget_set_sensitive (menuitemPlanarTrimixGW, isplanar);
    gtk_widget_set_sensitive (menuitemPlanarMixmodGW, isplanar);

    gtk_widget_set_sensitive (menuitemOrthogonalGW, graph_orthogonal_test (gr));
    gtk_widget_set_sensitive (menuitemOrthogonalFastGW,
        graph_orthogonal_test (gr));

    gtk_menu_popup (GTK_MENU(gw->popupmenu), NULL, NULL, NULL, NULL,
        event->button, event->time);
    break;
  }

  return FALSE;
#endif
}


/*****************************************************************************/
/*       NAME: drawingarea_MotionNotify                                      */
/*    PURPOSE: callback, mouse cursor has been moved on the drawingareaGW    */
/*             => display coordinates                                        */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 23-MAR-2000 WL                                                */
/*****************************************************************************/
gboolean drawingarea_MotionNotify(GtkWidget * widget, GdkEventMotion * event,
                                  gpointer user_data)
{
  GraphWindow * gw = graph_window_get(widget);
  if (! graph_window_has_graph(gw)) return FALSE;

  if (graph_window_get_zoom(gw) != 1.0) {
    return FALSE;
  }

#if 0
  _print_event_button_info (event);
#endif

  graph_window_set_cursor_position(gw, event->x, event->y);

  /*  */
  {
    GdkPoint pt = graph_window_mouse_to_viewport(gw, event->x, event->y);
    SelectionManager * selManager = graph_window_get_selection_manager(gw);

    event->x = pt.x;
    event->y = pt.y;

    assert (selManager != NULL);
    if (selManager->mouseMoved(selManager, event)) {
    	gtk_widget_queue_draw (gw->drawingarea);
    }
  }

  return FALSE;
}

/*****************************************************************************/
/*       NAME: drawingarea_Configure                                         */
/*    PURPOSE: callback, drawingareaGW resized/shown                         */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 23-MAR-2000 WL                                                */
/*****************************************************************************/
gint drawingarea_Configure(GtkWidget * widget, GdkEventConfigure * event)
{
  GraphWindow * gw = graph_window_get(widget);
  graph_window_redraw(gw, NEW);
  return TRUE;
}

/*****************************************************************************/
/*       NAME: drawingarea_Expose                                            */
/*    PURPOSE: callback, drawingareaGW must be redrawn                       */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 10-APR-2000 WL                                                */
/*****************************************************************************/
gint drawingarea_Expose(GtkWidget * widget, GdkEventExpose * event)
{
  GraphWindow * gw = graph_window_get(widget);
  graph_window_expose(gw, event);
  return FALSE;
}

/*****************************************************************************/
/*       NAME: graphwindow_DeleteEvent                                       */
/*    PURPOSE: callback, the graphwindow has been closed                     */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 10-JUN-2000 Werner Lehmann                                    */
/*****************************************************************************/
gint graphwindow_DeleteEvent(GtkWidget * widget, GdkEvent * event, gpointer data)
{
  GraphWindow * gw = graph_window_get(widget);

  graph_window_hide(gw);
  return (TRUE);
  /* TRUE == do not destroy the window / do not emit the
   signal any further */
}

/****************************************************************************/
/*       NAME : adjustmentValueChangedGW                                    */
/*    PURPOSE : callback, the scrollbar values (actually the adjustment     */
/*              values) have been changed.                                  */
/* PARAMETERS : standard gtk                                                */
/*    CREATED : 24-MAY-2000 WL                                              */
/****************************************************************************/

void scrollValueChangedGW(GtkRange * range, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(GTK_WIDGET(range));
  gtk_widget_queue_draw(GTK_WIDGET (gw->window));
}

/****************************************************************************/
/*       NAME : create_new_graph                                            */
/*    PURPOSE : creates a new graph                                         */
/* PARAMETERS : the name of the new graph                                   */
/*    CREATED : 14-AUG-1995 PS                                              */
/*   MODIFIED : 20-JUN-2000 WL: GTK+ port (introduced parameter)            */
/****************************************************************************/
void create_new_graph(GraphWindow * gw, char * name)
{
	XGraphManager * manager = xgraph_manager_get_instance();

	g_strchomp(name);

	if (strcmp(name, "$") == 0) {
		rv_user_error ("Not allowed", "Sorry. It's not allowed to create a new graph with name $.");
		return;
	}

	/* Ask the user, if we shall overwrite the current relation, if there
	 * is one. */
	if (xgraph_manager_exists(manager, name)) {
		if (error_and_wait(error_msg[overwrite_graph]) == NOTICE_NO)
			return;
	}

	/* If there currently is a graph in the graph window and it has the same
	 * name as the new, just clear the graph. */
	if (graph_window_has_graph(gw)) {
		XGraph * curGr = graph_window_get_graph(gw);
		const gchar * curName = xgraph_get_name(curGr);

		if (g_str_equal(curName, name)) {
			xgraph_clear(curGr);
			return;
		} else {
			/* There is a graph in the window, but it has a different name.
			 * Delete it, if it does not have an associated relation. Ignore
			 * it otherwise. */
			//if (!rel_exist(rel_root, curName))
				//xgraph_destroy(curGr);
		}
	} else {
		/* There was no graph in the graph window.  */
	}

	/* Delete the current graph, if there is one. The user already gave us his
	 * okay. Create and set the new graph afterwards. */
	if (xgraph_manager_exists(manager, name))
		xgraph_manager_delete_by_name(manager, name);

	{
		XGraph * gr = xgraph_manager_create_graph(manager, name);

		graph_window_set_graph(gw, gr);
		graph_window_redraw(gw, NEW);
	}
}

/****************************************************************************/
/*       NAME : menuitemNewGW_activate                                      */
/*    PURPOSE : shows the dialog for "New Graph"                            */
/* PARAMETERS : standard gtk                                                */
/*    CREATED : 20-JUN-2000 WL                                              */
/****************************************************************************/
void menuitemNewGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
  XGraph * gr = graph_window_get_graph(gw);
  GString * caption;
  GString * desc;
  GString * value;

  caption = g_string_new("New Graph");
  desc = g_string_new("Enter new graph name:");
  if (gr)
    value = g_string_new(xgraph_get_name(gr));
  else
    value = g_string_new(NULL);

  if (show_input_string(caption, desc, value, MAXNAMELENGTH))
    create_new_graph(gw, value->str);

  g_string_free(caption, TRUE);
  g_string_free(desc, TRUE);
  g_string_free(value, TRUE);
}

/*****************************************************************************/
/*       NAME: menuitemDeleteGW_activate (WAS: del_gr)                       */
/*    PURPOSE: deletes the current graph                                     */
/* PARAMETERS: standard gtk                                                  */
/*    CREATED: 06-JAN-1997 PS                                                */
/*   MODIFIED: 14-JUN-2000 WL: GTK+ port                                     */
/*****************************************************************************/
void menuitemDeleteGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);

	if (!gr)
		return;
	else {
		const gchar * name = xgraph_get_name(gr);
		gchar * msg = g_strdup_printf("%s\nGRAPH:: %s", error_msg[rel_del],
				name);
		int answer = error_and_wait(msg);
		g_free (msg);


		if (answer == NOTICE_YES) {
			return

#if 1
			/* The rest is done by the observers and the associated graph
			 * manager. */
			xgraph_destroy (gr);

#else
			graphlistptr gl;

		graph_root = del_graph(graph_root, name);
		if (!(gl = mk_graph(name, NORMAL))) {
			error_notifier(error_msg[not_allowed]);
			return;
		}

		rel_graph_modified = FALSE;
		graph_root = append_graph(graph_root, gl);
		gr = get_graph(graph_root, name);
		graph_window_set_graph(gw, gr);
		graph_window_redraw(gw, NEW);
		graph_root = del_graph(graph_root, name);
		init_rel_dir();
#endif
		}
	}
}

/***************************************************************************/
/*       NAME: cp_gr                                                       */
/*    PURPOSE: copies the current graph                                    */
/* PARAMETERS: name of the new graph                                       */
/*    CREATED: 07-JAN-1997 PS                                              */
/*   MODIFIED: 19-JUN-2000 WL: GTK+ port                                   */
/***************************************************************************/
void cp_gr(GraphWindow * gw, const gchar * name)
{
	XGraph * gr = graph_window_get_graph(gw);

#if 1
	if (! gr) return;
	else {
		const gchar * curName = xgraph_get_name (gr);
		XGraphManager * manager = xgraph_manager_get_instance ();

		g_strchomp(name);

#warning TODO: The graph name is no checked here.
#if 0
		if (name_type(name) != REL) {
			error_notifier(error_msg [name_not_allowed]);
			return;
		}
#endif

		/* The copy should have ANOTHER name. */
		if (g_str_equal (name, curName) == 0)
			return;

		/* If a graph with the current name already exists, ask the user
		 * what to do? If we shall overwrite the current graph, we can
		 * simply clear it. */
		if (xgraph_manager_exists (manager, name)) {
			XGraph * copyGr = xgraph_manager_get_by_name (manager, name);

			if (error_and_wait(error_msg [overwrite_graph]) != NOTICE_YES)
				return;
			xgraph_clear (copyGr);
			graph_window_set_graph (gw, copyGr);
		}
		else {
			/* Just create a copy of the given name. The copy becomes the
			 * graph in the window. */
			XGraph * copyGr = xgraph_copy (gr, name);
			graph_window_set_graph (gw, copyGr);
		}
	}
#else
	graphlistptr graph_copy;

  if (!gr)
    return;

  trailing_blanks(name);
  if (name_type(name) != REL) {
    error_notifier(error_msg [name_not_allowed]);
    return;
  }

  if (strcmp(name, gr->name) == 0)
    return;

  if (rel_exist(rel_root, name)) {
    /* relation "name" exists */
    if (graph_exist(graph_root, name)) {
      /* with associated graph */
      if (error_and_wait(error_msg [overwrite_graph]) == NOTICE_YES) {
        graph_root = del_graph(graph_root, name);
        /* overwrite graph */
        /* no relation to current graph (may be deleted) */
        if (!rel_exist(rel_root, gr->name))
          rename_graph(gr, name);
        else {
          /* relation to current graph exists */
          graph_copy = copy_graph(gr, name);
          graph_root = append_graph(graph_root, graph_copy);
        }
      } else
        return;
    } else {
      /* but no graph */
      if (!rel_exist(rel_root, gr->name))
        rename_graph(gr, name);
      else {
        /* relation to current graph exists */
        graph_copy = copy_graph(gr, name);
        graph_root = append_graph(graph_root, graph_copy);
      }
    }
  } else {
    /* if there is no relation "graph->name" */
    /* no relation to current graph (may be deleted) */
    if (!rel_exist(rel_root, gr->name))
      rename_graph(gr, name);
    else {
      /* relation to current graph exists */
      graph_copy = copy_graph(gr, name);
      graph_root = append_graph(graph_root, graph_copy);
    }
  }
  gr = get_graph(graph_root, name);
  graph_window_set_graph(gw, gr);
  if (gr)
    gr->state = MODIFIED;

  rel_graph_modified = FALSE;
  /* ...to have the rel graph dir updated */
  init_rel_dir();
  graph_window_redraw(gw, NEW);
#endif
}

/****************************************************************************/
/*       NAME : menuitemCopyGW_activate                                     */
/*    PURPOSE : Callback: popup menuitem "Copy"                             */
/* PARAMETERS : standard gtk                                                */
/*    CREATED : 19-JUN-2000 WL                                              */
/****************************************************************************/

void menuitemCopyGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);

	if (!gr) return;
	else {
		if (xgraph_is_correspondence(gr)) {
			rv_user_error ("Not allowed", "Sorry, it's not allowed to copy a correspondence graph.");
			return;
		}
		else {
			GString * caption;
			GString * desc;
			GString * value;

			caption = g_string_new("Copy Graph");
			desc = g_string_new("Enter new graph name:");
			value = g_string_new(xgraph_get_name(gr));

			if (show_input_string(caption, desc, value, MAXNAMELENGTH))
				cp_gr(gw, value->str);

			g_string_free(caption, TRUE);
			g_string_free(desc, TRUE);
			g_string_free(value, TRUE);
		}
	}
}


void menuitemMarkNodesGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);

	if (!graph_window_is_editable(gw)) {
		rv_user_error ("Not allowed", "Graph cannot be changed.");
	}
	else {
		GString * caption = g_string_new("Mark Nodes");
		GString * graphname = g_string_new(xgraph_get_name(gr));
		GString * value = g_string_new(NULL);
		RvMarkLevel level = FIRST;

		int n = xgraph_get_node_count (gr);

		if (show_mark_edgesnodes(caption, graphname, value, MAXTERMLENGTH
				+ MAXNAMELENGTH + 1, &level))
		{
			GError * err = NULL;
			KureRel * impl = rv_lang_eval(rv_get_instance(), value->str, &err);

			if (!impl) {
				rv_user_error_with_descr ("Unable to evaluate term",
						err->message, "The evaluation of the term \"%s\" "
						"failed. See below for further details.", value->str);
				g_error_free(err);
			}
			else {
				if (! kure_rel_fits_si(impl)) {
					gchar * dimStr = rv_user_format_impl_dim (impl);
					rv_user_error ("Result too big", "Relation has dimension %s.", dimStr);
					g_free (dimStr);
				}
				else if ( !kure_is_vector (impl, NULL)) {
					rv_user_error ("Not a vector",
							"Only relational vectors can be used to mark nodes in a graph.");
				}
				else if ( kure_rel_get_rows_si(impl) != n) {
					rv_user_error ("Invalid dimension", "The vector has to have %d rows.", n);
				}
				else xgraph_mark_nodes_by_impl (gr, impl, (int) level + 1);

				kure_rel_destroy(impl);
			}
		}

		g_string_free(caption, TRUE);
		g_string_free(graphname, TRUE);
		g_string_free(value, TRUE);
	}
}


void menuitemMarkEdgesGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);

	/*
	 * The relation has to have dimension nxn where n is the number of nodes in
	 * the graph. The relation has to be a subset of the graph's relation.
	 */

	if (!graph_window_is_editable(gw)) {
		rv_user_error ("Not allowed", "Graph cannot be changed.");
	}
	else {
		GString * caption = g_string_new("Mark Edges");
		GString * graphname = g_string_new(xgraph_get_name(gr));
		GString * value = g_string_new(NULL);
		RvMarkLevel level = FIRST;

		int n = xgraph_get_node_count (gr);

		if (show_mark_edgesnodes(caption, graphname, value, MAXTERMLENGTH
				+ MAXNAMELENGTH + 1, &level))
		{
			GError * err = NULL;
			KureRel * impl = rv_lang_eval(rv_get_instance(), value->str, &err); // New Relation

			// get actual Graph Relation
			Rel *rel            = rel_new_from_xgraph (rel_manager_get_context(rv_get_rel_manager(rv_get_instance())), gr);
			KureRel *currentRel = rel_get_impl (rel);
			int scss = 0;


			if (!impl) {
				rv_user_error_with_descr ("Unable to evaluate term",
						err->message, "The evaluation of the term \"%s\" "
						"failed. See below for further details.", value->str);
				g_error_free(err);
			}
			else {
				if (! kure_rel_fits_si(impl)) {
					gchar * dimStr = rv_user_format_impl_dim (impl);
					rv_user_error ("Result too big", "Relation has dimension %s.", dimStr);
					g_free (dimStr);
				}
				else if (kure_rel_get_rows_si(impl) != kure_rel_get_cols_si(impl)) {
					gchar * dimStr = rv_user_format_impl_dim (impl);
					rv_user_error ("Invalid dimension", "Relation has dimension %s. "
							"Number of rows and columns has to be equal.", dimStr);
					g_free (dimStr);
				}
				/* The relation has to have the same dimension as the graph's
				 * relation. */
				else if (kure_rel_get_rows_si(impl) != n || kure_rel_get_cols_si(impl) != n) {
					rv_user_error ("Invalid dimension", "The relation has to have size %dx%d.", n,n);
				}
				/* The relation has to be a subset of the graph's relation */
				else if (!kure_includes(impl,currentRel,scss)) {
					rv_user_error ("Subset violation", "The relation is not a subset of the graphs relation.");
				}
				else {
					xgraph_mark_edges_by_impl (gr, impl, (int) level + 1);
				}

				kure_rel_destroy(impl);
			}
		}

		g_string_free(caption, TRUE);
		g_string_free(graphname, TRUE);
		g_string_free(value, TRUE);
	}
}


void menuitemUnmarkGraphGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));

	if (graph_window_is_editable(gw)) {
		XGraph * gr = graph_window_get_graph(gw);

		if (gr) {
			xgraph_unmark_nodes(gr);
			xgraph_unmark_edges(gr);
		}
	} else
		rv_user_error ("Not allowed", "Graph cannot be changed.");
}

/****************************************************************************/
/*       NAME : menuitemFitInWindowGW_activate (RENAMED: rescale_graph)     */
/*    PURPOSE : adjusts the graph node coordinates to the window client area*/
/*              area                                                        */
/* PARAMETERS : standard gtk                                                */
/*    CREATED : 08-SEP-1995 PS                                              */
/*   MODIFIED : 29-MAY-2000 WL: GTK+ port                                   */
/****************************************************************************/
void menuitemFitInWindowGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	XGraph * gr = graph_window_get_graph(gw);

	if (gr) {
		xgraph_fill_area(gr, graph_window_get_display_width(gw),
				graph_window_get_display_height(gw));
	}
}

/****************************************************************************/
/*       NAME : menuitemShowGridGW_activate                                 */
/*    PURPOSE : toggles the display of the graph grid                       */
/* PARAMETERS : standard gtk                                                */
/*    CREATED : 29-MAY-2000 WL                                              */
/****************************************************************************/
void menuitemShowGridGW_activate(GtkMenuItem * menuitem, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));

  gw->useGrid = !gw->useGrid;
  gtk_item_toggle(GTK_ITEM (menuitem));

  graph_window_redraw(gw, OLD);
}


void menuitemFlipHorzGW_activate (GtkMenuItem * menuitem,
		gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	if (graph_window_has_graph(gw)) {
		xgraph_flip_display_horz(graph_window_get_graph(gw));
	}
}


void menuitemFlipVertGW_activate (GtkMenuItem * menuitem,
		gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));
	if (graph_window_has_graph(gw)) {
		xgraph_flip_display_vert(graph_window_get_graph(gw));
	}
}


void cbZoomLevelGW_Changed(GtkComboBox * widget, gpointer user_data)
{
  GraphWindow * gw = graph_window_get(GTK_WIDGET (widget));
  gchar * sel = gtk_combo_box_get_active_text(widget);
  gint zoom /*%*/= (sel != NULL) ? atoi(sel) : 100;

  if (0 == zoom) /* fit to window */{
    graph_window_fit_zoom_to_graph(gw);
  } else
    graph_window_set_zoom(gw, zoom / 100.0);

  graph_window_redraw(gw, NEW);

  g_free(sel);
}



void menuitemDefaultLayoutGW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	GraphWindow * gw = graph_window_get(GTK_WIDGET(menuitem));

	if ( !graph_window_is_editable(gw)) {
		rv_user_error ("Not allowed", "Graph cannot be changed.");
	}
	else {
		XGraph * gr = graph_window_get_graph(gw);
		xgraph_default_layout (gr, DEFAULT_GRAPH_RADIUS);
	}
}

