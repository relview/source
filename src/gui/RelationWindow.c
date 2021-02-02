/*
 * RelationWindow.c
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

#include "RelationWindow.h"
#include <stdlib.h> /* calloc */
#include "Relview.h" /* rv_get_glade_xml */
#include "utilities.h" /* win_(un)register_window */
#include "prefs.h"
#include <assert.h>
#include <string.h>
#include "Observer.h"

struct _RelationWindow
{
  GtkWidget * window;
  RelationViewport * viewport;
  RelationObserver observer;

  GtkWidget * popup;

  MenuDomain * menu_domain;

  GSList * observers;
};

static void _relation_window_update_title (RelationWindow * self);
static void _relation_window_relation_renamed (RelationWindow * self,
                                               Rel * rel);
static void _relation_window_relation_on_delete (RelationWindow * self,
                                                 Rel * rel);

static
gboolean _relation_window_prepare_popup (RelationViewport * vp,
                                         GtkWidget * popup,
                                         RelationWindow * self);

static void _relation_window_rel_changed (RelationWindow * self, Rel * old_rel, Rel * new_rel);


/*!
 * Creates the popup menu for the \ref RelationWindow and sets up signal
 * handlers.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 */
static void _relview_window_load_popup_menu_from_xml (RelationWindow * self)
{
	//g_warning ("_relview_window_load_popup_menu_from_xml: Deactivated!");

	GtkBuilder * builder = rv_get_gtk_builder(rv_get_instance());

	self->popup = GTK_WIDGET(gtk_builder_get_object(builder, "popupMenuRelationWindow"));
}

// Implements \ref MenuDomainExtractFunc. See \ref graph_window_ctor below.
static gpointer _extract_menu_domain_object (GtkMenuItem * item, gpointer user_data)
{
	/* Returns the \ref RelationWindow object itself. */
	return user_data;
}

static gboolean _non_empty (gpointer user_data, MenuEntry * ent, gpointer object)
{
	return NULL != relation_window_get_relation ((RelationWindow*)object);
}

static void _L (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	kure_L(rel_get_impl(rel));
	rel_changed(rel);
}

static void _O (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	kure_O(rel_get_impl(rel));
	rel_changed(rel);
}

static void _I (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	kure_I(rel_get_impl(rel));
	rel_changed(rel);
}

static void _transpose (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	KureRel * impl = rel_get_impl(rel);
	kure_transpose(impl, impl);
	rel_changed(rel);
}

static void _complement (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	KureRel * impl = rel_get_impl(rel);
	kure_complement(impl, impl);
	rel_changed(rel);
}

static void _trans_closure (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	KureRel * impl = rel_get_impl(rel);
	kure_trans_hull(impl, impl);
	rel_changed(rel);
}

static void _refl_closure (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	KureRel * impl = rel_get_impl(rel);
	kure_refl_hull(impl, impl);
	rel_changed(rel);
}

static void _symm_closure (gpointer user_data, MenuEntry * ent, gpointer object)
{
	Rel * rel = relation_window_get_relation ((RelationWindow*)object);
	KureRel * impl = rel_get_impl(rel);
	kure_symm_hull(impl, impl);
	rel_changed(rel);
}

/*!
 * Creates a new \ref RelationWindow, that has the default relation as its
 * current relation set. It must be destroyed with \ref relation_window_destroy
 * after use.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \return Returns the newly created \ref RelationWindow.
 */
RelationWindow * relation_window_new ()
{
	Relview * rv = rv_get_instance();
	GtkBuilder * builder = rv_get_gtk_builder(rv);
	Workspace * workspace = rv_get_workspace(rv);
  RelationWindow * self
    = (RelationWindow*) calloc (1, sizeof (RelationWindow));


  self->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  //win_register_window (self->window);

  gtk_window_set_title (GTK_WINDOW (self->window), "Relation");
  //gtk_window_set_default_size (GTK_WINDOW (self->window), 200, 230);
  gtk_widget_set_name (GTK_WIDGET (self->window), "windowRelation");

  gtk_signal_connect (GTK_OBJECT(self->window), "delete-event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide_on_delete), NULL);

  self->viewport = relation_viewport_new ();
  relation_viewport_show_caption (self->viewport, FALSE);

  gtk_container_add (GTK_CONTAINER (self->window),
                     relation_viewport_get_widget (self->viewport));

  /* Show all children, but not the window itself. */
  gtk_widget_show_all (gtk_bin_get_child (GTK_BIN (self->window)));

  _relview_window_load_popup_menu_from_xml (self);
  relation_viewport_set_popup
    (self->viewport, self->popup,
     RELATION_VIEWPORT_PREPARE_POPUP_FUNC (_relation_window_prepare_popup),
     self);

  /* Done by Glade
  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (glade_xml_get_widget (self->xml, "menuitemDrawMode"), TRUE);
  */

  /* prepare our RelationObserver object */
  self->observer.renamed
    = RELATION_OBSERVER_RENAMED_FUNC (_relation_window_relation_renamed);
  self->observer.changed = NULL;
  self->observer.onDelete
    = RELATION_OBSERVER_ON_DELETE_FUNC (_relation_window_relation_on_delete);
  self->observer.object = (gpointer) self;

  /* set the initial zoom */
  relation_viewport_set_zoom
    (self->viewport, prefs_get_int ("settings", "relationwindow_zoom", 3));

  /* Set the default relation as initial relation in the viewport. */
  relation_window_set_relation
	  (self, rel_manager_get_by_name(rv_get_rel_manager(rv), "$"));

  /* Register a \ref MenuDomain so e.g. plugins can add menu items to this
   * window. This is only necessary only once. */
  {
		MenuManager * mm = rv_get_menu_manager(rv);

		if ( !menu_manager_get_domain(mm, RELATION_WINDOW_MENU_PREFIX))
		{
			struct {
				char * path;
				char * builder_name;
			} *p, subs[] = { {"operations", "menuitemOperationsRW"},
							 {"actions", "menuitemActionsRW"},
							 {0}};

			self->menu_domain = menu_domain_new_full(RELATION_WINDOW_MENU_PREFIX,
					_extract_menu_domain_object, self, NULL);

			for (p = subs ; p->path != NULL ; ++p) {
				GtkWidget * menu
					= GTK_WIDGET(gtk_builder_get_object(builder, p->builder_name));
				g_assert (menu != NULL);
				menu_domain_add_subdomain(self->menu_domain, p->path,
						GTK_MENU_SHELL(gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu))));
			}

			menu_manager_add_domain(mm, self->menu_domain);
		}

		/* Add some default operations and actions. */
		{
			MenuDomain * dom
				= menu_manager_get_domain (mm, RELATION_WINDOW_MENU_PREFIX);
			MenuEntry * ent;

			ent = menu_entry_new ("L", _non_empty, _L, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("O", _non_empty, _O, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("I", _non_empty, _I, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("Transpose (^)", _non_empty, _transpose, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("Complement (-)", _non_empty, _complement, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("Refl. closure", _non_empty, _refl_closure, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("Symm. closure", _non_empty, _symm_closure, NULL);
			menu_domain_add (dom, "operations", ent);

			ent = menu_entry_new ("Trans. closure", _non_empty, _trans_closure, NULL);
			menu_domain_add (dom, "operations", ent);
		}
	}

  workspace_add_window (workspace, GTK_WINDOW(self->window),  "relation-window");

  if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
	  relation_window_show (self);

  return self;
}


/*!
 * Destroys the \ref RelationWindow and all internal used data.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow to destroy.
 */
void relation_window_destroy (RelationWindow * self)
{
  /* Unregister the observer from the relation, if there is one. */
  {
    Rel * rel = relation_viewport_get_relation (self->viewport);
    if (rel)
      relation_unregister_observer (rel, &self->observer);
  }

  /* The viewport must be removed from the window, to destroy is separately. */
  gtk_container_remove (GTK_CONTAINER (self->window),
                        relation_viewport_get_widget (self->viewport));

  relation_viewport_destroy (self->viewport);
  gtk_widget_destroy (self->window);

  free (self);
}


/*!
 * Returns the window's width and height (in pixels).
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \param pwidth Stores the width of the window in pixels. Must not be NULL.
 * \param pheight Stores the height of the window in pixels. Must not be NULL.
 *
 */
void relation_window_get_size (RelationWindow * self,
                               int * /*out*/ pwidth, int * /*out*/ pheight)
{
  assert (pwidth && pheight);

  gtk_window_get_size (GTK_WINDOW (self->window), pwidth, pheight);
}


/*!
 * Returns the internal \ref RelationViewport object to draw the relation to
 * the window.
 *
 * \warning Using the \ref RelationViewport directly can be very dangerous,
 *          due to internal dependencies. Use the \ref RelationWindow directly
 *          if possible.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \return Returns the internal \ref RelationViewport.
 */
RelationViewport * relation_window_get_viewport (RelationWindow * self)
{
  return self->viewport;
}


/*!
 * Returns the current \ref Rel "Relation" of the given \ref RelationWindow, or
 * NULL, is no current \ref Rel "Relation" is set.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \return Returns the \ref Rel "Relation", or NULL, if no relation is set.
 */
gboolean relation_window_has_relation (RelationWindow * self)
{
  return relation_viewport_has_relation (self->viewport);
}


/*!
 * Returns the visibility of the given \ref RelationWindow.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \return Returns TRUE, if the \ref RelationWindow is visible and FALSE
 *         otherwise.
 */
gboolean relation_window_is_visible (RelationWindow * self)
{
  return self
    && GTK_WIDGET_VISIBLE (relation_window_get_window (self));
}


/*!
 * Hide the given \ref RelationWindow using gtk_widget_hide.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow to hide.
 */
void relation_window_hide (RelationWindow * self)
{
  gtk_widget_hide (relation_window_get_window (self));
}


/*!
 * Show the given \ref RelationWindow using gtk_widget_show.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow to show.
 */
void relation_window_show (RelationWindow * self)
{
  gtk_widget_show (relation_window_get_window (self));
}


/*!
 * Updates the window title of the \ref RelationWindow. If no relation is set, this
 * will also be shown.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 */
static void _relation_window_update_title (RelationWindow * self)
{
  Rel * rel = relation_viewport_get_relation (self->viewport);
  gchar * caption = NULL;

  if (rel) {
	  mpz_t rows, cols;
	  KureRel * impl = rel_get_impl(rel);
	  gchar * rows_str, *cols_str;

	  mpz_init(rows); mpz_init(cols);
	  kure_rel_get_rows (impl, rows);
	  kure_rel_get_cols (impl, cols);

	  rows_str = rv_user_format_large_int (rows);
	  cols_str = rv_user_format_large_int (cols);

	  caption = g_strdup_printf ("Rel %s, %s x %s", rel_get_name(rel),
			  rows_str, cols_str);

	  g_free (rows_str); g_free (cols_str);
	  mpz_clear (rows); mpz_clear (cols);
  }
  else {
    caption = g_strdup ("No Relation");
  }

  gtk_window_set_title (GTK_WINDOW (self->window), caption);
  g_free (caption);
}


/*!
 * Called by the relation, when the relation is going to be deleted. It's part
 * of the \ref RelationObserver interface realization.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \param rel The \ref Rel "Relation", that is going to be deleted.
 */
static void _relation_window_relation_on_delete (RelationWindow * self,
                                                 Rel * rel)
{
  Rel * _rel = relation_window_get_relation (self);

  if (rel != _rel) {
	  g_warning ("_relation_window_relation_on_delete: Wrong relation "
			  "(\"%s\", %p). We currently have \"%s\", %p.\n",
			  rel_get_name(rel), rel, rel_get_name(_rel), _rel);
  }
  else relation_window_set_relation
		  (self, rel_manager_get_by_name(rv_get_rel_manager(rv_get_instance()), "$"));
}


/*!
 * Called by the relation, when the relation has changed its name. It's part
 * of the \ref RelationObserver interface realization.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \param rel The \ref Rel "Relation", that has changed it's name.
 */
static void _relation_window_relation_renamed (RelationWindow * self,
                                               Rel * rel)
{
  Rel * _rel = relation_window_get_relation (self);

  assert (rel == _rel);
  _relation_window_update_title (self);
}


/*!
 * Set the relation in the \ref RelationWindow and so in the
 * \ref RelationViewport.
 *
 * \note Don't use the \ref RelationViewport to directly set the relation.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \param rel The \ref Rel "Relation" to set.
 */
void relation_window_set_relation (RelationWindow * self, Rel * rel)
{
  Rel * _rel = relation_window_get_relation (self);

  if (rel != _rel) {
	  //_relation_window_rel_changed(self, _rel, rel);

	  /* register an observer to the new relation and unregister us from the
	   * previous one. */
	  if (_rel)
		relation_unregister_observer (_rel, &self->observer);

	  relation_viewport_set_relation (self->viewport, rel);
	  _relation_window_update_title (self);

	  _relation_window_rel_changed(self, _rel, rel);

	  if (rel)
		relation_register_observer (rel, &self->observer);
  }
}


/*!
 * Returns the current \ref Rel "Relation" which is shown in the
 * \ref RelationViewport.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \return Returns the \ref Rel "Relation".
 */
Rel * relation_window_get_relation (RelationWindow * self)
{
  return relation_viewport_get_relation (self->viewport);
}


/*!
 * Returns the GtkWindow of the \ref RelationWindow.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param self The \ref RelationWindow.
 * \return Returns the GtkWindow casted to GtkWidget.
 */
GtkWidget * relation_window_get_window (RelationWindow * self)
{
  return self->window;
}


/*!
 * Called by the \ref RelationViewport, when the popup should be shown. We use
 * it to set the sensitivity of some menu items.
 *
 * \author stb
 * \date 08.08.2008
 *
 * \param vp The \ref RelationViewport.
 * \param popup The popup menu.
 * \param self The \ref RelationWindow.
 * \return Always returns TRUE.
 */
static gboolean _relation_window_prepare_popup (RelationViewport * vp,
                                                GtkWidget * popup,
                                                RelationWindow * self)
{
	/* NOTE: Could be used in the feature to show a reduced set of
	 * options and/or to disable some options which are not available
	 * if no relation is present. Additionally, some options should
	 * be deactivated if the respective function cannot handle special
	 * relations. */

	Rel * rel = relation_viewport_get_relation (vp);
	if (! rel) return FALSE; /* don't show menu */
	else return TRUE; /* show menu */
}

#define REL_WINDOW_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,RelationWindowObserver,obj,func, __VA_ARGS__)

void relation_window_register_observer (RelationWindow * self, RelationWindowObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void relation_window_unregister_observer (RelationWindow * self, RelationWindowObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }

void _relation_window_rel_changed (RelationWindow * self, Rel * old_rel, Rel * new_rel) {
	REL_WINDOW_OBSERVER_NOTIFY(self, relChanged, _2(old_rel, new_rel));
}


#include "Graph.h"
#include "utilities.h"
#include "label.h"
#include "msg_boxes.h"
#include "DirWindow.h"
#include "prefs.h"
#include "Relview.h"
#include "LabelChooserDialog.h"
#include "RelationViewport.h"
#include "RelationWindow.h"
#include "GraphWindow.h"
#include "Relation.h" /* rel_is_valid_name */
#include "RelationProxyAdapter.h"
#include "Observer.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>
#include <gtk/gtk.h>


/*****************************************************************************/
/*       NAME : rel_graph                                                    */
/*    PURPOSE : uses the relation as graph connection list (edges, nodes)    */
/*    CREATED : 17-AUG-1995 PS                                               */
/*   MODIFIED : 13-JUN-2000 WL: GTK+ port                                    */
/*****************************************************************************/
static XGraph * _rel_to_xgraph (Rel * rel)
{
	KureRel * impl = rel_get_impl(rel);

	if ( !kure_is_hom(impl, NULL) || !kure_rel_fits_si(impl)) {
		g_warning ("_rel_to_xgraph: Relation is either not "
				"quadratic or too big.");
		return NULL;
	}
	else {
		gint n = (gint) kure_rel_get_rows_si(impl);
		gint i,j;

		XGraph * gr = xgraph_new (rel_get_name(rel));
		GraphBuilder * builder = xgraph_get_builder(gr);
		RelationProxy * proxy = relation_proxy_adapter_new(rel);

		for (i = 0 ; i < n ; i ++) {
			g_assert (builder->buildNode (builder, i));
		}
		for (i = 0 ; i < n ; i ++)
			for (j = 0 ; j < n ; j ++)
				if (proxy->getBit (proxy,j,i))
					if (! builder->buildEdge (builder,i,j)) {
						g_warning ("_rel_to_xgraph: Unable to create an edge %d->%d.\n", i+1,j+1);
					}

		proxy->destroy (proxy);
		builder->destroy (builder);

		return gr;
	}
}

static void _rel_to_graph (RelationViewport * self)
{
	GraphWindow * gw = graph_window_get_instance();
	Rel * rel = relation_viewport_get_relation(self);
	XGraphManager * manager = xgraph_manager_get_instance();
	const gchar * name = rel_get_name(rel);
	XGraph * gr = NULL;

	/* Relation valid? */
	if (!rel)
		return;

	/* Is the relation homogeneous? */
	if ( !kure_is_hom (rel_get_impl(rel), NULL)) {
		rv_user_error ("Invalid dimension", "Only quadratic relations can be "
				"mapped to graphs.");
		return;
	}

	gr = xgraph_manager_get_by_name(manager, name);
	/* Does the graph already exist? */
	if (gr && ! xgraph_is_empty(gr)) {
		if (error_and_wait(error_msg[overwrite_graph]) == NOTICE_NO)
			return;
		else {
			/* Update the current graph and keep its layout (as far as
			 * possible). */
			xgraph_update_from_rel (gr, rel);
		}
	} else {
		GraphLayoutService * layouter = NULL;

		if (gr) /* empty */ xgraph_destroy(gr);

		gr = _rel_to_xgraph(rel);
		g_assert (xgraph_manager_insert(manager, gr));

		layouter = default_graph_layout_service_new(gr, DEFAULT_GRAPH_RADIUS);

		xgraph_apply_layout_service (gr, layouter);

		layouter->destroy (layouter);

		/* We don't have to call xgraph_layout_changed because is a new one
		 * and thus, the graph window will redraw its contents anyway. */
		graph_window_set_graph(gw, gr);

		xgraph_layout_changed(gr);
	}
}


/* ------------------------------------------------------------ PopupMenu --- */


/*****************************************************************************/
/*       NAME : menuitemNew_activate                                         */
/*    PURPOSE : Callback: popup menuitem "New"                               */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 16-JUN-2000 WL                                               */
/*****************************************************************************/
void on_menuitemNewRelVP_activate(GtkMenuItem * menuitem, gpointer user_data)
{
	//RelationViewport * self = relation_viewport_get(GTK_WIDGET (menuitem));
	Relview * rv = rv_get_instance();
	mpz_t cols, rows;
	gchar * name;

	mpz_init(cols);
	mpz_init(rows);

	name = rv_user_new_relation(rv, NULL, cols, rows);
	if (name) {
		Namespace * ns = rv_get_namespace(rv);
		RvObject * conflict = (RvObject*) namespace_get_by_name(ns, name);

		gboolean really_insert = TRUE;

		if (conflict) {
			RvReplaceAction action = rv_user_ask_replace_object2
					(rv, conflict, NULL, FALSE);
			if (action != RV_REPLACE_ACTION_REPLACE) {
				really_insert = FALSE;
			}
			else {
				/* WARN: When relation_window_set_relation (below) is called,
				 *       the this must no longer be the current relation. */
				rv_object_destroy (conflict);
			}
		}

		if (really_insert) {
			Rel * rel = rel_manager_create_rel (rv_get_rel_manager(rv),
					name, rows, cols);
			if (rel) {
				relation_window_set_relation (relation_window_get_instance(), rel);

			}
		}

		g_free (name);
	}

	mpz_clear(rows);
	mpz_clear(cols);
}

/*****************************************************************************/
/*       NAME : menuitemDelete_activate (WAS: del_relation)                  */
/*    PURPOSE : Callback: popup menuitem "Delete"                            */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 28-MAR-1995 PS                                               */
/*   MODIFIED : 19-JUN-2000 WL: GTK+ port                                    */
/*****************************************************************************/
void on_menuitemDeleteRelVP_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	RelationViewport * self = relation_viewport_get(GTK_WIDGET (menuitem));
	Rel * rel = relation_viewport_get_relation(self);

	if (rel) {
		Relview * rv = rv_get_instance();
		const gchar * name = rel_get_name(rel);

		/* Don't delete the $ relation. */
		if (g_str_equal(name, "$")) {
			rv_user_message("Unable to delete",	"The $ relation can't be deleted.");
		}
		else {
			gboolean sure = rv_user_ask_remove_object
					(rv, RV_OBJECT_TYPE_RELATION, name, NULL);

			if (sure) {
				rel_destroy (rel);

				/* We must not call relation_viewport_set_relation directly. We
				 * have to use relation_window_set_relation instead. */
				relation_window_set_relation (relation_window_get_instance(),
						rel_manager_get_by_name(rv_get_rel_manager(rv), "$"));
			}
		}
	}
}

/*****************************************************************************/
/*       NAME : menuitemRename_activate                                      */
/*    PURPOSE : Callback: popup menuitem "Rename"                            */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 19-JUN-2000 WL                                               */
/*****************************************************************************/
void on_menuitemRenameRelVP_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	RelationViewport * self = relation_viewport_get(GTK_WIDGET (menuitem));
	Rel * rel = relation_viewport_get_relation(self);

	if (rel) {
		Relview * rv = rv_get_instance();
		const gchar * old_name = rel_get_name(rel);

		if (g_str_equal(old_name, "$")) {
			rv_user_message("Unable to rename", "The $ relation can't be "
					"renamed. You can evaluate X=$ to make a copy.");
		}
		else {
			GString * caption, *descr, *value;
			RelManager * manager = rel_get_manager (rel);
			gboolean ret;

			caption = g_string_new ("Enter relation name");
			descr = g_string_new ("Rename relation");
			value = g_string_new (old_name);

		    ret = rv_ask_rel_name (caption, descr, value,
		    		FALSE /* Don't default to $ when empty. */);

		    if ( ret) {
		    	gchar * new_name = g_strstrip (g_string_free (value, FALSE));
		    	gboolean sure = TRUE;

		    	/* Rename only, when the new and old name differ. */
			    if ( !g_str_equal (new_name, old_name)) {
			    	if (rel_manager_exists(manager, new_name)) {
						RvReplacePolicy policy = rv_user_ask_replace_object(rv,
								RV_OBJECT_TYPE_RELATION, new_name, NULL, FALSE);
						RvReplaceAction action = RV_REPLACE_POLICY_TO_ACTION(policy);

						if (RV_REPLACE_ACTION_REPLACE == action)
							rel_manager_delete_by_name(manager, new_name);
						else sure = FALSE;
					}

					if (sure)
						rel_rename (rel, new_name);
			    }

			    g_free (new_name);
		    }
		    else /* user canceled */ {
		    	g_string_free (value, TRUE);
		    }

			 g_string_free (descr, TRUE);
			 g_string_free (caption, TRUE);
		}
	}
}


/*****************************************************************************/
/*       NAME : menuitemClear_activate                                       */
/*    PURPOSE : clear the relation (popup menu)                              */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 30-MAR-1995 PS (clear_relation)                              */
/*   MODIFIED : 26-MAR-2000 WL: GTK+ port                                    */
/*****************************************************************************/
void on_menuitemClearRelVP_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  RelationViewport * self = relation_viewport_get (GTK_WIDGET (menuitem));
  Rel * rel = relation_viewport_get_relation(self);

  if (rel) {
    kure_O(rel_get_impl(rel));
    rel_changed(rel);
  }
}

/*****************************************************************************/
/*       NAME : menuitemRandomFill_activate                                  */
/*    PURPOSE : fill the relation with a specified fill percentage (popup    */
/*              menu)                                                        */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : Werner Lehmann                                               */
/*       DATE : 26-MAR-2000 Werner Lehmann                                   */
/*****************************************************************************/
void on_menuitemRandomFillVP_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	RelationViewport * self = relation_viewport_get(GTK_WIDGET (menuitem));
	Rel * rel = relation_viewport_get_relation(self);

	if (rel) {
		gfloat prob;

		if ( !rv_user_get_random_fill_percentage(rv_get_instance(), &prob))
			return;
		else {
			kure_random_simple(rel_get_impl(rel), prob);
			rel_changed(rel);
		}
	}
}

/*****************************************************************************/
/*       NAME : menuitemRelationToGraph_activate                             */
/*    PURPOSE : uses the relation as graph connection list (edges, nodes)    */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 17-AUG-1995 PS                                               */
/*   MODIFIED : 13-JUN-2000 WL: GTK+ port                                    */
/*****************************************************************************/
void on_menuitemRelToGraph_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
  RelationViewport * self = relation_viewport_get (GTK_WIDGET (menuitem));
  _rel_to_graph (self);
}


/*****************************************************************************/
/*       NAME : menuitemLabelRel_activate                                    */
/*    PURPOSE : shows dialog to enter the relation labels                    */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 20-JUN-2000 WL                                               */
/*****************************************************************************/
void on_menuitemLabelRel_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	RelationViewport * self = relation_viewport_get (GTK_WIDGET (menuitem));
	Relview * rv = rv_get_instance ();
	GString *rowLabelName, *colLabelName;
	gboolean ret;
	Rel * rel = relation_viewport_get_relation (self);
	LabelAssoc assoc = {0};

	assoc = rv_label_assoc_get (rv, rel, self);

	rowLabelName = g_string_new (assoc.labels[0] ? label_get_name(assoc.labels[0]) : "");
	colLabelName = g_string_new (assoc.labels[1] ? label_get_name(assoc.labels[1]) : "");

	ret = label_chooser_dialog_preset (rowLabelName, colLabelName);
	if (ret) {
		/* we have to look for the special case '(none)'.
		 * "" means to label_ref(...), that no labeling is desired. g_strdup()
		 * must be used due to the mutating trailing blanks implementation. */
		if (! label_chooser_isLabelSelected (rowLabelName))
			assoc.labels[0] = NULL;
		else assoc.labels[0] = rv_label_get_by_name (rv, rowLabelName->str);

		if (! label_chooser_isLabelSelected (colLabelName))
			assoc.labels[1] = NULL;
		else assoc.labels[1] = rv_label_get_by_name (rv, colLabelName->str);

		rv_label_assoc_set (rv, &assoc);
	}
	else { /* user canceled */ }

	g_string_free (rowLabelName, TRUE);
	g_string_free (colLabelName, TRUE);
}


/*****************************************************************************/
/*       NAME : menuitemUnlabelRel_activate (WAS: rel_unlabel)               */
/*    PURPOSE : removes labels from the current relation                     */
/* PARAMETERS : standard gtk                                                 */
/*    CREATED : 03-JUN-1996 PS                                               */
/*****************************************************************************/
void on_menuitemUnlabelRel_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  RelationViewport * self = relation_viewport_get (GTK_WIDGET (menuitem));
  Rel * rel = relation_viewport_get_relation (self);
  if (rel) {
	  Relview * rv = relation_viewport_get_relview (self);
	  LabelAssoc assoc = rv_label_assoc_get (rv, rel, self);
	  assoc.labels[0] = assoc.labels[1] = NULL;
	  rv_label_assoc_set (rv, &assoc);
  }
}


/*!
 * Called if the user toggles the draw mode. The values are set inside Glade
 * and have to be the same as in RelationViewport.h.
 */
void on_menuitemDrawModeDR_toggled (GtkButton * button, gpointer user_data) {
	relation_viewport_set_line_mode (relation_viewport_get (GTK_WIDGET (button)), MODE_DOWN_RIGHT); }
void on_menuitemDrawModeLR_toggled (GtkButton * button, gpointer user_data) {
	relation_viewport_set_line_mode (relation_viewport_get (GTK_WIDGET (button)), MODE_LEFT_RIGHT); }
void on_menuitemDrawModeTD_toggled (GtkButton * button, gpointer user_data) {
	relation_viewport_set_line_mode (relation_viewport_get (GTK_WIDGET (button)), MODE_DOWN_UP); }
void on_menuitemDrawModeDL_toggled (GtkButton * button, gpointer user_data) {
	relation_viewport_set_line_mode (relation_viewport_get (GTK_WIDGET (button)), MODE_DOWN_LEFT); }


/*****************************************************************************/
/*       NAME : show_relationwindow                                          */
/*    PURPOSE : create relationwindow if necessary and show it               */
/*    CREATED : 28-MAR-2000 WL                                               */
/*****************************************************************************/
void show_relationwindow ()
{
  RelationWindow * rw = relation_window_get_instance ();
  relation_window_show (rw);
}

/*****************************************************************************/
/*       NAME : hide_relationwindow                                          */
/*    PURPOSE : hide relationwindow                                          */
/*    CREATED : Werner Lehmann                                               */
/*       DATE : 28-MAR-2000 Werner Lehmann                                   */
/*****************************************************************************/
void hide_relationwindow ()
{
  RelationWindow * rw = relation_window_get_instance ();
  GtkWidget * window = relation_window_get_window (rw);

  //win_on_hide (window);
  relation_window_hide (rw);
}


/*****************************************************************************/
/*       NAME : relationwindow_isVisible                                     */
/*    PURPOSE : TRUE if windowRelation is created & visible/shown            */
/*    CREATED : Werner Lehmann                                               */
/*       DATE : 28-MAR-2000 Werner Lehmann                                   */
/*****************************************************************************/
int relationwindow_isVisible (void)
{
  return relation_window_is_visible (relation_window_get_instance ());
}



static gchar * _default_filename (gpointer obj)
{
	Rel * rel = (Rel*)obj;
	const gchar * name = rel_get_name(rel);
	if (g_str_equal (name, "$")) return g_strdup ("dollar");
	else return g_strdup (name);
}

static gboolean _export (IOHandler * handler, const gchar * filename, gpointer obj, GError ** perr)
{
	return io_handler_save_single_rel (handler, filename, (Rel*)obj, perr);
}


void on_menuitemRelExportRW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	RelationViewport * rw = relation_viewport_get (GTK_WIDGET(menuitem));
	Rel * rel = relation_viewport_get_relation(rw);
	if (! rel) return;
	else {
		rv_user_export_object (IO_HANDLER_ACTION_SAVE_SINGLE_REL, "Export Relation",
						rel, _default_filename, _export);
	}
}

static gboolean _import (IOHandler * handler, const gchar * filename, GError ** perr)
{
	return io_handler_load_rels (handler, filename, NULL, NULL, perr);
}

/*!
 * The user clicked the "Import" menu item in the relation window.
 */
void on_menuitemRelImportRW_activate (GtkMenuItem * menuitem, gpointer user_data)
{
	rv_user_import_object (IO_HANDLER_ACTION_LOAD_RELS, "Import Relation(s)", _import);
}




RelationWindow * g_relationWindow = NULL;

/*! Create and/or return the global RelationWindow instance. The singleton
 * replaces the global variable relation and all other associated global
 * variables. They are available through the RelationWindow object now.
 *
 * \author stb
 * \return Returns a RelationWindow instance.
 */
RelationWindow * relation_window_get_instance ()
{
  if (NULL ==  g_relationWindow) {
    g_relationWindow = relation_window_new ();

    /* set the window position and size to the values saved in the
      * preferences and show. */
    //{
      //GtkWidget * window = relation_window_get_window (g_relationWindow);

      //prefs_restore_window_info (GTK_WINDOW (window));
      //if (prefs_visible_by_name ("windowRelation"))
        //relation_window_show (g_relationWindow);
    //}
  }

  return g_relationWindow;
}

/*! Destroys the global RelationWindow instance, if there is one.
 *
 * \author stb
 */
void relation_window_destroy_instance ()
{
  if (g_relationWindow) {
    relation_window_destroy (g_relationWindow);
    g_relationWindow = NULL;
  }
}
