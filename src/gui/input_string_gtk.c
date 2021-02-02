
/******************************************************************************/
/* INPUT_STRING                                                               */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : dialog to have the user enter a string                          */
/*  CREATED : 2000.06.19 Werner Lehmann (WL)                                  */
/******************************************************************************/

#include "Relview.h" // rv_get_gtk_builder
#include "prefs.h"
#include "utilities.h"

#include <assert.h>


/******************************************************************************/
/*       NAME : show_input_string                                             */
/*    PURPOSE : Creates and shows a dialog the user can enter a string with.  */
/* PARAMETERS : *caption is used as dialog caption, *desc is used to fill the */
/*              descriptive label above the entry and *value is set as        */
/*              default value and contains the entered string if the function */
/*              returns TRUE (==the user clicked ok).                         */
/*              maxlen is the maximum input string length.                    */
/*    CREATED : 19-JUN-2000 WL                                                */
/******************************************************************************/

gboolean
show_input_string (const GString *caption, const GString *desc,
                   GString /*inout*/ *value, int maxlen)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
  GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogInputString"));
  GtkWidget * entry = GTK_WIDGET(gtk_builder_get_object(builder, "entryInput")),
    * label = GTK_WIDGET(gtk_builder_get_object(builder, "labelDescr"));
  gboolean ret = FALSE;

  assert (maxlen > 0);

  gtk_window_set_title (GTK_WINDOW (dialog), caption->str);

  gtk_entry_set_max_length (GTK_ENTRY (entry), maxlen);
  gtk_entry_set_text (GTK_ENTRY (entry), value->str);
  
  gtk_label_set_text (GTK_LABEL (label), desc->str);

  /* Reset the focus to the text entry for the graph name. */
  gtk_widget_grab_focus (entry);
  
  {
    gint resp = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide (dialog);
    
    switch (resp) {
    case 1 /* see Glade */:
      g_string_assign (value, gtk_entry_get_text (GTK_ENTRY (entry)));
      ret = TRUE;
      break;
    default:
      ret = FALSE;
    }
  }
  
  return ret;
}
