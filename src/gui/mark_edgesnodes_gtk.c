
/****************************************************************************/
/* MARK_EDGESNODES                                                          */
/* ------------------------------------------------------------------------ */
/*  PURPOSE : dialog for graph context menuitems mark edges/nodes           */
/*  CREATED : 20-JUN-2000 Werner Lehmann (WL)                               */
/* MODIFIED : 20-NOV-2000 WL:                                               */
/*            - new clearable history combobox                              */
/****************************************************************************/

#include "mark_edgesnodes_gtk.h"

#include <gtk/gtk.h>
#include <string.h>
#include <assert.h>

#include "prefs.h"
#include "utilities.h"

#include "history.h"


/****************************************************************************/
/*       NAME : on_dialogMarkGraph_cbHistory_changed                        */
/*    PURPOSE : Callback: a history combobox item has been selected => fill */
/*              entriy accordingly.                                         */
/*    CREATED : 20-NOV-2000 WL                                              */
/****************************************************************************/
void
on_dialogMarkGraph_cbHistory_changed (GtkComboBox * cb, gpointer user_data)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
  GtkWidget * entryTerm = GTK_WIDGET(gtk_builder_get_object(builder, "entryMarkGraphTerm"));
  gchar * term = gtk_combo_box_get_active_text (cb);

  if (term != NULL) {
    gtk_entry_set_text (GTK_ENTRY (entryTerm), term);
    g_free (term);
  }
}


/****************************************************************************/
/*       NAME : on_buttonClearMN_clicked                                    */
/*    PURPOSE : Callback: Clear button has been clicked                     */
/* PARAMETERS : standard GTK+                                               */
/*    CREATED : 20-NOV-2000 WL                                              */
/****************************************************************************/
void
on_dialogMarkGraph_buttonClear_clicked (GtkButton * button, gpointer user_data)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
  GtkWidget * cbHistory = GTK_WIDGET(gtk_builder_get_object(builder, "cbMarkGraphHistory"));
  GtkListStore * model
    = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (cbHistory)));

  gtk_combo_box_set_active (GTK_COMBO_BOX (cbHistory), -1);
  gtk_list_store_clear (model);

  /* store history to prefs (the cleared list may be 'uncleared' otherwise
     if the dialog is closed with cancel) */
  prefs_store_glist ("markhistory", NULL);
}



static void addToComboBox (gpointer * str, gpointer * cb)
{
  gtk_combo_box_append_text ((GtkComboBox*) cb, (const gchar*) str);
}

static void fillComboBox (GtkComboBox *cb, const GList * items,
                          gint selectedNo)
{
  g_list_foreach (items, (GFunc) addToComboBox, (gpointer) cb);
  gtk_combo_box_set_active (cb, selectedNo);
}


/****************************************************************************/
/*       NAME : show_mark_edgesnodes                                        */
/*    PURPOSE : Creates and shows the mark edges / mark nodes dialog.       */
/* PARAMETERS : * caption is used as dialog caption, * value is default and */
/*              return value of the term, maxlen is its maximum length and  */
/*              level is default and return value for the mark level        */
/*              (0 == first, 1 == second).                                  */
/*    CREATED : 20-JUN-2000 WL                                              */
/****************************************************************************/


gboolean
show_mark_edgesnodes (const GString * caption, const GString * graphname,
                       GString * /*inout*/ value, int maxlen,
                       RvMarkLevel * /*inout*/ plevel)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
  GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogMarkGraph"));
  GtkWidget * entryTerm = GTK_WIDGET(gtk_builder_get_object(builder, "entryMarkGraphTerm")),
    * rbMarkLevelFirst = GTK_WIDGET(gtk_builder_get_object(builder, "rbMarkGraphLevel")),
    * cbHistory = GTK_WIDGET(gtk_builder_get_object(builder, "cbMarkGraphHistory")),
    * labelGraphName = GTK_WIDGET(gtk_builder_get_object(builder, "labelMarkGraphName")),
    * buttonClear = GTK_WIDGET(gtk_builder_get_object(builder, "buttonClearMarkGraphHistory"));
  gboolean ret = FALSE;

  gtk_window_set_title (GTK_WINDOW (dialog), caption->str);

  gtk_label_set_text (GTK_LABEL (labelGraphName), graphname->str);
  
  gtk_entry_set_max_length (GTK_ENTRY (entryTerm), maxlen);
  gtk_entry_set_text (GTK_ENTRY (entryTerm), value->str);

  /* We have to clear the combo box before we can reuse it. This is because
   * in contrast to earlier version, we now don't destroy and recreate our
   * widgets. */
  gtk_list_store_clear (GTK_LIST_STORE(gtk_combo_box_get_model(cbHistory)));

  {
      History history;

      history_new_from_glist
        (&history, prefs_get_int ("settings", "maxhistorylength", 15),
         prefs_restore_glist ("markhistory"));
      
      if (! history_is_empty (&history)) {
        fillComboBox (GTK_COMBO_BOX (cbHistory),
                      history_get_glist (&history), -1);
      }

      history_free (&history); 
  }

  if (FIRST == *plevel) {
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (rbMarkLevelFirst), TRUE);
  } 
  else if (SECOND == *plevel) {
    gtk_toggle_button_set_active
      (GTK_TOGGLE_BUTTON (rbMarkLevelFirst), FALSE);
  } else { assert (*plevel == FIRST || *plevel == SECOND); }

  
  /* run the dialog and store the term and the selected mark level into 
   * the corresponding output variables */
  {
    gint resp = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide (dialog);
    
    switch (resp) {
    case 1 /* see Glade */:
      g_string_assign (value, gtk_entry_get_text (GTK_ENTRY (entryTerm)));
      *plevel = gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON (rbMarkLevelFirst)) ? FIRST : SECOND;
      ret = TRUE;
      break;
    default:
      ret = FALSE;
    }
  }
  
  /* update the history. We need to reread it, since it could
   * become cleared by the user. */
  {
    History history;
    
    history_new_from_glist
      (&history, prefs_get_int ("settings", "maxhistorylength", 50),
       prefs_restore_glist ("markhistory"));

    history_prepend (&history, value->str);
    prefs_store_glist ("markhistory", history_get_glist (&history));

    history_free (&history); 
  }

  return ret;
}

