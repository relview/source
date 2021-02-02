#include <gtk/gtk.h>

GtkWidget *windowRelviewOps;

void rvops_init ();
void show_rvops ();
void destroy_rvops ();
void hide_rvops ();
int rvops_isVisible (void);



GtkWidget* create_windowRelviewOps (void);

gint rvops_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data);

void buttonMultiOpClicked (GtkButton *button, gpointer user_data);
void buttonDomDefClicked (GtkButton *button, gpointer user_data);
