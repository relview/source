#include "history.h"

#include <string.h>
#include <assert.h>

/*private*/ static void history_trim (History * h);


void history_new (History * h, gint maxlen)
{
  assert (maxlen > 0);

  h->maxlen = maxlen;
  h->list = NULL;
}

void history_new_from_glist (History * h, gint maxlen, const GList *list)
{
  if (NULL == list) {
    history_new (h, maxlen);
  }
  else {
    GList * iter = NULL;
    
    assert (maxlen > 0);

    h->maxlen = maxlen;
    h->list = NULL;
    
    for (iter = g_list_last (list) ; iter != NULL
           ; iter = g_list_previous (iter)) {
      h->list = g_list_prepend (h->list, strdup ((char*) iter->data));
    }
    history_trim (h);
  }
}

const GList * history_get_glist (History * h)
{
  return h->list;
}

gboolean history_is_empty (History * h)
{
  return NULL == h->list;
}

void history_free (History * h)
{
  g_list_foreach (h->list, (GFunc) g_free, NULL);
  g_list_free (h->list);
  h->list = NULL;
}

void history_prepend (History * h, const gchar * elem)
{
  /* remove a maybe previous occurence in of elem in the history */
  GList * iter = g_list_find_custom (h->list, elem,
                                     (GCompareFunc) strcmp);
  if (iter) {
    h->list = g_list_delete_link (h->list, iter);
  }

  h->list = g_list_prepend (h->list, strdup ((char*) elem));
  history_trim (h);
}

/* returns the index of the str (0-index), from new-to-old.
 * returns -1, if `str` is not in the history. */
gint history_position (History * h, const char * str)
{
  GList * iter = g_list_find_custom (h->list, str,
                                     (GCompareFunc) strcmp);
  if (! iter)
    return -1;
  else
    return g_list_position (h->list, iter);
}

void history_set_maxlen (History * h, int maxlen)
{
  assert (maxlen > 0);
  h->maxlen = maxlen;
  history_trim (h);
}

/*private*/ void history_trim (History * h)
{
  GList * iter = g_list_nth (h->list, h->maxlen);
  if (iter != NULL) {
    while (iter != NULL) {
      GList * tmp = iter;

      g_free (iter->data);
      iter = g_list_delete_link (tmp, iter);
    }
  }
}


#if 0

/* gcc test.c history.c `pkg-config glib-2.0 --cflags --libs` -I../include */

#include "history.h"

void dump (GList * l)
{
        GList * iter;
        int i = 0;
        for (iter = g_list_first (l) ; iter != NULL ; iter = g_list_next (iter)) {
                printf ("[%3d] \"%s\"\n", i++, iter->data);
        }
}

int main ()
{
        History h;
        history_new (&h, 5);
        history_prepend (&h, "h");
        history_prepend (&h, "a");
        history_prepend (&h, "l");
        history_prepend (&h, "l");
        history_prepend (&h, "o");
        dump (h.list);
        printf ("----\n");
        history_prepend (&h, "w");
        history_prepend (&h, "e");
        history_prepend (&h, "l");
        history_prepend (&h, "t");
        dump (h.list);
        history_free (&h);
}

#endif
