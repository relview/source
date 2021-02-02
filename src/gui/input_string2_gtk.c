
/******************************************************************************/
/* INPUT_STRING2                                                              */
/* -------------------------------------------------------------------------- */
/*  PURPOSE : dialog to have the user enter two strings                       */
/*  CREATED : 2000.06.19 Werner Lehmann (WL)                                  */
/******************************************************************************/

#include <gtk/gtk.h>
#include <assert.h>

#include "Relview.h"
#include "prefs.h"
#include "utilities.h"

#if 0
// NOTE: If you need this part, migrate to GtkBuilder first. Use "dialogInputString2".
//       As an example consider "show_input_string".

/******************************************************************************/
/*       NAME : show_input_string2                                            */
/*    PURPOSE : Creates and shows a dialog the user can enter a string with.  */
/* PARAMETERS : *caption is used as dialog caption, *desc1/2 is used to fill  */
/*              the descriptive label above entry1/2 and *value1/2 is set as  */
/*              default value and contains the entered string if the function */
/*              returns TRUE (==the user clicked ok).                         */
/*              maxlen1/2 is the maximum input string length.                 */
/*    CREATED : 20-JUN-2000 WL                                                */
/******************************************************************************/

gboolean
show_input_string2 (const GString *caption,
                    const GString *desc1, GString *value1, int maxlen1,
                    const GString *desc2, GString *value2, int maxlen2)
{
  GtkWidget * dialog = create_dialogInputString2 ();
  GtkWidget * entry1 = lookup_widget (dialog, "entryInput1"),
    * label1 = lookup_widget (dialog, "labelDescr1"),
    * entry2 = lookup_widget (dialog, "entryInput2"),
    * label2 = lookup_widget (dialog, "labelDescr2");
  gboolean ret = FALSE;

  assert (maxlen1 > 0 && maxlen2 > 0);

  gtk_window_set_title (GTK_WINDOW (dialog), caption->str);

  gtk_entry_set_max_length (GTK_ENTRY (entry1), maxlen1);
  gtk_entry_set_max_length (GTK_ENTRY (entry2), maxlen2);
  gtk_entry_set_text (GTK_ENTRY (entry1), value1->str);
  gtk_entry_set_text (GTK_ENTRY (entry2), value2->str);
  
  gtk_label_set_text (GTK_LABEL (label1), desc1->str);
  gtk_label_set_text (GTK_LABEL (label2), desc2->str);
  
  {
    gint resp = gtk_dialog_run (GTK_DIALOG (dialog));
    
    switch (resp) {
    case 1 /* see Glade */:
      g_string_assign (value1, gtk_entry_get_text (GTK_ENTRY (entry1)));
      g_string_assign (value2, gtk_entry_get_text (GTK_ENTRY (entry2)));
      ret = TRUE;
      break;
    default:
      ret = FALSE;
    }
  }
  
  gtk_widget_destroy (dialog);
  
  return ret;
}
#endif
