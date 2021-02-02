/*
 * OptionsDialog.c
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

#include "OptionsDialog.h"
#include <gtk/gtk.h>

#include "prefs.h"
#include "EvalWindow.h"
#include "DirWindow.h"
#include "random.h"
#include "history.h"

#include "RelationWindow.h" // redraw_rel
#include "Relation.h" /* struct Rel */

#include "Relview.h" // rv_user_error

#include "GraphWindow.h" /* for redraw after anti-aliasing has changed. */
#include "RelationViewport.h" /* relation_viewport_set_labels (for
                               * default labels) */
#include "Workspace.h"

#include <assert.h>

#define OBJECT_ATTACH_KEY "OptionsDialogObj"

typedef struct _OptionsDialog
{
	Relview * rv;
	GtkBuilder * builder;
} OptionsDialog;


static void _load_options (Relview * rv);
static void _save_options (Relview * rv);


OptionsDialog * _options_dialog_get (GtkWidget * widget)
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
		= g_object_get_data (G_OBJECT(widget), OBJECT_ATTACH_KEY);

	  if (pdata) {
		return (OptionsDialog*) pdata;
	  }

	  widget = gtk_widget_get_parent (widget);
	}
  }

  assert (FALSE);
  return NULL;
}

/*!
 * Show the options dialog window and allow the user to make changes.
 */
void show_options (Relview * rv)
{

	GtkBuilder * builder = rv_get_gtk_builder (rv);
	GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogOptions"));
	OptionsDialog * self;
	gint respId;

	self = g_new0 (OptionsDialog,1);
	self->rv = rv;
	self->builder = builder;

	/* Attach the window widget to the window. You can call _*_window_get
	 * to obtain the window from any sub widget. */
	g_object_set_data (G_OBJECT(dialog), OBJECT_ATTACH_KEY, (gpointer) self);

	_load_options (rv);

	respId = gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_hide (dialog);

	if (1 /*see Glade*/ == respId) {
		_save_options (rv);
	}

	g_free (self);
}


void _load_options(Relview * rv)
{
	GtkBuilder * builder = rv_get_gtk_builder(rv);
#define LOOKUP(name) GTK_WIDGET(gtk_builder_get_object (builder, #name))
	GtkAdjustment * adjust = NULL;
	gchar *text = NULL;

	/* history depth */
	adjust = gtk_range_get_adjustment(GTK_RANGE (LOOKUP(hscaleHistory)));
	gtk_adjustment_set_value(adjust, prefs_get_int("settings",
			"maxhistorylength", 50));
	gtk_adjustment_value_changed(adjust);

	/* directory-orientation */
	if (prefs_get_int("settings", "xrvprog-orientation", 0))
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP (rbOrientationHorizontal)), TRUE);
	else
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP (rbOrientationVertical)), TRUE);

	/* pseudo random numbers */
	if (prefs_get_int("settings", "pseudorandom", TRUE))
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP(rbPseudoRandom)), TRUE);
	else
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP(rbPseudoRandom_No)), TRUE);

	/* Set sensitivity of the seed entry and button. */
	gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON (LOOKUP(rbPseudoRandom)));

	text = prefs_get_string("settings", "randomseed");
	if (NULL == text)
		text = g_strdup("");
	gtk_entry_set_text(GTK_ENTRY (LOOKUP(entrySeed)), text);
	g_free(text);

	/* ring bell */
	if (prefs_get_int("settings", "ring_bell", TRUE))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (LOOKUP (rbBellRing)),
				TRUE);
	else
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP (rbBellRing_No)), TRUE);

	/* ask for confirmation */
	if (prefs_get_int("settings", "ask_for_confirmation", TRUE))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (LOOKUP (rbAskConfirm)),
				TRUE);
	else
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON (LOOKUP (rbAskConfirm_No)), TRUE);

	text = prefs_get_string("settings", "relation_display_limit");
	if (NULL == text)
		text = g_strdup("10000");
	gtk_entry_set_text(GTK_ENTRY (LOOKUP(entryRelationDisplayLimit)), text);
	g_free(text);

	text = prefs_get_string("settings", "graphNode_display_limit");
	if (NULL == text)
		text = g_strdup("100");
	gtk_entry_set_text(GTK_ENTRY (LOOKUP(entryGraphNodeDisplayLimit)), text);
	g_free(text);

	/* use anti-aliasing */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON (LOOKUP (cbUseAntiAliasing)), prefs_get_int(
					"settings", "use_anti_aliasing", FALSE));

	/* use default labels */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON (LOOKUP (cbUseDefaultLabels)), prefs_get_int(
					"settings", "use_default_labels", FALSE));

	/* disable windows */
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON (LOOKUP (cbDisableWindow)), prefs_get_int(
					"settings", "disable_windows_on_limit", FALSE));
#undef LOOKUP
}

void _save_options(Relview * rv)
{
	GtkBuilder * builder = rv_get_gtk_builder(rv);
	GtkToggleButton *rbOrientationHorz = NULL;
	const gchar *text = NULL;
	gboolean orientation_changed = FALSE;

	/* save options to preferences */

#define LOOKUP(name) GTK_WIDGET(gtk_builder_get_object (builder, (name)))

	/* history depth */
	prefs_set_int("settings", "maxhistorylength",
			(int) (gtk_range_get_adjustment(
					GTK_RANGE (LOOKUP ("hscaleHistory")))->value));

	/* maybe the current history lists must be cut off */
#warning TODO: macht die Anweisung hier einen Sinn??
	//check_eval_history_length ();
	{
		History history;

		history_new_from_glist(&history, prefs_get_int("settings",
				"maxhistorylength", 15), prefs_restore_glist("markhistory"));
		prefs_store_glist("markhistory", history_get_glist(&history));
		history_free(&history);
	}

	/* xrv/prog-orientation */
	rbOrientationHorz = GTK_TOGGLE_BUTTON (LOOKUP ("rbOrientationHorizontal"));
	orientation_changed = (prefs_get_int("settings", "xrvprog-orientation", 0)
			!= gtk_toggle_button_get_active(rbOrientationHorz));

	prefs_set_int("settings", "xrvprog-orientation",
			gtk_toggle_button_get_active(rbOrientationHorz) ? 1 : 0);
	/* This is not necessary anymore. The DirWindow will update itself. */
	//if (orientation_changed)
	//recreate_dirwindow();

	/* pseudo random numbers */
	prefs_set_int("settings", "pseudorandom", gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON (LOOKUP
					("rbPseudoRandom"))));

	/* random seed */
	text = gtk_entry_get_text(GTK_ENTRY (LOOKUP ("entrySeed")));
	prefs_set_string("settings", "randomseed", text);
	_srandom(atoi(text));

	/* ring bell */
	prefs_set_int("settings", "ring_bell", gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON (LOOKUP ("rbBellRing"))));

	/* ask for confirmation */
	prefs_set_int("settings", "ask_for_confirmation",
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON (LOOKUP ("rbAskConfirm"))));

	/* relation display limit */
	text = gtk_entry_get_text(GTK_ENTRY (LOOKUP ("entryRelationDisplayLimit")));
	prefs_set_string("settings", "relation_display_limit", text);

	redraw_rel(relation_window_get_instance());

	/* GraphNode display limit */
	text = gtk_entry_get_text(GTK_ENTRY (LOOKUP ("entryGraphNodeDisplayLimit")));
	prefs_set_string("settings", "graphNode_display_limit", text);

	// Redraw the GraphWindow
	{
		GraphWindow * gw = graph_window_get_instance();
		graph_window_redraw(gw, NEW);
	}

	/* use anti-aliasing */
	prefs_set_int("settings", "use_anti_aliasing",
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON (LOOKUP ("cbUseAntiAliasing"))));
	/* Redraw the GraphWindow */
	{
		GraphWindow * gw = graph_window_get_instance();
		graph_window_redraw(gw, NEW);
	}

	/* use default-labels */
	prefs_set_int("settings", "use_default_labels",
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON (LOOKUP ("cbUseDefaultLabels"))));
	{
		RelationWindow * rw = relation_window_get_instance();
		RelationViewport * rv = relation_window_get_viewport(rw);
		relation_viewport_configure(rv);
		relation_viewport_redraw(rv);
		relation_viewport_invalidate_rect(rv, NULL);
	}

	prefs_set_int("settings", "disable_windows_on_limit",
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON (LOOKUP ("cbDisableWindow"))));
#undef LOOKUP
}



/*!
 * Enable/Disable the controls to change the random seed.
 */
void on_rbPseudoRandom_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean disableSeed = gtk_toggle_button_get_active (togglebutton);
	OptionsDialog * self = _options_dialog_get(GTK_WIDGET(togglebutton));
	GtkWidget *seedLabel, *seedButton, *seedEntry;

	seedLabel = GTK_WIDGET(gtk_builder_get_object (self->builder, "labelSeed"));
	seedButton
			= GTK_WIDGET(gtk_builder_get_object (self->builder, "buttonSeed"));
	seedEntry = GTK_WIDGET(gtk_builder_get_object (self->builder, "entrySeed"));

	gtk_widget_set_sensitive (seedLabel, disableSeed);
	gtk_widget_set_sensitive (seedEntry, disableSeed);
	gtk_widget_set_sensitive (seedButton, disableSeed);
}

void on_buttonSeed_clicked(GtkButton *button, gpointer user_data)
{
	OptionsDialog * self = _options_dialog_get(GTK_WIDGET(button));
	GtkEntry * entry = GTK_ENTRY (gtk_builder_get_object(self->builder, "entrySeed"));
	gchar * seed = g_strdup_printf("%ld", random());

	gtk_entry_set_text(entry, seed);
	g_free(seed);
}

void on_buttonSaveLayout_clicked (GtkButton *button, gpointer user_data)
{
	workspace_save(rv_get_workspace(rv_get_instance()));
	rv_user_message ("Success", "Current window layout has been saved.");
}


// Callback for the font selection dialog.
static void on_fontsel_ok_clicked(GtkButton * button, gpointer user_data)
{
  GtkFontSelectionDialog *dialog= GTK_FONT_SELECTION_DIALOG (user_data);
  gchar * fontname = gtk_font_selection_dialog_get_font_name(dialog);

  if (!fontname) {
	  rv_user_error ("Don't know what to do", "Unable to obtain the selected "
			  "font name. The default font has not been changed.");
  }
  else {

    GtkWidget * msgDialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
    "The default font has been changed. Restart Relview to "
    "apply this change.");
    gtk_window_set_position(GTK_WINDOW(msgDialog), GTK_WIN_POS_CENTER);
    gtk_dialog_run(GTK_DIALOG (msgDialog));
    gtk_widget_destroy(msgDialog);

    prefs_set_string("fonts", "default", fontname);
  }

  g_free(fontname);
  gtk_widget_destroy(GTK_WIDGET (dialog));
}

void on_buttonSetFont_clicked(GtkButton *button, gpointer user_data)
{
  GtkWidget * dialog;
  gchar * font = prefs_get_string("fonts", "default");

  dialog = gtk_font_selection_dialog_new("Choose font");
  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG (dialog), font);
  gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG (dialog),
  "The quick brown fox jumps over the lazy dog.");

  gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_modal(GTK_WINDOW (dialog), TRUE);

  g_signal_connect(dialog, "destroy", G_CALLBACK(gtk_widget_destroyed), &dialog);
  g_signal_connect(GTK_FONT_SELECTION_DIALOG (dialog)->ok_button,
  "clicked", G_CALLBACK (on_fontsel_ok_clicked),
  dialog);
  g_signal_connect_swapped(GTK_FONT_SELECTION_DIALOG (dialog)->cancel_button,
  "clicked", G_CALLBACK (gtk_widget_destroy),
  dialog);

  gtk_widget_show(dialog);

  g_free(font);
}
