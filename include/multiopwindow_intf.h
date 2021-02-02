#ifndef __multiopwindow_intf_h__
#define __multiopwindow_intf_h__

#include "multiop.h"

void create_windowMultiOp (MultiOpInfo *info);

void on_buttonApplyMO_clicked (GtkButton *button, gpointer user_data);
void on_buttonCloseMO_pressed (GtkButton *button, gpointer user_data);

void button_toggled (GtkToggleButton *togglebutton, gpointer user_data);

#endif
