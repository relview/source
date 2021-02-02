#ifndef __multiop_h__
#define __multiop_h__

#include <gtk/gtk.h>

struct OpInfo {
  int paramcount;           /* number of parameters for this op [1,2] */
  char *name;               /* text to be shown in the togglebutton */
  char *syntax;             /* sprintf template */
  GtkWidget *toggle;
  /* custom calculation function, usually = std_proc */
  void (*process_op) (gchar *name, gchar *param1, gchar *param2, struct OpInfo *opinfo);
};

typedef struct {
  char *caption;            /* title of dialog, also used for prefs, must not have '=' */
  int opcount;              /* number of operations */
  int buttonwidth;          /* width of each togglebutton [pixel] */
  struct OpInfo *opinfos;
  GtkWidget *window,
    *entryResultMO, *entryParam1MO, *entryParam2MO,
    *labelParam2MO, *buttonApplyMO, *buttonCloseMO;
} MultiOpInfo;

/* publish new multi operation structs here, definition should be in multiop.c */
extern MultiOpInfo basicop_info;
extern MultiOpInfo resquot_info;
extern MultiOpInfo closure_info;
extern MultiOpInfo domain_info;
extern MultiOpInfo dirprod_info;
extern MultiOpInfo dirsum_info;
extern MultiOpInfo powerset_info;

/* prototypes */

void std_proc (gchar *name, gchar *param1, gchar *param2, struct OpInfo *opinfo);
void ord_proc (gchar *name, gchar *param1, gchar *param2, struct OpInfo *opinfo);

#endif
