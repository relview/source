#include <gtk/gtk.h>

gboolean
show_input_string (const GString *caption, const GString *desc,
                   GString *value, int maxlen);

/* EXAMPLE

  GString *caption, *desc, *value;

  caption = g_string_new ("Rename Relation");
  desc = g_string_new ("Enter new relation name:");
  value = g_string_new (rel->name);

  if (show_input_string (caption, desc, value, MAXNAMELENGTH))
    ren_relation (value->str);

  g_string_free (caption, TRUE);
  g_string_free (desc, TRUE);
  g_string_free (value, TRUE);

*/
