#ifndef HISTORY_H
#  define HISTORY_H

#include <glib.h>

typedef struct _History 
{
  gint maxlen;
  GList * list;
} History;


void history_new (History * h, gint maxlen);
void history_new_from_glist (History * h, gint maxlen, const GList *list);
const GList * history_get_glist (History * h);
void history_free (History * h);
void history_prepend (History * h, const gchar * elem);
void history_set_maxlen (History * h, int maxlen);
gboolean history_is_empty (History * h);
gint history_position (History * h, const char * str);

#endif /* history.h */
