#ifndef MARK_GRAPH_H
#  define MARK_GRAPH_H

#include <gtk/gtk.h>

typedef enum _RvMarkLevel
{
  FIRST = 0, SECOND = 1
} RvMarkLevel;

gboolean
show_mark_edgesnodes (const GString * caption, const GString * graphname,
                      GString * /*inout*/ value, int maxlen,
                      RvMarkLevel * plevel);

#endif /* mark_graph.h */
