#include <gtk/gtk.h>

void test_window_init ();
void show_testwindow ();
void hide_testwindow ();
int testwindow_isVisible (void);
void test_window_destroy ();


extern GtkWidget *windowTests;
extern GtkWidget *entryRel1T;
extern GtkWidget *entryRel2T;
extern GtkWidget *labelRel1T;
extern GtkWidget *labelRel2T;
extern GtkWidget *buttonTestT;
extern GtkWidget *buttonCloseT;
extern GtkWidget *radiobutton1DT;
extern GtkWidget *radiobutton2DT;
extern GtkWidget *clistResultsT;
extern GdkPixmap *res_checked;
extern GdkPixmap *res_clear;

GtkWidget *create_windowTests (void);

void on_buttonTestT_clicked (GtkButton *button, gpointer user_data);
void on_buttonCloseT_clicked (GtkButton *button, gpointer user_data);
void on_radiobutton_toggled (GtkToggleButton *togglebutton, gpointer user_data);
