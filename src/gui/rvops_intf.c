#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "utilities.h"
#include "rvops_gtk.h"
#include "multiopwindow_gtk.h"
//#include "rv_gtk.h"

#define LABEL_PADDING 10


/****************************************************************************/
/*       NAME : add_label                                                   */
/*    PURPOSE : Create a label, attach it to the table, and set all         */
/*              important properties.                                       */
/*    CREATED : 06-JUL-2000 WL                                              */
/****************************************************************************/
static void add_label (const gchar * str, GtkWidget * table,
                       guint left_attach, guint right_attach,
                       guint top_attach, guint bottom_attach)
{
  GtkWidget * label;

  label = gtk_label_new (str);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label,
                    left_attach, right_attach, top_attach, bottom_attach,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_misc_set_padding (GTK_MISC (label), LABEL_PADDING, 0);
}

/****************************************************************************/
/*       NAME : add_button                                                  */
/*    PURPOSE : Create a button, attach it to the table, and set all        */
/*              important properties.                                       */
/*    CREATED : 06-JUL-2000 WL                                              */
/****************************************************************************/
static GtkWidget * add_button_data (const gchar * str, GtkWidget * table,
                                    guint left_attach, guint right_attach,
                                    guint top_attach, guint bottom_attach,
                                    GtkSignalFunc func, gint tag,
                                    gpointer data)
{
  GtkWidget * button;

  button = gtk_button_new_with_label (str);
  gtk_widget_show (button);
  gtk_table_attach (GTK_TABLE (table), button,
                    left_attach, right_attach, top_attach, bottom_attach,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                      GTK_SIGNAL_FUNC (func),
                      GINT_TO_POINTER (tag));
  if (data)
    gtk_object_set_data (GTK_OBJECT (button), "data", data);

  return button;
}

static GtkWidget * add_button (const gchar * str, GtkWidget * table,
                               guint left_attach, guint right_attach,
                               guint top_attach, guint bottom_attach,
                               GtkSignalFunc func, gint tag)
{
  return add_button_data (str, table, left_attach, right_attach, top_attach,
                          bottom_attach, func, tag, NULL);
}

/****************************************************************************/
/*       NAME : create_windowRelviewOps                                     */
/*    PURPOSE : Create the relview operations window                        */
/*    CREATED : 19-DEC-2000 WL                                              */
/****************************************************************************/
GtkWidget * create_windowRelviewOps ()
{
  GtkWidget * windowRelviewOps;
  GtkWidget * tableRelview;

  /* create all widgets */

  windowRelviewOps = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (windowRelviewOps),
                       "windowRelviewOps", windowRelviewOps);
  gtk_window_set_title (GTK_WINDOW (windowRelviewOps), "Relview Operations");
  gtk_widget_set_name (windowRelviewOps, "windowRelviewOps");

  tableRelview = gtk_table_new (14, 5, TRUE);
  gtk_widget_show (tableRelview);
  gtk_container_add (GTK_CONTAINER (windowRelviewOps), tableRelview);
  gtk_container_set_border_width (GTK_CONTAINER (tableRelview), 2);
  gtk_table_set_row_spacings (GTK_TABLE (tableRelview), 2);
  gtk_table_set_col_spacings (GTK_TABLE (tableRelview), 2);

  /* labels */

  add_label ("Basic Operations:", tableRelview, 0, 5, 0, 1);
  add_label ("Residuals and quotients:", tableRelview, 0, 5, 2, 3);
  add_label ("Closures:", tableRelview, 0, 5, 4, 5);
  add_label ("Domains:", tableRelview, 0, 5, 6, 7);
  add_label ("Direct product (a*b):", tableRelview, 0, 5, 8, 9);
  add_label ("Direct sum (a+b):", tableRelview, 0, 5, 10, 11);
  add_label ("Powersets 2^a:", tableRelview, 0, 5, 12, 13);

  /* buttons */

  add_button_data ("|", tableRelview, 0, 1, 1, 2, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & basicop_info);
  add_button_data ("&", tableRelview, 1, 2, 1, 2, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & basicop_info);
  add_button_data ("-", tableRelview, 2, 3, 1, 2, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & basicop_info);
  add_button_data ("*", tableRelview, 3, 4, 1, 2, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 3,
                   & basicop_info);
  add_button_data ("^", tableRelview, 4, 5, 1, 2, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 4,
                   & basicop_info);

  add_button_data ("S/R", tableRelview, 0, 1, 3, 4, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & resquot_info);
  add_button_data ("R\\S", tableRelview, 1, 2, 3, 4, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & resquot_info);
  add_button_data ("SYQ", tableRelview, 2, 3, 3, 4, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & resquot_info);

  add_button_data ("TRANS", tableRelview, 0, 1, 5, 6, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & closure_info);
  add_button_data ("REFL", tableRelview, 1, 2, 5, 6, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & closure_info);
  add_button_data ("SYMM", tableRelview, 2, 3, 5, 6, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & closure_info);

  add_button ("DEF", tableRelview, 0, 1, 7, 8, GTK_SIGNAL_FUNC (buttonDomDefClicked), 0);
  add_button_data ("1st", tableRelview, 1, 2, 7, 8, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & domain_info);
  add_button_data ("2nd", tableRelview, 2, 3, 7, 8, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & domain_info);
  add_button_data ("ORD", tableRelview, 3, 4, 7, 8, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & domain_info);

  add_button_data ("TUP", tableRelview, 2, 3, 9, 10, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & dirprod_info);
  add_button_data ("P-2", tableRelview, 1, 2, 9, 10, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & dirprod_info);
  add_button_data ("P-1", tableRelview, 0, 1, 9, 10, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & dirprod_info);

  add_button_data ("SUM", tableRelview, 2, 3, 11, 12, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & dirsum_info);
  add_button_data ("I-2", tableRelview, 1, 2, 11, 12, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 1,
                   & dirsum_info);
  add_button_data ("I-1", tableRelview, 0, 1, 11, 12, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & dirsum_info);

  add_button_data ("INJ", tableRelview, 3, 4, 13, 14, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 3,
                   & powerset_info);
  add_button_data ("TOTF", tableRelview, 2, 3, 13, 14, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 2,
                   & powerset_info);
  add_button_data ("PARTF", tableRelview, 1, 2, 13, 14, GTK_SIGNAL_FUNC (buttonMultiOpClicked),
                   1, & powerset_info);
  add_button_data ("EPSI", tableRelview, 0, 1, 13, 14, GTK_SIGNAL_FUNC (buttonMultiOpClicked), 0,
                   & powerset_info);

  gtk_signal_connect (GTK_OBJECT (windowRelviewOps), "delete_event",
                      GTK_SIGNAL_FUNC (rvops_delete_event), NULL);

  return windowRelviewOps;
}
