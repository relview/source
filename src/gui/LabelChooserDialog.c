#include "Relview.h"
#include <string.h>
#include <assert.h>
#include "Relation.h"
#include "LabelChooserDialog.h"

/*!
 * Converts the global list of available labels to a format appropriate
 * to use it with the GTK+ ComboBox. Additionally the special label
 * "(none)" was added here for convenience.
 *
 * @return Returns the newly created list.
 */
static GList/*<gchar*>*/ * _prepareLabelList (Relview * rv)
{
	GList/*<gchar*>*/ * s = NULL;
	GList/*<Label*>*/ * labels = rv_label_list (rv), *iter = NULL;

	s = g_list_append (s, g_strdup("(none)"));

	for ( iter = labels ; iter ; iter = iter->next)
		s = g_list_prepend (s, g_strdup (label_get_name((Label*)iter->data)));

	s = g_list_reverse (s);

	return s;
}


static void _addToComboBox (gpointer str, gpointer cb)
{
  gtk_combo_box_append_text ((GtkComboBox*) cb, (const gchar*) str);
}


static void _fillComboBox (GtkComboBox *cb, GList/*<gchar*>*/ * items,
                           gint selectedNo)
{
	/* First free the combo box. This is necessary, because each call uses the
	 * same widget and list mode. */
	gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model (cb)));

	g_list_foreach (items, _addToComboBox, cb);
	gtk_combo_box_set_active (cb, selectedNo);
}


static gint _selected_label_index (GList/*<gchar*>*/ * items, const gchar * labelName)
{
  GList * iter = g_list_find_custom (items, (gconstpointer) labelName,
                                     (GCompareFunc) strcmp);

  if (iter == NULL) {
    /* maybe a used label was deleted from the list of available labels. */
    assert (g_list_length (items) > 0);
    return 0;
  }
  
  return g_list_position (items, iter);
}


/*! Determines wheter the user selected no labeling at all.
 *
 * @return Returns TRUE if the user doesn't want labeling, FALSE otherwise.
 */
gboolean label_chooser_isNoneSelected (GString * sel)
{ return 0 == strncmp (sel->str, "(none)", 6); }



/*! Determines wheter the user selected a custom labeling.
 *
 * @return Returns TRUE if the user wants custom labeling, FALSE otherwise.
 */
gboolean label_chooser_isLabelSelected (GString * sel)
{
  return ! label_chooser_isNoneSelected (sel);
}


/*!
 * Similar to \ref label_chooser_dialog, but the current label names are given
 * in the arguments.
 */
gboolean
label_chooser_dialog_preset (GString * /*inout*/ rowLabelName,
                              GString * /*inout*/ colLabelName)
{
	Relview * rv = rv_get_instance ();
	GtkBuilder * builder = rv_get_gtk_builder (rv);
	GtkWidget * dialog 		= GTK_WIDGET(gtk_builder_get_object(builder, "dialogLabelChooser"));
	GtkWidget * cbRowLabels = GTK_WIDGET(gtk_builder_get_object(builder, "cbRowLabels")),
			* cbColLabels 	= GTK_WIDGET(gtk_builder_get_object(builder, "cbColLabels"));
	gboolean ret = FALSE;

	/* set the selection to the labels currently in use, or to (none) */
	{
		GList/*<gchar*>*/ *names = _prepareLabelList (rv);

		if (g_str_equal (rowLabelName->str, "")) g_string_assign (rowLabelName, "(none)");
		if (g_str_equal (colLabelName->str, "")) g_string_assign (colLabelName, "(none)");

		_fillComboBox (GTK_COMBO_BOX (cbRowLabels), names,
				_selected_label_index (names, rowLabelName->str));
		_fillComboBox (GTK_COMBO_BOX (cbColLabels), names,
				_selected_label_index (names, colLabelName->str));

		g_list_foreach (names, (GFunc)g_free, NULL);
		g_list_free (names);
	}

	/* Run the dialog. */
	{
		gint resp = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_hide (dialog);

		switch (resp) {
		case 1 /* see Glade */:
		{
			gchar * rowStr, *colStr;

			rowStr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (cbRowLabels));
			colStr = gtk_combo_box_get_active_text (GTK_COMBO_BOX (cbColLabels));

			g_string_assign (rowLabelName, rowStr);
			g_string_assign (colLabelName, colStr);

			g_free (rowStr);
			g_free (colStr);

			ret = TRUE;
			break;
		}
		default:
			ret = FALSE;
		}
	}

	return ret;
}
