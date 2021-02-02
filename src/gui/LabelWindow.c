/*
 * LabelWindow.c
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


/******************************************************************************/
/* LABELWINDOW                                                                */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : shows the label directory                                       */
/*  CREATED : 1995 Peter Schneider (PS)                                       */
/* MODIFIED : 04-JUL-2000 Werner Lehmann (WL)                                 */
/*                        - GTK+ port                                         */
/*                        - moved all relevant parts from xv_x.c into this    */
/*                          module)                                           */
/*                        - slightly new dialog design                        */
/******************************************************************************/

#include "label.h"
#include "msg_boxes.h"
#include "utilities.h"
#include "Relview.h"
#include "LabelWindow.h"

#define LABEL_WINDOW_OBJECT_ATTACH_KEY "LabelWindowObj"

static LabelWindow * 	_label_window_get (GtkWidget * widget);
static void 			_show_label(LabelWindow * self, Label * label);
static void 			_show_label_list(LabelWindow * self);
static void 			_on_sel_changed (GtkTreeSelection * sel, gpointer user_data);

struct _LabelWindow
{
	Relview * rv;
	GtkWidget * window;

	GtkListStore * liststoreLabels;
	GtkListStore * liststoreItems;
};

LabelWindow * g_labelWindow = NULL;


LabelWindow * _label_window_get (GtkWidget * widget)
{
  while (widget) {
	if (GTK_IS_MENU (widget)) {
	  widget = gtk_menu_get_attach_widget (GTK_MENU(widget));
	  if (! widget) {
		fprintf (stderr, "The clicked menu doesn't have an attached widget.\n");
		assert (widget != NULL);
	  }
	}
	else {
	  gpointer * pdata
		= g_object_get_data (G_OBJECT(widget), LABEL_WINDOW_OBJECT_ATTACH_KEY);

	  if (pdata) {
		return (LabelWindow*) pdata;
	  }

	  widget = gtk_widget_get_parent (widget);
	}
  }

  assert (FALSE);
  return NULL;
}


LabelWindow * label_window_get_instance ()
{
	if ( !g_labelWindow) {
		g_labelWindow = label_window_new (rv_get_instance());
	}
	return g_labelWindow;
}


LabelWindow * label_window_new (Relview * rv)
{
	Workspace * workspace = rv_get_workspace(rv);
	GtkBuilder * builder = rv_get_gtk_builder(rv);
	LabelWindow * self = g_new0 (LabelWindow, 1);
	GtkTreeView * tvLabels;

	self->rv = rv;
	self->window = GTK_WIDGET(gtk_builder_get_object(builder, "windowLabels"));

	self->liststoreLabels = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststoreLabelsLD"));
	self->liststoreItems  = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststoreItemsLD"));
	tvLabels = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeviewLabelsLD"));

	/* Attach the window widget to the window. You can call _file_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(self->window), LABEL_WINDOW_OBJECT_ATTACH_KEY,
	                   (gpointer) self);

	g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(tvLabels)),
			"changed", G_CALLBACK(_on_sel_changed), self);

	/* Add the window to the workspace. Also restores position and visibility. */
	workspace_add_window (workspace, GTK_WINDOW(self->window), "labels-window");

	if (workspace_is_window_visible_by_default (workspace, GTK_WINDOW(self->window)))
		label_window_show (self);

	return self;
}


void label_window_show (LabelWindow * self)
{
	gtk_widget_show (self->window);
	_show_label_list (self);
}


void label_window_hide (LabelWindow * self)
{
	gtk_widget_hide (self->window);
}


gboolean label_window_is_visible (LabelWindow * self)
{
#if GTK_MINOR_VERSION >= 18
	return gtk_widget_get_visible (self->window);
#else
	return GTK_WIDGET_VISIBLE(self->window);
#endif
}


GtkWidget * label_window_get_widget (LabelWindow * self) { return self->window; }


/*!
 * Reloads the labels from the global manager. Should be replaced by a observer
 * in the future.
 */
void label_window_update (LabelWindow * self)
{
	_show_label_list(self);
}


/*! Display some assignments "(<number> : <current label>)".
 *
 * @author {stb}
 * @date {07-MAR-2008}
 * @param label The label to enumerate.
 */
void _show_label (LabelWindow * self, Label * label)
{
	gtk_list_store_clear(self->liststoreItems);

	if ( !label) return;
	else {
		GString * s = g_string_new ("");
		LabelIter * iter = label_iterator (label);
		int n = 20, i;

		label_iter_seek(iter, 1 /*first label*/);

		for (i = 0 ; i <= n && label_iter_is_valid(iter); i ++) {
			GtkTreeIter k;

			gchar * a = g_strdup_printf("%d : %s", label_iter_number(iter),
					label_iter_name(iter, s));
			gtk_list_store_append(self->liststoreItems, &k);
			gtk_list_store_set(self->liststoreItems, &k, 0, a, -1);
			g_free (a);
			label_iter_next (iter);
		}

		g_string_free (s, TRUE);
	}
}


/*!
 * Show the list of labels on the left and the assignments of the first such
 * label on the right.
 */
void _show_label_list(LabelWindow * self)
{
	if (!label_window_is_visible(self))
		return;
	else {
		GList/*<Label*>*/ * labels = rv_label_list (self->rv), *iter = NULL;

		/* Update the list of labels on the left. */
		gtk_list_store_clear(self->liststoreLabels);

		for ( iter = labels ; iter ; iter = iter->next) {
			GtkTreeIter k = {0};

			gtk_list_store_append (self->liststoreLabels, &k);
			gtk_list_store_set (self->liststoreLabels, &k,
					0, label_get_name((Label*)iter->data),
					-1);
		}

		/* On default show the first label's assignment on the right. */
		if (labels)
			_show_label (self, (Label*)labels->data);
		else /* There is no label */
			gtk_list_store_clear(self->liststoreItems);

		g_list_free (labels);
	}
}


/*!
 * Callback for the treeview's selection of the labels. Is called when the
 * user changes the selection.
 */
void _on_sel_changed (GtkTreeSelection * sel, gpointer user_data)
{
	LabelWindow * self = (LabelWindow*) user_data;
	GtkTreeIter iter;
	GtkTreeModel * model = NULL;

	gtk_list_store_clear (self->liststoreItems);

	if ( !gtk_tree_selection_get_selected(sel, &model, &iter)) {
		/* Nothing selected. Just clear the items. */
	}
	else {
		const gchar * name = NULL;
		Label * label = NULL;

		gtk_tree_model_get(model, &iter, 0, &name, -1);
		label = rv_label_get_by_name (self->rv, name);

		if (label)
			_show_label (self, label);
	}
}


/******************************************************************************/
/*       NAME : label_window_destroy                                          */
/*    PURPOSE : frees all alocated resources                                  */
/*    CREATED : 04-JUL-2000 WL                                                */
/******************************************************************************/
void label_window_destroy (LabelWindow * self)
{
}

/*!
 * Callback if the user pressed the "Clear" button to clear the list of labels.
 */
void on_buttonClearLD_clicked(GtkButton * button, gpointer user_data)
{
	LabelWindow * self = _label_window_get (GTK_WIDGET(button));

	if (error_and_wait(error_msg[label_del_all]) == NOTICE_YES) {
		rv_label_clear (self->rv);
		label_window_update (self);
	}
}


/*!
 * The user pressed the "Close" button. Hide the window.
 */
void on_buttonCloseLD_clicked (GtkButton *button, gpointer user_data)
{
	LabelWindow * self = _label_window_get (GTK_WIDGET(button));
	label_window_hide (self);
}
