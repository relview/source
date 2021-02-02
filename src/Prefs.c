/*
 * Prefs.c
 *
 *  Copyright (C) 2000-2011 University of Kiel, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/****************************************************************************/
/* PREFERENCES                                                              */
/* ------------------------------------------------------------------------ */
/*  PURPOSE: Maintains a chained list of preference structs that can be     */
/*           read and written from and to the disk. File format is the      */
/*           common INI file format with [SECTIONS] and key=value pairs.    */
/*           Advantage of this format is that it is easily understood, can  */
/*           be edited and viewed by humans and has no problem with         */
/*           different endian architectures because all data is ASCII       */
/*           based.                                                         */
/*  CREATED: 21-AUG-2000 WL                                                 */
/* MODIFIED: 06-OCT-2000 WL: prefs_store/restore_window_info changed:       */
/*                           Window layout is only explicitly saved, not on */
/*                           each application shutdown.                     */
/****************************************************************************/

#include "Relview.h" // Verbose
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "prefs.h"

GSList * sections = NULL;

/* prototypes, only for local use */

static PPrefSection find_section (char * name);
static PItem find_key (GSList * items, char * key);
static void free_section (PPrefSection sec);
static void free_items (GSList * items);
static void save_section (FILE * prefs, PPrefSection sec);
static void save_items (FILE * prefs, GSList * items);
static void load_sections (FILE * prefs);


/* ------------------------------------------------------ Prefs Observer --- */

GSList * _observers = NULL;

void prefs_register_observer (PrefsObserver * observer)
{
  if (NULL == g_slist_find (_observers, observer)) {
    _observers = g_slist_prepend (_observers, observer);
  }
}

void prefs_unregister_observer (PrefsObserver * observer)
{
  _observers = g_slist_remove (_observers, observer);
}

#define PREFS_OBSERVER_NOTIFY(func,...) \
        OBSERVER_NOTIFY_GLOBAL(_observers,GSList,PrefsObserver,func, __VA_ARGS__)

/****************************************************************************/
/* PUBLIC FUNCTIONS                                                         */
/****************************************************************************/

/****************************************************************************/
/*       NAME: prefs_load                                                   */
/*    PURPOSE: Discards all current data and loads the specified prefs      */
/*             file.                                                        */
/* PARAMETERS: If the filename does not contain an absolute path the file   */
/*             is searched in the home directory of the current user.       */
/*    CREATED: 21-AUG-2000 WL                                               */
/****************************************************************************/
void prefs_load (char * filename)
{
  FILE * prefs;
  GString * file;

  file = g_string_new (filename);
  if (!g_path_is_absolute (filename))
    g_string_sprintf (file, "%s/%s", g_get_home_dir (), filename);

  prefs = fopen (file->str, "r");
  if (!prefs)
    return;

  prefs_clear ();
  load_sections (prefs);

  fclose (prefs);
  g_string_free (file, TRUE);
}

/****************************************************************************/
/*       NAME: prefs_save                                                   */
/*    PURPOSE: Save all sections and values to the specified file.          */
/* PARAMETERS: If the filename does not contain an absolute path the file   */
/*             is created in the home directory of the current user.        */
/*    CREATED: 21-AUG-2000 WL                                               */
/****************************************************************************/
void prefs_save (char * filename)
{
  FILE * prefs;
  GString * file;

  file = g_string_new (filename);
  if (!g_path_is_absolute (filename))
    g_string_sprintf (file, "%s/%s", g_get_home_dir (), filename);

  verbose_printf (VERBOSE_DEBUG, "Saving prefs to file \"%s\" ...\n", file->str);

  prefs = fopen (file->str, "w");
  if (!prefs) {
    fprintf (stderr, "Could not write to preferences file \"%s\"\n", file->str);
    return;
  }

  prefs_save_to_stream (prefs);

  fclose (prefs);
  g_string_free (file, TRUE);
}

/****************************************************************************/
/*       NAME: prefs_save_to_stream                                         */
/*    PURPOSE: Save all sections and values to the specified file STREAM.   */
/* PARAMETERS: Similar to prefs_save (and even used by it). This is useful  */
/*             for debug output to stdout or to reuse open streams.         */
/*    CREATED: 22-AUG-2000 WL                                               */
/****************************************************************************/
void prefs_save_to_stream (FILE * prefs)
{
  GSList * current = sections;

  while (current) {
    save_section (prefs, current->data);
    current = current->next;
  }
}

/****************************************************************************/
/*       NAME: prefs_clear                                                  */
/*    PURPOSE: Clears all sections and values and frees allocated memory.   */
/*    CREATED: 21-AUG-2000 WL                                               */
/****************************************************************************/
void prefs_clear ()
{
  while (sections) {
    free_section (sections->data);
    sections = g_slist_remove (sections, sections->data);
  }
}

/****************************************************************************/
/*       NAME: prefs_remove_section                                         */
/*    PURPOSE: Removes [section] if it exists.                              */
/*    CREATED: 01-SEP-2000 WL                                               */
/****************************************************************************/
void prefs_remove_section (char * section)
{
  PPrefSection sec = find_section (section);

  if (sec) {
    free_section (sec);
    sections = g_slist_remove (sections, sec);
  }
}


/****************************************************************************/
/*       NAME: prefs_get_string                                             */
/*    PURPOSE: Find value for key in section. Returns NULL if not found.    */
/*             If != NULL, return value must be freed (g_free (val);).      */
/*    CREATED: 21-AUG-2000 WL                                               */
/****************************************************************************/
char * prefs_get_string (char * section, const char * key)
{
  PPrefSection sec;
  PItem item;

  /* Get section. If it does not exist: exit function. */
  sec = find_section (section);
  if (!sec)
    return NULL;

  /* Find key. If it does not exist: exit function. */
  item = find_key (sec->items, key);
  if (!item)
    return NULL;

  /* Get value. */
  return g_strdup (item->value);
}

/*!
 * Returns the value for key in the given section. If no such value exists,
 * the return value is defval, in case is it non-NULL. Otherwise the return
 * value is "".
 *
 * \author stb
 * \date 31.10.2008
 *
 * \param section The section that holds the key/value pair.
 * \param key The key to look for.
 * \param defval The defval to return in case, there is no value given
 *               for the key in the given section.
 * \return Returns the value for key, if key has a value in the given section.
 *         If not, a copy of defval is returned in case it is non-NULL.
 *         Otherwise the return value is a copy of "". In any case the string
 *         returned must be freed with free() and is non-NULL.
 */
char * prefs_get_string_default (char * section, char * key,
                                 char * defval)
{
  const char * value = prefs_get_string (section, key);
  if (NULL == value)
    value = defval ? defval : "";
  return strdup (value);
}


/****************************************************************************/
/*       NAME: prefs_get_int                                                */
/*    PURPOSE: Find value for key in section. Returns defaultval if not     */
/*             found.                                                       */
/*    CREATED: 28-AUG-2000 WL                                               */
/****************************************************************************/
int prefs_get_int (char * section, char * key, int defaultval)
{
  gchar * value;
  int result = defaultval;

  value = prefs_get_string (section, key);
  if (value)
    result = atoi (value);

  g_free (value);
  return result;
}

/*!
 * Sets the value of the given key in given section. Does not notify
 * the observers.
 *
 * \author stb
 * \date 12.06.2009
 */
void prefs_set_string_no_notify (char * section, char * key, char * value)
{
  PPrefSection sec;
  PItem item;

  /* Get section. If it does not exist: create and append to sections. */
  sec = find_section (section);
  if (!sec) {
    sec = g_malloc (sizeof (TPrefSection));
    sec->name = g_strdup (section);
    sec->items = NULL;
    sections = g_slist_append (sections, sec);
  }

  /* Find key. If it does not exist: create and append to sec->items. */
  item = find_key (sec->items, key);
  if (!item) {
    item = g_malloc (sizeof (TItem));
    item->key = g_strdup (key);
    item->value = NULL;
    sec->items = g_slist_append (sec->items, item);
  }

  /* Set value. */
  g_free (item->value);
  item->value = g_strdup (value);
}

/*!
 * Like prefs_set_string_no_notify, but calls the notifiers.
 *
 * \author wl,stb
 * \data 21.08.2000
 */
void prefs_set_string (char * section, char * key, char * value)
{
	prefs_set_string_no_notify (section, key, value);
	PREFS_OBSERVER_NOTIFY (changed, _2(section,key));
}

/*!
 * Sets the value of the given key in given section. Does not notify
 * the observers. Wrapper function for prefs_set_string_no_notify.
 *
 * \author stb
 * \date 12.06.2009
 */
void prefs_set_int_no_notify (char * section, char * key, int value)
{
  gchar * valstr;

  valstr = g_strdup_printf ("%d", value);
  prefs_set_string_no_notify (section, key, valstr);
  g_free (valstr);
}

/*!
 * Like prefs_set_int_no_notify, but calls the notifiers.
 *
 * \author wl,stb
 * \data 28.08.2000
 */
void prefs_set_int (char * section, char * key, int value)
{
	prefs_set_int_no_notify (section, key, value);
	PREFS_OBSERVER_NOTIFY (changed, _2(section,key));
}



/*!
 * Sets the value of the given key in given section. Does not notify
 * the observers. Wrapper function for prefs_set_string_no_notify.
 *
 * \author stb
 */
void prefs_set_float_no_notify (char * section, char * key, double value)
{
  gchar * valstr;

  valstr = g_strdup_printf ("%lf", value);
  prefs_set_string_no_notify (section, key, valstr);
  g_free (valstr);
}


/*!
 * Like prefs_set_float_no_notify, but calls the notifiers.
 *
 * \author stb
 */
void prefs_set_float (char * section, char * key, double value)
{
	prefs_set_float_no_notify (section, key, value);
	PREFS_OBSERVER_NOTIFY (changed, _2(section,key));
}


/****************************************************************************/
/*       NAME: prefs_store_glist                                            */
/*    PURPOSE: Replaces [section] with items from list which must all be    */
/*             char *.                                                      */
/*    CREATED: 01-SEP-2000 WL                                               */
/****************************************************************************/
void prefs_store_glist (char * section, GList * list)
{
  char key [5];
  int i = 1;

  prefs_remove_section (section);
  while (list) {
    sprintf (key, "%04d", i ++);
    prefs_set_string (section, key, list->data);
    list = list->next;
  }
}

/****************************************************************************/
/*       NAME: prefs_restore_glist                                          */
/*    PURPOSE: Builts GList from items in [section]. Only items with four   */
/*             digits and ascending from 0001 are used.                     */
/*    CREATED: 01-SEP-2000 WL                                               */
/****************************************************************************/
GList * prefs_restore_glist (char * section)
{
  GList * list = NULL;
  char key [5];
  char * value;
  int i = 1;

  while (TRUE) {
    sprintf (key, "%04d", i ++);
    value = prefs_get_string (section, key);
    if (!value)
      break;
    list = g_list_append (list, value);
  }
  return list;
}

/****************************************************************************/
/* PRIVATE FUNCTIONS                                                        */
/****************************************************************************/
static PPrefSection find_section (char * name)
{
  GSList * current = sections;

  while (current)
    if (strcmp (((PPrefSection) current->data)->name, name) == 0)
      return current->data;
    else
      current = current->next;

  return NULL;
}

static PItem find_key (GSList * items, char * key)
{
  while (items)
    if (strcmp (((PItem) items->data)->key, key) == 0)
      return items->data;
    else
      items = items->next;

  return NULL;
}

static void free_section (PPrefSection sec)
{
  if (!sec)
    return;

  g_free (sec->name);
  free_items (sec->items);
  g_free (sec);
}

static void free_items (GSList * items)
{
  PItem item;

  while (items) {
    item = items->data;
    g_free (item->key);
    g_free (item->value);
    g_free (item);
    items = g_slist_remove (items, item);
  }
}

static void save_section (FILE * prefs, PPrefSection sec)
{
  fprintf (prefs, "[%s]\n", sec->name);
  save_items (prefs, sec->items);
  fprintf (prefs, "\n");
}

static void save_items (FILE * prefs, GSList * items)
{
  PItem item;

  while (items) {
    item = items->data;
    fprintf (prefs, "%s=%s\n", item->key, item->value);
    items = items->next;
  }
}

void split_line (char * line, char * key, char * value)
{
  char * equal;

  key [0] = value [0] = '\0';
  if (!line || strlen (line) < 3)
    return;

  equal = strchr (line, '=');
  if (!equal || (line == equal) || (line + strlen (line) - 1 == equal))
    return;

  memset (key, '\0', equal - line + 1);
  strncpy (key, line, equal - line);
  g_strstrip (key);
  strcpy (value, equal + 1);
  g_strstrip (value);
}

static void load_sections (FILE * prefs)
{
  char line [200];
  char key [200];
  char value [200];
  PPrefSection sec = NULL;

  /* find section name */
  while (!feof (prefs) && !ferror (prefs)) {
    fgets (line, 200, prefs);
    g_strstrip (line); /* trim whitespaces away */
    if (strlen (line) == 0) /* empty line? */
      continue;
    if (line [0] == ';') /* comment? */
      continue;

    if (line [0] == '[') { /* section header? */
      if (strlen (line) <3)
        fprintf (stderr,
                 "Error in pref file (invalid section header), line: %s\n",
                 line);
      else if (line [strlen (line) - 1] != ']')
        fprintf (stderr,
                 "Error in pref file (invalid section header), line: %s\n",
                 line);
      else {
        sec = g_malloc0 (sizeof (TPrefSection));
        sec->name = g_strndup (& line [1], strlen (line) - 2);
        sections = g_slist_append (sections, sec);
      }

    } else /* key/value pair */
      if (!sec)
        fprintf (stderr,
                 "Error in pref file (section header expected), line: %s\n",
                 line);
      else {
        split_line (line, key, value);
        prefs_set_string (sec->name, key, value);
      }
  }
}
