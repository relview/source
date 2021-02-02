#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "utilities.h"
#include "multiopwindow_intf.h"

/******************************************************************************/
/*       NAME : add_toggle_button                                             */
/*    PURPOSE : Create a toggle button, packs it into the hbox, and sets all  */
/*               important properties.                                        */
/*    CREATED : 10-AUG-2000 WL                                                */
/******************************************************************************/

GtkWidget *add_toggle_button (GtkWidget *box, gchar *caption, int idx,
                              GtkSignalFunc sigfunc, MultiOpInfo *info)
{
  GtkWidget *toggle;

  toggle = gtk_toggle_button_new_with_label (caption);
  gtk_widget_ref (toggle);
  gtk_widget_show (toggle);
  gtk_box_pack_start (GTK_BOX (box), toggle, FALSE, FALSE, 0);
  gtk_widget_set_usize (toggle, info->buttonwidth, -2);
  gtk_signal_connect (GTK_OBJECT (toggle), "toggled",
                      GTK_SIGNAL_FUNC (sigfunc), info);

  return toggle;
}

/******************************************************************************/
/*       NAME : create_windowMultiOp                                          */
/*    PURPOSE : Create the multi operations window                            */
/*    CREATED : 10-AUG-2000 WL                                                */
/******************************************************************************/

void create_windowMultiOp (MultiOpInfo *info)
{
  GtkWidget *vbox1;
  GtkWidget *frame1;
  GtkWidget *table1;
  GtkWidget *label1;
  GtkWidget *hbox1;
  GtkWidget *label2;
  GtkWidget *hbuttonbox1;
  int i, max_paramcount = 0;

  for (i = 0; i < info->opcount; i++)
    if (info->opinfos[i].paramcount > max_paramcount)
      max_paramcount = info->opinfos[i].paramcount;

  info->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (info->window, info->caption);
  gtk_object_set_data (GTK_OBJECT (info->window), "info->window", info->window);
  gtk_window_set_title (GTK_WINDOW (info->window), info->caption);
  /* gtk_container_set_border_width (GTK_CONTAINER (info->window), 5); */

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox1, "vbox1");
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "vbox1", vbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (info->window), vbox1);

  frame1 = gtk_frame_new (NULL);
  gtk_widget_set_name (frame1, "frame1");
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

  if (max_paramcount < 2)
    table1 = gtk_table_new (3, 2, FALSE);
  else
    table1 = gtk_table_new (4, 2, FALSE);
  gtk_widget_set_name (table1, "table1");
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame1), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox1, "hbox1");
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_table_attach (GTK_TABLE (table1), hbox1, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  /* toggle buttons */

  for (i = 0; i < info->opcount; i++)
    info->opinfos[i].toggle = add_toggle_button (hbox1, info->opinfos[i].name, i,
                                                 GTK_SIGNAL_FUNC(button_toggled), info);

  /* entries & labels */

  /* result */
  label1 = gtk_label_new ("Result");
  gtk_widget_set_name (label1, "label1");
  gtk_widget_ref (label1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "label1", label1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label1);
  gtk_table_attach (GTK_TABLE (table1), label1, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label1), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label1), 5, 0);

  info->entryResultMO = gtk_entry_new ();
  gtk_widget_set_name (info->entryResultMO, "entryResultMO");
  gtk_widget_ref (info->entryResultMO);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "entryResultMO", info->entryResultMO,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (info->entryResultMO);
  gtk_table_attach (GTK_TABLE (table1), info->entryResultMO, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* param1 */
  if (max_paramcount >= 2)
    label2 = gtk_label_new ("1st Parameter");
  else
    label2 = gtk_label_new ("Parameter");
  gtk_widget_set_name (label2, "label2");
  gtk_widget_ref (label2);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "label2", label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label2), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (label2), 5, 0);

  info->entryParam1MO = gtk_entry_new ();
  gtk_widget_set_name (info->entryParam1MO, "entryParam1MO");
  gtk_widget_ref (info->entryParam1MO);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "entryParam1MO", info->entryParam1MO,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (info->entryParam1MO);
  gtk_table_attach (GTK_TABLE (table1), info->entryParam1MO, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* param2 */
  if (max_paramcount >= 2) {
    info->labelParam2MO = gtk_label_new ("2nd Parameter");
    gtk_widget_set_name (info->labelParam2MO, "labelParam2MO");
    gtk_widget_ref (info->labelParam2MO);
    gtk_object_set_data_full (GTK_OBJECT (info->window), "labelParam2MO", info->labelParam2MO,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (info->labelParam2MO);
    gtk_table_attach (GTK_TABLE (table1), info->labelParam2MO, 0, 1, 3, 4,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (info->labelParam2MO), 1, 0.5);
    gtk_misc_set_padding (GTK_MISC (info->labelParam2MO), 5, 0);


    info->entryParam2MO = gtk_entry_new ();
    gtk_widget_set_name (info->entryParam2MO, "entryParam2MO");
    gtk_widget_ref (info->entryParam2MO);
    gtk_object_set_data_full (GTK_OBJECT (info->window), "entryParam2MO", info->entryParam2MO,
                              (GtkDestroyNotify) gtk_widget_unref);
    gtk_widget_show (info->entryParam2MO);
    gtk_table_attach (GTK_TABLE (table1), info->entryParam2MO, 1, 2, 3, 4,
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
  }

  /* button box & buttons */

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_set_name (hbuttonbox1, "hbuttonbox1");
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "hbuttonbox1", hbuttonbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, FALSE, TRUE, 0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 0);

  info->buttonApplyMO = gtk_button_new_with_label ("Apply");
  gtk_widget_set_name (info->buttonApplyMO, "buttonApplyMO");
  gtk_widget_ref (info->buttonApplyMO);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "buttonApplyMO", info->buttonApplyMO,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (info->buttonApplyMO);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), info->buttonApplyMO);
  GTK_WIDGET_SET_FLAGS (info->buttonApplyMO, GTK_CAN_DEFAULT);

  info->buttonCloseMO = gtk_button_new_with_label ("Close");
  gtk_widget_set_name (info->buttonCloseMO, "buttonCloseMO");
  gtk_widget_ref (info->buttonCloseMO);
  gtk_object_set_data_full (GTK_OBJECT (info->window), "buttonCloseMO", info->buttonCloseMO,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (info->buttonCloseMO);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), info->buttonCloseMO);
  GTK_WIDGET_SET_FLAGS (info->buttonCloseMO, GTK_CAN_DEFAULT);

  /* button signals */

  gtk_signal_connect (GTK_OBJECT (info->buttonApplyMO), "clicked",
                      GTK_SIGNAL_FUNC (on_buttonApplyMO_clicked),
                      info);
  gtk_signal_connect (GTK_OBJECT (info->buttonCloseMO), "clicked",
                      GTK_SIGNAL_FUNC (on_buttonCloseMO_pressed),
                      info);

  /* activate default operation */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->opinfos[0].toggle), TRUE);
}
