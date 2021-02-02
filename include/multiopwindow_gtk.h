#ifndef __multiopwindow_gtk_h__
#define __multiopwindow_gtk_h__

#include "multiop.h"

void multiop_window_init (MultiOpInfo *info);
void show_multiopwindow (MultiOpInfo *info);
void hide_multiopwindow (MultiOpInfo *info);
int multiopwindow_isVisible (MultiOpInfo *info);
void set_current_op (MultiOpInfo *info, int op);
void multiop_window_destroy (MultiOpInfo *info);

#endif
