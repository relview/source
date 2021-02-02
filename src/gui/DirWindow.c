/*
 * DirWindow.c
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

#include "DirWindow.h"
#include "Relation.h"
#include "Relview.h"
#include "RelationWindow.h"
#include "GraphWindow.h"
#include "Function.h"
#include "Domain.h"
#include "Program.h"
#include "prefs.h"
#include <assert.h>

enum RelListCols {
	REL_COL_DISP_NAME = 0,
	REL_COL_INFO = 1,
	REL_COL_NAME = 2,
	REL_COL_PTR = 3
};

#define DIR_WINDOW_OBJECT_ATTACH_KEY "DirWindowObj"

/* Static data and functions */
struct _DirWindow
{
/* private: */
	Relview * rv;

	GtkWidget * window;
	GtkWidget * paned; /* The Gtk(H|V)Paned. */

	GtkTreeView * rels;
	GtkTreeView * doms;
	GtkTreeView * progs;
	GtkTreeView * funcs;

#define treeviewRels rels

	GtkWidget * radiobuttonHidden;

	GtkWidget * popupRel; /*! Popup widget, when the user right-press
						   * over a relation. */
	GtkWidget * popupFun; /*! Popup widget, when the user right-press
						   * over a function. */
	GtkWidget * popupFunList; /*! Popup widget, when the user right-press
							   * over the function list. */
	GtkWidget * popupProg; /*!< Popup for a program in the list. */

	Rel * popupRelSel; /*! Relation to use in the popup. May be NULL. */

	RelationObserver observer;
	RelManagerObserver r_m_observer;
	XGraphManagerObserver g_m_observer;
	DomManagerObserver dm_observer;
	FunManagerObserver fm_observer;
	ProgManagerObserver pm_observer;

	RelationWindowObserver rw_observer;

	PrefsObserver prefsObserver;
};


DirWindow * _dirwindow = NULL;


/*******************************************************************************
 *                                 Prototypes                                  *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

static DirWindow * 	_dir_window_ctor (DirWindow * self, Relview * rv);
static DirWindow * 	_dir_window_get (GtkWidget * widget);
static DirWindow * 	_dir_window_new (Relview * rv);
static void 		_dir_window_dtor (DirWindow * self);
static void 		_dir_window_destroy (DirWindow * self);
static void 		_dir_window_update_orientation (DirWindow * self);
static gboolean 	_dir_window_get_show_hidden (DirWindow * self);
static void 		_dir_window_update_rels (DirWindow * self);
static void 		_dir_window_update_doms (DirWindow * self);
static void 		_dir_window_update_labels (DirWindow * self);
static void 		_dir_window_update_functions (DirWindow * self);
static void 		_dir_window_update_programs (DirWindow * self);

static Rel * 		_dir_window_get_sel_rel (DirWindow * self);
static Dom * 		_dir_window_get_sel_dom (DirWindow * self);
static Fun * 		_dir_window_get_sel_fun (DirWindow * self);
static Prog * 		_dir_window_get_sel_prog (DirWindow * self);

static void 		_delete_selected_rel (DirWindow * self);
static gboolean 	_filter_and_delete_graph_func (Rel * rel, gpointer user_data);
static void 		_select_rel (DirWindow * self, const Rel * rel);
static gboolean 	_filter_dom_func (Dom * obj, gpointer user_data);

static void			_prefs_changed (DirWindow * self, const gchar* section, const gchar* key);

static void 		_rel_manager_changed (DirWindow * self, RelManager * manager);
static void 		_rel_manager_on_delete (DirWindow * self, RelManager * manager);

static void 		_graph_manager_changed (DirWindow * self, XGraphManager * manager);
static void 		_graph_manager_on_delete (DirWindow * self, XGraphManager * manager);

static void 		_fun_manager_changed (DirWindow * self, FunManager * manager);
static void 		_fun_manager_on_delete (DirWindow * self, FunManager * manager);

static void 		_dom_manager_changed (DirWindow * self, DomManager * manager);
static void 		_dom_manager_on_delete (DirWindow * self, DomManager * manager);

static void 		_prog_manager_changed (DirWindow * self, ProgManager * manager);
static void 		_prog_manager_on_delete (DirWindow * self, ProgManager * manager);

static void 		_rel_renamed (DirWindow * self, Rel * rel);
static void 		_rel_changed (DirWindow * self, Rel * rel);
static void 		_on_rel_delete (DirWindow * self, Rel * rel);

static void 		_rel_window_rel_changed (gpointer, RelationWindow*, Rel*,Rel*);

static void 		_showRelInfoDialog (Rel * rel);
static void 		_showRelBdd(Rel * rel);

G_MODULE_EXPORT void menuitemShowBdd_activate (GtkMenuItem * menuitem, DirWindow * self);

static void 		_selected_rel_changed (GtkTreeSelection * sel,
										   gpointer user_data);
static void 		_selected_dom_changed (GtkTreeSelection * sel,
										   gpointer user_data);
static void 		_selected_prog_changed (GtkTreeSelection * sel,
										   gpointer user_data);
static void 		_selected_func_changed (GtkTreeSelection * sel,
										   gpointer user_data);

static void 		del_all_relations ();
static void 		del_all_funs ();
static void 		del_all_progs ();
static void 		del_all_doms ();
static gchar * 		_build_rel_dim (Rel * rel);

static gboolean _find_rel_row_by_pointer (DirWindow * self, const Rel * rel, GtkTreeIter * iter);

/* external functions */
extern int name_type (char *); /* from *.lex */

static gint _name_compare (GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,	gpointer user_data);

/*******************************************************************************
 *                                 Auxillaries                                 *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

gchar * _build_rel_name (Rel * rel)
{
	if (g_str_equal (rel_get_name(rel), "$")) return g_strdup ("$");
	else if (rel_is_hidden (rel)) return g_strdup_printf ("*%s", rel_get_name(rel));
	else return g_strdup(rel_get_name(rel));
}

gchar * _build_rel_dim (Rel * rel)
{
	gchar * res = NULL;
	gboolean has_graph = xgraph_manager_exists(xgraph_manager_get_instance(),
			rel_get_name (rel));
	gchar * dimStr = rv_user_format_impl_dim (rel_get_impl(rel));

	res = g_strdup_printf ("%s   %s", (has_graph ? "graph" : "-----"), dimStr);

	g_free (dimStr);
	return res;
}


/*!
 * Uses as GtkTreeIterCompareFunc in \ref _dir_window_ctor.
 */
gint _name_compare (GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b,
		gpointer user_data)
{
	const gchar *name_a, *name_b;

	gtk_tree_model_get(model,a,0,&name_a,-1);
	gtk_tree_model_get(model,b,0,&name_b,-1);

	/* Skip prefixes for hidden relations. */
	if (g_str_has_prefix(name_a, "*")) name_a ++;
	if (g_str_has_prefix(name_b,"*")) name_b ++;

	if (!name_a || !name_b) return -1;
	else if (g_str_equal (name_a, "$")) return 1;
	else if (g_str_equal (name_b, "$")) return -1;
	else return g_ascii_strcasecmp (name_a, name_b);
}

/*!
 * Deletes the currently selected relation if it's different from $.
 * Additionally deleted the corresponding graph, if there is one.
 */
void _delete_selected_rel (DirWindow * self)
{
	Rel * rel = _dir_window_get_sel_rel (self);

	if (rel) {
		const gchar * name = rel_get_name(rel);

		/* Don't delete the $ relation. */
		if (g_str_equal (name, "$")) {
			rv_user_message("Unable to delete", "The $ relation can't be deleted.");
		}
		else {
			RelManager * manager = rv_get_rel_manager(self->rv);

			rel_manager_delete_by_name (manager, rel_get_name(rel));
			xgraph_manager_delete_by_name(xgraph_manager_get_instance(), name);
			_dir_window_update_rels (self);
		}
	}
}

void _select_rel (DirWindow * self, const Rel * rel)
{
	GtkTreeIter iter;
	_find_rel_row_by_pointer(self, rel, &iter);
	gtk_tree_selection_select_iter (gtk_tree_view_get_selection
			(self->rels), &iter);
}


 gboolean
_find_rel_row_by_pointer (DirWindow * self, const Rel * rel, GtkTreeIter * iter)
{
	GtkTreeModel * model = gtk_tree_view_get_model (self->treeviewRels);

	if (gtk_tree_model_get_iter_first(model, iter)) {
		do {
			Rel * cur_rel;
			gtk_tree_model_get (model,iter,REL_COL_PTR,&cur_rel,-1); /* 0 is name */
			if (cur_rel == rel)
				return TRUE;
		} while (gtk_tree_model_iter_next(model, iter));
	}

	return FALSE;
}


/*******************************************************************************
 *                             private: DirWindow                              *
 *                    (Private operation for a DirWindow.)                     *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/


DirWindow * _dir_window_ctor (DirWindow * self, Relview * rv)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv);
	self->rv = rv;
	self->window = GTK_WIDGET(gtk_builder_get_object(builder, "windowDir"));

	/* Attach the window widget to the window. You can call _file_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(self->window), DIR_WINDOW_OBJECT_ATTACH_KEY,
	                   (gpointer) self);

	self->paned = GTK_WIDGET(gtk_builder_get_object(builder, "panedDir"));

	self->rels = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewRelsDir"));
	self->doms = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewDomsDir"));
	self->progs = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewProgsDir"));
	self->funcs = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewFuncsDir"));

	/* We have to register the signal handlers for the lists by hand,
	 * because Glade currently cannot handle the GtkTreeSelection on its
	 * own. */
#define CONNECT(tv,clbk) g_signal_connect (G_OBJECT(gtk_tree_view_get_selection(tv)),\
		"changed", G_CALLBACK(clbk), self)
	CONNECT(self->rels, _selected_rel_changed);
	CONNECT(self->progs, _selected_prog_changed);
	CONNECT(self->funcs, _selected_func_changed);
#undef CONNECT

	/* All lists are sorted w.r.t. names. */
#define SORT(tv,cmp) gtk_tree_sortable_set_default_sort_func \
		(GTK_TREE_SORTABLE(gtk_tree_view_get_model(tv)), (cmp), NULL, NULL)
	SORT(self->rels, _name_compare);
	SORT(self->doms, _name_compare);
	SORT(self->progs, _name_compare);
	SORT(self->funcs, _name_compare);
#undef SORT

	self->radiobuttonHidden = GTK_WIDGET(gtk_builder_get_object (builder, "checkbuttonShowHiddenDir"));

	self->popupRel	   = GTK_WIDGET(gtk_builder_get_object(builder, "popupMenuRelationDir"));
	gtk_menu_attach_to_widget(GTK_MENU(self->popupRel), self->window, NULL);
	self->popupFun     = GTK_WIDGET(gtk_builder_get_object(builder, "popupMenuFunInList"));
	gtk_menu_attach_to_widget(GTK_MENU(self->popupFun), self->window, NULL);
	self->popupFunList = GTK_WIDGET(gtk_builder_get_object(builder, "popupMenuFunList"));
	gtk_menu_attach_to_widget(GTK_MENU(self->popupFunList), self->window, NULL);
	self->popupProg = GTK_WIDGET(gtk_builder_get_object(builder, "popupMenuProgInList"));
	gtk_menu_attach_to_widget(GTK_MENU(self->popupProg), self->window, NULL);

	/* Prepare the relation observer */
	self->observer.changed
		= RELATION_OBSERVER_CHANGED_FUNC(_rel_changed);
	self->observer.renamed
		= RELATION_OBSERVER_RENAMED_FUNC(_rel_renamed);
	self->observer.onDelete
		= RELATION_OBSERVER_RENAMED_FUNC(_on_rel_delete);
	self->observer.object = (gpointer) self;

	self->r_m_observer.changed
		 = (RelManagerObserver_changedFunc) _rel_manager_changed;
	self->r_m_observer.onDelete
		= (RelManagerObserver_onDeleteFunc) _rel_manager_on_delete;
	self->r_m_observer.object = self;
	rel_manager_register_observer(rv_get_rel_manager(self->rv), &self->r_m_observer);

	/* Prepare the graph manager observer */
	self->g_m_observer.changed
		 = (XGraphManagerObserver_changedFunc) _graph_manager_changed;
	self->g_m_observer.onDelete
		 = (XGraphManagerObserver_onDeleteFunc) _graph_manager_on_delete;
	self->g_m_observer.object = self;
	{
		XGraphManager * gm = xgraph_manager_get_instance();
		xgraph_manager_register_observer(gm, &self->g_m_observer);
	}

	/* Prepare the domain manager observer */
	self->dm_observer.changed
		= (DomManagerObserver_changedFunc) _dom_manager_changed;
	self->dm_observer.onDelete
		= (DomManagerObserver_onDeleteFunc) _dom_manager_on_delete;
	self->dm_observer.object = self;
	dom_manager_register_observer(rv_get_dom_manager(self->rv), &self->dm_observer);

	/* Prepare the function manager observer */
	self->fm_observer.changed
		= (FunManagerObserver_changedFunc) _fun_manager_changed;
	self->fm_observer.onDelete
		= (FunManagerObserver_onDeleteFunc) _fun_manager_on_delete;
	self->fm_observer.object = self;
	fun_manager_register_observer(fun_manager_get_instance(), &self->fm_observer);

	/* Prepare the program manager observer */
	self->pm_observer.changed
		= (ProgManagerObserver_changedFunc) _prog_manager_changed;
	self->pm_observer.onDelete
		= (ProgManagerObserver_onDeleteFunc) _prog_manager_on_delete;
	self->pm_observer.object = self;
	prog_manager_register_observer(prog_manager_get_instance(), &self->pm_observer);

	/* Has orientation changed? */
	self->prefsObserver.changed = PREFS_OBSERVER_CHANGED_FUNC(_prefs_changed);
	self->prefsObserver.object = self;
	prefs_register_observer (&self->prefsObserver);

	self->rw_observer.relChanged = _rel_window_rel_changed;
	self->rw_observer.object = self;
	relation_window_register_observer(relation_window_get_instance(), &self->rw_observer);

	_dir_window_update_orientation (self);

#if 0
	/* Show the window. The window preferences are restored by the
	 * \ref dir_window_show operation. */
	if (prefs_visible_by_name ("windowDirectory")) {
		dir_window_show (self);
	}
#else
	{
		Workspace * workspace = rv_get_workspace(rv);

		/* Add the window to the workspace. Also restores position and visibility. */
		workspace_add_window (workspace, GTK_WINDOW(self->window),
				"directory-window");

		if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window))) {
			dir_window_show (self);
		}
	}
#endif

	// Initialize
	dir_window_update (self);

	return self;
}

DirWindow * _dir_window_get (GtkWidget * widget)
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
		= g_object_get_data (G_OBJECT(widget), DIR_WINDOW_OBJECT_ATTACH_KEY);

	  if (pdata) {
		return (DirWindow*) pdata;
	  }

	  widget = gtk_widget_get_parent (widget);
	}
  }

  assert (FALSE);
  return NULL;
}

// Friend of \ref Relview.
DirWindow * _dir_window_new (Relview * rv)
{ return _dir_window_ctor (g_new0 (DirWindow,1), rv); }

void _dir_window_dtor (DirWindow * self)
{

}

void _dir_window_destroy (DirWindow * self)
{
    prefs_set_int ("settings", "dirwindow_childsize",
                   GTK_PANED (self->paned)->child1_size);

	_dir_window_dtor (self);
	g_free (self);
}

void _dir_window_update_orientation (DirWindow * self)
{
	GtkOrientation o;

	  if (!prefs_get_int ("settings", "xrvprog-orientation", 0)) { /* vertical */
		  o = GTK_ORIENTATION_VERTICAL;
		  gtk_orientable_set_orientation (GTK_ORIENTABLE(self->paned), GTK_ORIENTATION_VERTICAL);
	  }
	  else /* horizontal */ {
		  o = GTK_ORIENTATION_HORIZONTAL;
	  }

	  gtk_orientable_set_orientation (GTK_ORIENTABLE(self->paned), o);
}

// Returns TRUE, if the "hidden" object should be shown.
gboolean _dir_window_get_show_hidden (DirWindow * self)
{
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(self->radiobuttonHidden));
}


void _dir_window_update_rels (DirWindow * self)
{
	GtkTreeView * treeview = self->rels;
	GtkListStore * model = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	GtkTreeIter iter;
	gboolean show_hidden = _dir_window_get_show_hidden(self);
	RelManager * manager = rv_get_rel_manager(self->rv);

	Rel const * sel_rel
		= relation_window_get_relation(relation_window_get_instance());

	/* First, clear everything. */
	gtk_list_store_clear(model);

	/* Second, show all relations. The hidden relations are shown, if the
	 * corresponding check button was selected (by the user). */
	FOREACH_REL(manager, rel, rel_iter, {
			//fprintf (stderr, "cur rel: %p\n", rel);
			//fprintf (stderr, "cur rel: \"%s\"\n", rel_get_name(rel));

		if (!rel_is_hidden(rel) || show_hidden || g_str_equal(rel_get_name(rel), "$")) {
			gchar * info = _build_rel_dim (rel);
			gchar * disp_name = _build_rel_name(rel);

			/* Add observer to the relation, if not already done. */
			relation_register_observer (rel, &self->observer);

			//printf ("disp_name: %s, info: %s, name: %s, ptr: %p\n",
					//disp_name, info, rel_get_name(rel), rel);

			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					REL_COL_DISP_NAME, disp_name,
					REL_COL_INFO, info,
					REL_COL_NAME, rel_get_name(rel), /* Used to identify relations */
					REL_COL_PTR, rel,
					-1);
			g_free (info);
			g_free (disp_name);
		}
	});

	/* This is the cause for the graph window to update the (static) edges
	 * incorrectly. */
#warning TODO: See note. Reselection of relations is currently disabled.
	/* Third, reselect the previous selected relation (if there was one). */
	//if (sel_rel != NULL)
		//_select_rel (self, sel_rel);
}

void _dir_window_update_doms (DirWindow * self)
{
	GtkTreeView * treeview = self->doms;
	GtkListStore * model = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	GtkTreeIter iter;
	gboolean show_hidden = _dir_window_get_show_hidden(self);
	DomManager * manager = rv_get_dom_manager(self->rv);

	/* First, clear everything. */
	gtk_list_store_clear(model);

	/* Second, show all object. The hidden objects are shown, if the
	 * corresponding check button was selected (by the user). */
	FOREACH_DOM(manager, cur, obj_iter, {
		if (!dom_is_hidden(cur) || show_hidden) {
			gchar * disp_name
				= g_strconcat(dom_is_hidden(cur)?"*":"", dom_get_name(cur), NULL);
			gchar * term = dom_get_term (cur);

			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					0, disp_name,
					1, term,
					2, cur,
					-1);
			g_free (disp_name);
			g_free (term);
		}
	});
}

void _dir_window_update_labels (DirWindow * self) { }

void _dir_window_update_functions (DirWindow * self)
{
	GtkTreeView * treeview = self->funcs;
	GtkListStore * model = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	GtkTreeIter iter;
	gboolean show_hidden = _dir_window_get_show_hidden(self);
	FunManager * manager = fun_manager_get_instance ();

	/* First, clear everything. */
	gtk_list_store_clear(model);

	/* Second, show all object. The hidden objects are shown, if the
	 * corresponding check button was selected (by the user). */
	FOREACH_FUN(manager, cur, obj_iter, {
		gboolean is_hidden = fun_is_hidden(cur);

		if ( !is_hidden || show_hidden) {
			gchar * disp_name = g_strconcat(is_hidden?"*":"", fun_get_def(cur), NULL);

			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					0, disp_name,
					1, fun_get_name(cur),
					2, cur,
					-1);
			g_free (disp_name);
		}
	});
}

void _dir_window_update_programs (DirWindow * self)
{
	GtkTreeView * treeview = self->progs;
	GtkListStore * model = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	GtkTreeIter iter;
	gboolean show_hidden = _dir_window_get_show_hidden(self);
	ProgManager * manager = prog_manager_get_instance ();

	/* First, clear everything. */
	gtk_list_store_clear(model);

	/* Second, show all object. The hidden objects are shown, if the
	 * corresponding check button was selected (by the user). */
	FOREACH_PROG(manager, cur, obj_iter, {
		gboolean is_hidden = prog_is_hidden(cur);

		if ( !is_hidden || show_hidden) {
			gchar * disp_name = g_strconcat(is_hidden?"*":"", prog_get_signature(cur), NULL);

			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
					0, disp_name,
					1, prog_get_name(cur),
					2, cur,
					-1);
			g_free (disp_name);
		}
	});
}

/*!
 * Returns NULL is no relation is selected.
 */
Rel * _dir_window_get_sel_rel (DirWindow * self)
{
	GtkTreeSelection * sel = gtk_tree_view_get_selection(self->rels);
	GtkTreeIter iter;
	GtkTreeModel * model = NULL;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	else {
		const gchar * name = NULL;
		gtk_tree_model_get(model, &iter,REL_COL_NAME, &name, -1);
		return rel_manager_get_by_name(rv_get_rel_manager(self->rv), name);
	}
}

/*!
 * Returns NULL is no domain is selected.
 */
Dom * _dir_window_get_sel_dom (DirWindow * self)
{
	GtkTreeSelection * sel = gtk_tree_view_get_selection(self->doms);
	GtkTreeIter iter;
	GtkTreeModel * model = NULL;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	else {
		const gchar * name = NULL;
		gtk_tree_model_get(model, &iter, 0/*col*/, &name, -1);
		return dom_manager_get_by_name(rv_get_dom_manager(self->rv), name);
	}
}

/*!
 * Returns NULL is no function is selected.
 */
Fun * _dir_window_get_sel_fun (DirWindow * self)
{
	GtkTreeSelection * sel = gtk_tree_view_get_selection(self->funcs);
	GtkTreeIter iter;
	GtkTreeModel * model = NULL;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	else {
		const gchar * name = NULL;
		/* REMARK: The first column (index 0) is the term and not the name. */
		gtk_tree_model_get(model, &iter, 1/*name*/, &name, -1);
		return fun_manager_get_by_name(fun_manager_get_instance(), name);
	}
}

/*!
 * Returns NULL is no function is selected.
 */
Prog * _dir_window_get_sel_prog (DirWindow * self)
{
	GtkTreeSelection * sel = gtk_tree_view_get_selection(self->progs);
	GtkTreeIter iter;
	GtkTreeModel * model = NULL;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	else {
		const gchar * name = NULL;
		/* REMARK: The first column (index 0) is the signature and not the name. */
		gtk_tree_model_get(model, &iter, 1/*name*/, &name, -1);
		return prog_manager_get_by_name(prog_manager_get_instance(), name);
	}
}

/*******************************************************************************
 *                              public: DirWindow                              *
 *                     (Public operation for a DirWindow.)                     *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/


void dir_window_update (DirWindow * self)
{
	_dir_window_update_rels (self);
	_dir_window_update_doms (self);
	_dir_window_update_programs (self);
	_dir_window_update_labels (self);
	_dir_window_update_functions (self);
}

void dir_window_show (DirWindow * self)
{
	//prefs_restore_window_info_with_name (GTK_WINDOW(self->window), "windowDirectory");
	//gtk_widget_set_visible(self->window,TRUE);
	gtk_widget_show (self->window);
}
void dir_window_hide (DirWindow * self)
{
    prefs_set_int ("settings", "dirwindow_childsize",
                   GTK_PANED (self->paned)->child1_size);
    gtk_widget_hide (self->window);
	//gtk_widget_set_visible(self->window, FALSE);
}
gboolean dir_window_is_visible (DirWindow * self)
{
#if GTK_MINOR_VERSION >= 18
	return gtk_widget_get_visible (self->window);
#else
	return GTK_WIDGET_VISIBLE(self->window);
#endif
}

GtkWidget * dir_window_get_widget (DirWindow * self) { return GTK_WIDGET(self->window); }

DirWindow * dir_window_get_instance ()
{
	if (! _dirwindow)
		_dirwindow = _dir_window_new (rv_get_instance());
	return _dirwindow;
}


void dir_window_set_show_hidden (DirWindow * self, gboolean yesno)
{
	/* The rest is done by callbacks */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(self->radiobuttonHidden), yesno);
}

/*******************************************************************************
 *                       Callbacks for RelationObserver                        *
 *                       (Each relation is registered.)                        *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

/*!
 * Callback for the RelationObserver.
 */
void _rel_changed (DirWindow * self, Rel * rel)
{
	GtkTreeIter iter;
	gboolean found;

	found = _find_rel_row_by_pointer (self, rel, &iter);
	if (found) {
		GtkListStore * liststore
			= GTK_LIST_STORE(gtk_tree_view_get_model (self->treeviewRels));
		gchar * dim = _build_rel_dim (rel);
		gtk_list_store_set(liststore, &iter, 1, dim, -1);
		g_free (dim);
	}
}

/*!
 * Callback for the RelationObserver.
 */
void _on_rel_delete (DirWindow * self, Rel * rel)
{
	GtkTreeIter iter;
	gboolean found;

	found = _find_rel_row_by_pointer (self, rel, &iter);
	if (found) {
		GtkListStore * liststore
			= GTK_LIST_STORE(gtk_tree_view_get_model (self->treeviewRels));
		gtk_list_store_remove(liststore, &iter);
	}
}

/*!
 * Callback for the RelationObserver.
 */
void _rel_renamed (DirWindow * self, Rel * rel)
{

	GtkTreeIter iter;
	gboolean found;

	found = _find_rel_row_by_pointer (self, rel, &iter);
	if (found) {
		GtkListStore * liststore
			= GTK_LIST_STORE(gtk_tree_view_get_model (self->treeviewRels));
		gtk_list_store_set(liststore, &iter, 0, rel_get_name(rel), -1);
	}

}


/*******************************************************************************
 *                         Callbacks for PrefsObserver                         *
 *                          (Mainly the orientation)                           *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

void _prefs_changed (DirWindow * self, const gchar* section, const gchar* key)
{
	if (g_ascii_strcasecmp ("settings", section) == 0
			&& g_ascii_strcasecmp("xrvprog-orientation", key) == 0) {
		VERBOSE(VERBOSE_INFO, printf ("\tChanging orientation.\n");)
		_dir_window_update_orientation (self);
	}
}

/*******************************************************************************
 *                      Callbacks for RelManagerObserver                       *
 *                                                                             *
 *                               Fri, 9 Apr 2010                               *
 ******************************************************************************/

static void	_rel_manager_changed (DirWindow * self, RelManager * manager)
{
	_dir_window_update_rels (self);
}

static void _rel_manager_on_delete (DirWindow * self, RelManager * manager)
{
	_dir_window_update_rels (self);
}

/*******************************************************************************
 *                     Callbacks for GraphManagerObserver                      *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

void _graph_manager_changed (DirWindow * self, XGraphManager * manager)
{
	_dir_window_update_rels (self);
}


void _graph_manager_on_delete (DirWindow * self, XGraphManager * manager)
{
	_dir_window_update_rels (self);
}

/*******************************************************************************
 *                      Callbacks for DomManagerObserver                       *
 *                                                                             *
 *                               Wed, 3 Mar 2010                               *
 ******************************************************************************/

void _dom_manager_changed (DirWindow * self, DomManager * manager)
{
	_dir_window_update_doms (self);
}


void _dom_manager_on_delete (DirWindow * self, DomManager * manager)
{
	_dir_window_update_doms (self);
}

/*******************************************************************************
 *                      Callbacks for FunManagerObserver                       *
 *                                                                             *
 *                               Wed, 3 Mar 2010                               *
 ******************************************************************************/

void _fun_manager_changed (DirWindow * self, FunManager * manager)
{
	_dir_window_update_functions (self);
}


void _fun_manager_on_delete (DirWindow * self, FunManager * manager)
{
	_dir_window_update_functions (self);
}

/*******************************************************************************
 *                     Callbacks for ProgManagerObserver                       *
 *                                                                             *
 *                               Wed, 3 Mar 2010                               *
 ******************************************************************************/

void _prog_manager_changed (DirWindow * self, ProgManager * manager)
{
	_dir_window_update_programs (self);
}


void _prog_manager_on_delete (DirWindow * self, ProgManager * manager)
{
	_dir_window_update_programs (self);
}


/*******************************************************************************
 *                   Callbacks for RelationWindowObserver                      *
 *                                                                             *
 *                               Wed, 3 Mar 2010                               *
 ******************************************************************************/

void _rel_window_rel_changed (gpointer object, RelationWindow * rw,
		Rel * old_rel, Rel * new_rel)
{
	DirWindow * self = (DirWindow*) object;
	_select_rel (self, new_rel);
}

/*******************************************************************************
 *                          Relation-Popup Callbacks                           *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/


/*******************************************************************************
 *                              Widget Callbacks                               *
 *                                                                             *
 *                               Tue, 2 Mar 2010                               *
 ******************************************************************************/

void on_buttonCloseDir_clicked (GtkButton * button, gpointer user_data) {
	 dir_window_hide (_dir_window_get(GTK_WIDGET(button))); }

void on_checkbuttonShowHiddenDir_toggled (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(button));
	dir_window_update (self);
}

static void			_selected_rel_changed (GtkTreeSelection * sel,
								   gpointer user_data)
{
	DirWindow * self = (DirWindow*) user_data;
	Rel * rel = _dir_window_get_sel_rel (self);
	if (! rel) { /* nothing selected */ }
	else {
		XGraphManager * gm = xgraph_manager_get_instance();
		GraphWindow * gw = graph_window_get_instance ();
		const gchar * name = rel_get_name(rel);

		/* Show the relation and the associated graph if there is one. */
#warning NOTE: Does this cause a loop? RelationManager should call us again afterwards.
		relation_window_set_relation(relation_window_get_instance(), rel);
		if (xgraph_manager_exists (gm, name))
			graph_window_set_graph (gw, xgraph_manager_get_by_name(gm, name));
	}
}



static void 		_selected_prog_changed (GtkTreeSelection * sel,
										   gpointer user_data)
{
	g_warning ("_selected_prog_changed: NOT IMPLEMENTED!");
}

static void 		_selected_func_changed (GtkTreeSelection * sel,
										   gpointer user_data)
{
	g_warning ("_selected_func_changed: NOT IMPLEMENTED!");
}


void on_buttonNewRelDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));
	gchar * name;
	mpz_t cols, rows;

	mpz_init (cols);
	mpz_init (rows);

	name = rv_user_new_relation (self->rv, NULL, cols, rows);
	if (name) {
		Namespace * ns = rv_get_namespace(self->rv);
		RvObject * conflict = (RvObject*) namespace_get_by_name(ns, name);

		gboolean really_insert = TRUE;

		if (conflict) {
			RvReplaceAction action = rv_user_ask_replace_object2
					(self->rv, conflict, NULL, FALSE);
			if (action != RV_REPLACE_ACTION_REPLACE) {
				really_insert = FALSE;
			}
			else rv_object_destroy (conflict);
		}

		if (really_insert) {
			Rel * rel = rel_manager_create_rel (rv_get_rel_manager(self->rv),
					name, rows, cols);
			if (rel) {
				relation_window_set_relation (relation_window_get_instance(), rel);
				_dir_window_update_rels (self);
			}
		}

		g_free (name);
	}

	mpz_clear (cols);
	mpz_clear (rows);
}


void on_buttonDeleteRelDir_clicked (GtkButton * button, gpointer user_data)
{ _delete_selected_rel(_dir_window_get (GTK_WIDGET (button))); }

/*!
 * Used in \ref on_buttonClearRelsDir_clicked to filter the relations the
 * user want to delete.
 */
gboolean _filter_and_delete_graph_func (Rel * rel, gpointer user_data)
{
	DirWindow * self = (DirWindow*) user_data;
	gboolean show_hidden = _dir_window_get_show_hidden(self);
	const gchar * name = rel_get_name (rel);

	if ((show_hidden || !rel_is_hidden(rel)) && !g_str_equal(name, "$")) {
		xgraph_manager_delete_by_name (xgraph_manager_get_instance(), name);
		return TRUE;
	}
	else return FALSE;
}

void on_buttonClearRelsDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));

	/* Clears the relations which are currently visible. I.e. hidden ones are
	 * only deleted, if they are visible. */

	rel_manager_delete_with_filter (rv_get_rel_manager(self->rv),
			_filter_and_delete_graph_func, self);
	_dir_window_update_rels (self);
}


void on_buttonNewEditDomDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));
	GString * name = g_string_new ("");
	GString * first_comp = g_string_new ("");
	GString * second_comp = g_string_new ("");
	DomainType type = DIRECT_PRODUCT;
	DomManager * manager = rv_get_dom_manager (self->rv);
	Dom * sel = _dir_window_get_sel_dom (self);

	/* If a domain is selected, let the user edit it. */
	if (sel) {
		g_string_assign (name, dom_get_name(sel));
		g_string_assign (first_comp, dom_get_first_comp(sel));
		g_string_assign (second_comp, dom_get_second_comp(sel));
		type = dom_get_type (sel);
	}

	if (rv_user_edit_domain (self->rv, name, &type, first_comp, second_comp)) {
		/* If the user edited a domain and if he hasn't changed it's name, we
		 * won't ask to overwrite it. In all other cases, we will. */
		if (sel && g_str_equal(name->str, dom_get_name(sel))) {
			GError * err = NULL;
			Dom * dom = dom_new (name->str, type, first_comp->str, second_comp->str, &err);
			if (! dom) {
				rv_user_error_with_descr ("Unable to create domain",
						err->message,
						"See below for details.");
				g_error_free (err);
			}
			else {
				dom_move (sel, dom);
				_dir_window_update_doms(self);
			}
		}
		else {
			gboolean really_create = FALSE;
			Namespace * ns = dom_manager_get_namespace(manager);

			RvObject * conflict = (RvObject*)namespace_get_by_name (ns, name->str);

			if (conflict) {
				RvReplacePolicy policy = rv_user_ask_replace_object2 (self->rv,
						conflict, NULL, FALSE);
				RvReplaceAction action = RV_REPLACE_POLICY_TO_ACTION(policy);

				if (action == RV_REPLACE_ACTION_REPLACE) {
					rv_object_destroy(conflict);
					really_create = TRUE;
				}
			}
			else really_create = TRUE;

			if (really_create) {
				GError * err = NULL;
				Dom * dom = dom_new (name->str, type, first_comp->str, second_comp->str, &err);
				if (! dom) {
					rv_user_error_with_descr ("Unable to create domain",
							err->message,
							"See below for details.");
					g_error_free (err);
				}
				else dom_manager_insert (manager, dom);
			}
		}
	}

	g_string_free (name,TRUE);
	g_string_free (first_comp,TRUE);
	g_string_free (second_comp,TRUE);
}


void on_buttonDeleteDomDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));
	Dom * dom = _dir_window_get_sel_dom (self);
	if (dom) dom_destroy (dom);
}

/*!
 * Used in e.g. \ref on_buttonClearDomsDir_clicked to filter the domains the
 * user want to delete.
 */
gboolean _filter_dom_func (Dom * obj, gpointer user_data)
{
	DirWindow * self = (DirWindow*) user_data;
	gboolean show_hidden = _dir_window_get_show_hidden(self);

	return show_hidden || !dom_is_hidden(obj);
}

void on_buttonClearDomsDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));
	DomManager * manager = rv_get_dom_manager(self->rv);

	/* Clears the domains which are currently visible. I.e. hidden ones are
	 * only deleted, if they are visible. */
	dom_manager_delete_with_filter (manager, _filter_dom_func, self);
}


void on_menuitemEditFun_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Fun * fun = _dir_window_get_sel_fun (self);
	if (fun) {
		gboolean success;
		GString * def = g_string_new (fun_get_term (fun));

		success = rv_user_edit_function (self->rv, def);
		if (success) {
			GError * err = NULL;
			Fun * edited_fun = fun_new_from_def (def->str, &err);
			if (! edited_fun) {
				rv_user_error ("Invalid Function", "The definition \"%s\" is not "
						"a valid function. Reason: %s", err->message);
				g_error_free (err);
			}
			else {
				FunManager * manager = fun_manager_get_instance ();
				gboolean really_insert = TRUE;
				const gchar * name = fun_get_name (edited_fun);

				Namespace * ns = rv_get_namespace(self->rv);
				RvObject * conflict = namespace_get_by_name(ns, name);

				/* If the edited function has the same name as the original
				 * function, don't ask to overwrite. */
				if (! g_str_equal (fun_get_name(fun), name) && conflict) {
					RvReplaceAction action = rv_user_ask_replace_object2
							(self->rv, conflict, NULL, FALSE);
					if (action != RV_REPLACE_ACTION_REPLACE) {
						fun_destroy (edited_fun);
						really_insert = FALSE;
					}
					else {
						rv_object_destroy(conflict);
					}
				}
				else if (g_str_equal (fun_get_name(fun), name))
					fun_manager_delete_by_name (manager, name);

				if (really_insert)
					fun_manager_insert (manager, edited_fun);
			}
		}

		g_string_free(def, TRUE);
	}
}

void on_menuitemDeleteFun_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Fun * fun = _dir_window_get_sel_fun (self);
	if (fun) fun_destroy (fun);
}

void on_menuitemUnHideFun_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Fun * fun = _dir_window_get_sel_fun (self);
	if (fun) fun_set_hidden(fun, !fun_is_hidden(fun));
	_dir_window_update_functions (self);
}

void on_menuitemDefineFun_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	gboolean success;
	GString * def = g_string_new ("");

	success = rv_user_edit_function (self->rv, def);
	if (success) {
		GError * err = NULL;
		Fun * f = fun_new_from_def(def->str, &err);
		if (! f) {
			rv_user_error ("Invalid Function", "The definition \"%s\" is not "
					"a valid function. Reason: %s", err->message);
			g_error_free (err);
		}
		else {
			FunManager * manager = fun_manager_get_instance ();
			gboolean really_insert = TRUE;
			const gchar * name = fun_get_name (f);

			Namespace * ns = rv_get_namespace(self->rv);
			RvObject * conflict = namespace_get_by_name(ns, name);

			if (conflict) {
				RvReplaceAction action = rv_user_ask_replace_object2
						(self->rv, conflict, NULL, FALSE);
				if (action != RV_REPLACE_ACTION_REPLACE) {
					fun_destroy (f);
					really_insert = FALSE;
				}
				else rv_object_destroy (conflict);
			}

			if (really_insert)
				fun_manager_insert (manager, f);
		}
	}

	g_string_free(def, TRUE);
}


/*!
 * Used in \ref on_buttonClearFuncsRelDir_clicked to delete the visible
 * functions.
 */
gboolean _filter_fun_func (Fun * obj, gpointer user_data)
{
	DirWindow * self = (DirWindow*) user_data;
	gboolean show_hidden = _dir_window_get_show_hidden(self);

	return show_hidden || !fun_is_hidden(obj);
}

void on_buttonClearFuncsRelDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));

	/* Ask before we delete anything. */
	GtkMessageDialog * message = gtk_message_dialog_new
			(dir_window_get_widget(self), GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "Clear functions");
	gtk_message_dialog_format_secondary_text (message,
			"Do you really want to delete all visible functions?");
	if (gtk_dialog_run(GTK_DIALOG(message)) == GTK_RESPONSE_YES) {
		/* Clears the functions that are currently visible. I.e. hidden ones are
		 * only deleted, if they are visible. */

		fun_manager_delete_with_filter (rv_get_fun_manager(self->rv),
				_filter_fun_func, self);
		_dir_window_update_functions (self);
	}
	gtk_widget_destroy (GTK_WIDGET(message));
}


/*!
 * Used in \ref on_buttonClearProgsRelDir_clicked to delete the visible
 * functions.
 */
gboolean _filter_prog_func (Prog * obj, gpointer user_data)
{
	DirWindow * self = (DirWindow*) user_data;
	gboolean show_hidden = _dir_window_get_show_hidden(self);

	return show_hidden || !prog_is_hidden(obj);
}

void on_buttonClearProgsRelDir_clicked (GtkButton * button, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET (button));

	/* Ask before we delete anything. */
	GtkMessageDialog * message = gtk_message_dialog_new
			(dir_window_get_widget(self), GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "Clear programs");
	gtk_message_dialog_format_secondary_text (message,
			"Do you really want to delete all visible programs?");
	if (gtk_dialog_run(GTK_DIALOG(message)) == GTK_RESPONSE_YES) {
		/* Clears the programs that are currently visible. I.e. hidden ones are
		 * only deleted, if they are visible. */

		prog_manager_delete_with_filter (rv_get_prog_manager(self->rv),
				_filter_prog_func, self);
		_dir_window_update_programs (self);
	}
	gtk_widget_destroy (GTK_WIDGET(message));
}

gboolean on_treeviewFuncsDir_button_release_event (GtkWidget * widget,
		GdkEventButton * event, gpointer user_data)
{
	if (event->button == 3) {
		DirWindow * self = _dir_window_get (widget);
		Fun * fun = _dir_window_get_sel_fun (self);

		if ( fun) {
			gtk_menu_popup(GTK_MENU (self->popupFun), NULL, NULL, NULL, NULL,
									 event->button, event->time);
		}
		else {
			gtk_menu_popup(GTK_MENU (self->popupFunList), NULL, NULL, NULL, NULL,
									 event->button, event->time);
		}
	}
	return FALSE; /* Further propagation */
}

void on_menuitemDeleteProg_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Prog * prog = _dir_window_get_sel_prog (self);
	if (prog) prog_destroy (prog);
}

void on_menuitemUnHideProg_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Prog * prog = _dir_window_get_sel_prog (self);
	if (prog) prog_set_hidden(prog, !prog_is_hidden(prog));
	_dir_window_update_programs (self);
}

gboolean on_treeviewProgsDir_button_release_event (GtkWidget * widget,
		GdkEventButton * event, gpointer user_data)
{
	if (event->button == 3) {
		DirWindow * self = _dir_window_get (widget);
		Prog * prog = _dir_window_get_sel_prog (self);

		if ( prog) {
			gtk_menu_popup(GTK_MENU (self->popupProg), NULL, NULL, NULL, NULL,
									 event->button, event->time);
		}
	}
	return FALSE; /* Further propagation */
}

void on_menuitemDeleteRel_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	_delete_selected_rel(self);
}

void on_menuitemUnHideRel_activate (GtkMenuItem * menu_item, gpointer user_data)
{
	DirWindow * self = _dir_window_get (GTK_WIDGET(menu_item));
	Rel * rel = _dir_window_get_sel_rel (self);
	if (rel && g_str_equal(rel_get_name(rel), "$")) {
		rv_user_error ("Not allowed", "Changing the visibility of $ is not allowed.");
	}
	else if (rel) {
		rel_set_hidden (rel, !rel_is_hidden(rel));
		_dir_window_update_rels (self);
	}
}

gboolean on_treeviewRelsDir_button_release_event (GtkWidget * widget,
		GdkEventButton * event, gpointer user_data)
{
	if (event->button == 3) {
		DirWindow * self = _dir_window_get (widget);
		Rel * rel = _dir_window_get_sel_rel (self);

		if ( rel) {
			gtk_menu_popup(GTK_MENU (self->popupRel), NULL, NULL, NULL, NULL,
									 event->button, event->time);
		}
	}
	return FALSE; /* Further propagation */
}


