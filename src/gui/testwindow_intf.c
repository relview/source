#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

//#include "utilities.h"
//#include "rv_gtk.h"

void on_buttonTestT_clicked (GtkButton *button, gpointer user_data);
void on_buttonCloseT_clicked (GtkButton *button, gpointer user_data);
void on_radiobutton_toggled (GtkToggleButton *togglebutton, gpointer user_data);

GtkWidget *windowTests = NULL;


#define pixmaps_square  /* undefine to get round pixmaps */

/* Bitmaps */

#ifdef pixmaps_square

GdkBitmap *maskSquareChecked;

static char * square_checked_data [] = {
"16 16 3 1",
"       c None",
".      c #000000000000",
"X      c #FFFFFFFFFFFF",
"                ",
" ..........   ..",
" .          ... ",
" .         ...  ",
" .        ...   ",
" .       ...  . ",
" .  .   ....  . ",
" . ...  ...   . ",
" ...... ...   . ",
" . .......    . ",
" .  ......    . ",
" .   ....     . ",
" .    ...     . ",
" .     .      . ",
" .............. ",
"                "};

GdkBitmap *maskSquareClear;

static char * square_clear_data [] = {
"16 16 3 1",
"       c None",
".      c #000000000000",
"X      c #FFFFFFFFFFFF",
"                ",
" .............. ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .            . ",
" .............. ",
"                "};

#else

GdkBitmap *maskRoundChecked;

static char * round_checked_data [] = {
"17 17 3 1",
"       c None",
".      c #000000000000",
"X      c #FFFFFFFFFFFF",
"                 ",
"      .....      ",
"    .........    ",
"   ...     ...   ",
"  ..         ..  ",
"  ..   ...   ..  ",
" ..   .....   .. ",
" ..  .......  .. ",
" ..  .......  .. ",
" ..  .......  .. ",
" ..   .....   .. ",
"  ..   ...   ..  ",
"  ..         ..  ",
"   ...     ...   ",
"    .........    ",
"      .....      ",
"                 "};

GdkBitmap *maskRoundClear;

static char * round_clear_data [] = {
"17 17 3 1",
"       c None",
".      c #000000000000",
"X      c #FFFFFFFFFFFF",
"                 ",
"      .....      ",
"    .........    ",
"   ...     ...   ",
"  ..         ..  ",
"  ..         ..  ",
" ..           .. ",
" ..           .. ",
" ..           .. ",
" ..           .. ",
" ..           .. ",
"  ..         ..  ",
"  ..         ..  ",
"   ...     ...   ",
"    .........    ",
"      .....      ",
"                 "};

#endif

/* public widgets */


GtkWidget *entryRel1T = NULL;
GtkWidget *entryRel2T = NULL;
GtkWidget *labelRel1T = NULL;
GtkWidget *labelRel2T = NULL;
GtkWidget *buttonTestT = NULL;
GtkWidget *buttonCloseT = NULL;
GtkWidget *radiobutton1DT = NULL;
GtkWidget *radiobutton2DT = NULL;
GtkWidget *clistResultsT = NULL;

GdkPixmap *res_checked = NULL;
GdkPixmap *res_clear = NULL;


GtkWidget *create_windowTests (void)
{
  GtkWidget *vbox1;
  GtkWidget *frame1;
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GSList *hbox1_group = NULL;
  GtkWidget *table1;
  GtkWidget *frame2;
  GtkWidget *vbox3;
  GtkWidget *scrolledwindow1;
  GtkWidget *label9;
  GtkWidget *hbuttonbox1;
  GdkColormap *colormap;

  windowTests = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (windowTests, "windowTests");
  gtk_object_set_data (GTK_OBJECT (windowTests), "windowTests", windowTests);
  gtk_window_set_title (GTK_WINDOW (windowTests), "Tests");
  gtk_window_set_policy (GTK_WINDOW (windowTests), FALSE, TRUE, FALSE);
  gtk_widget_set_usize (windowTests, 280,300);
  /* gtk_window_set_default_size (GTK_WINDOW (windowTests), 220,400); */

  vbox1 = gtk_vbox_new (FALSE, 5);
  gtk_widget_set_name (vbox1, "vbox1");
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "vbox1", vbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (windowTests), vbox1);

  frame1 = gtk_frame_new ("Relations");
  gtk_widget_set_name (frame1, "frame1");
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox1), frame1, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

  vbox2 = gtk_vbox_new (FALSE, 4);
  gtk_widget_set_name (vbox2, "vbox2");
  gtk_widget_ref (vbox2);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "vbox2", vbox2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (frame1), vbox2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 5);

  hbox1 = gtk_hbox_new (FALSE, 30);
  gtk_widget_set_name (hbox1, "hbox1");
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

  radiobutton1DT = gtk_radio_button_new_with_label (hbox1_group, "Monadic Tests");
  hbox1_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton1DT));
  gtk_widget_set_name (radiobutton1DT, "radiobutton1DT");
  gtk_widget_ref (radiobutton1DT);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "radiobutton1DT", radiobutton1DT,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton1DT);
  gtk_box_pack_start (GTK_BOX (hbox1), radiobutton1DT, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton1DT), TRUE);

  radiobutton2DT = gtk_radio_button_new_with_label (hbox1_group, "Dyadic Tests");
  hbox1_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton2DT));
  gtk_widget_set_name (radiobutton2DT, "radiobutton2DT");
  gtk_widget_ref (radiobutton2DT);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "radiobutton2DT", radiobutton2DT,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton2DT);
  gtk_box_pack_start (GTK_BOX (hbox1), radiobutton2DT, FALSE, FALSE, 0);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_set_name (table1, "table1");
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox2), table1, TRUE, TRUE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);

  entryRel1T = gtk_entry_new ();
  gtk_widget_set_name (entryRel1T, "entryRel1T");
  gtk_widget_ref (entryRel1T);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "entryRel1T", entryRel1T,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entryRel1T);
  gtk_table_attach (GTK_TABLE (table1), entryRel1T, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  entryRel2T = gtk_entry_new ();
  gtk_widget_set_name (entryRel2T, "entryRel2T");
  gtk_widget_ref (entryRel2T);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "entryRel2T", entryRel2T,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entryRel2T);
  gtk_table_attach (GTK_TABLE (table1), entryRel2T, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  labelRel1T = gtk_label_new ("1st Relation");
  gtk_widget_set_name (labelRel1T, "labelRel1T");
  gtk_widget_ref (labelRel1T);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "labelRel1T", labelRel1T,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelRel1T);
  gtk_table_attach (GTK_TABLE (table1), labelRel1T, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (labelRel1T), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (labelRel1T), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (labelRel1T), 10, 0);

  labelRel2T = gtk_label_new ("2nd Relation");
  gtk_widget_set_name (labelRel2T, "labelRel2T");
  gtk_widget_ref (labelRel2T);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "labelRel2T", labelRel2T,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelRel2T);
  gtk_table_attach (GTK_TABLE (table1), labelRel2T, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (labelRel2T), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (labelRel2T), 1, 0.5);
  gtk_misc_set_padding (GTK_MISC (labelRel2T), 10, 0);

  frame2 = gtk_frame_new ("Testresults");
  gtk_widget_set_name (frame2, "frame2");
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "frame2", frame2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (vbox1), frame2, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame2), 5);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox3, "vbox3");
  gtk_widget_ref (vbox3);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "vbox3", vbox3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox3);
  gtk_container_add (GTK_CONTAINER (frame2), vbox3);
  gtk_container_set_border_width (GTK_CONTAINER (vbox3), 5);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow1, "scrolledwindow1");
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (vbox3), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  clistResultsT = gtk_clist_new (1);
  gtk_widget_set_name (clistResultsT, "clistResultsT");
  gtk_widget_ref (clistResultsT);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "clistResultsT", clistResultsT,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clistResultsT);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clistResultsT);
  gtk_clist_set_column_width (GTK_CLIST (clistResultsT), 0, 80);
  gtk_clist_column_titles_hide (GTK_CLIST (clistResultsT));

  label9 = gtk_label_new ("label9");
  gtk_widget_set_name (label9, "label9");
  gtk_widget_ref (label9);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "label9", label9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label9);
  gtk_clist_set_column_widget (GTK_CLIST (clistResultsT), 0, label9);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_set_name (hbuttonbox1, "hbuttonbox1");
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "hbuttonbox1", hbuttonbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox1, FALSE, TRUE, 0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 0);

  buttonTestT = gtk_button_new_with_label ("Test");
  gtk_widget_set_name (buttonTestT, "buttonTestT");
  gtk_widget_ref (buttonTestT);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "buttonTestT", buttonTestT,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonTestT);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), buttonTestT);
  GTK_WIDGET_SET_FLAGS (buttonTestT, GTK_CAN_DEFAULT);
  /* gtk_widget_grab_default (buttonTestT); */

  buttonCloseT = gtk_button_new_with_label ("Close");
  gtk_widget_set_name (buttonCloseT, "buttonCloseT");
  gtk_widget_ref (buttonCloseT);
  gtk_object_set_data_full (GTK_OBJECT (windowTests), "buttonCloseT", buttonCloseT,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonCloseT);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), buttonCloseT);
  GTK_WIDGET_SET_FLAGS (buttonCloseT, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (radiobutton1DT), "toggled",
                      GTK_SIGNAL_FUNC (on_radiobutton_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (radiobutton2DT), "toggled",
                      GTK_SIGNAL_FUNC (on_radiobutton_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (buttonTestT), "clicked",
                      GTK_SIGNAL_FUNC (on_buttonTestT_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (buttonCloseT), "clicked",
                      GTK_SIGNAL_FUNC (on_buttonCloseT_clicked),
                      NULL);

  /* result pixmaps */

  colormap = gtk_widget_get_colormap (windowTests);

#ifdef pixmaps_square
  res_checked = gdk_pixmap_colormap_create_from_xpm_d (
      NULL, colormap, &maskSquareChecked, NULL, (gchar **) square_checked_data);
  res_clear = gdk_pixmap_colormap_create_from_xpm_d (
      NULL, colormap, &maskSquareClear, NULL, (gchar **) square_clear_data);
#else
  res_checked = gdk_pixmap_colormap_create_from_xpm_d (
      NULL, colormap, &maskRoundChecked, NULL, (gchar **) round_checked_data);
  res_clear = gdk_pixmap_colormap_create_from_xpm_d (
      NULL, colormap, &maskRoundClear, NULL, (gchar **) round_clear_data);
#endif

  return windowTests;
}

