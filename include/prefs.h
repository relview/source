/*
 * prefs.h
 *
 *  Copyright (C) 2000-2010 University of Kiel, Germany
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

#ifndef __prefs_h__
#define __prefs_h__

#include <stdio.h>
#include <gtk/gtk.h>

extern GSList *sections;

/******************************************************************************/
/* PREFERENCES DATA STRUCTURES                                                */
/******************************************************************************/

typedef struct {
  char *name;
  GSList *items;
} TPrefSection;

typedef TPrefSection *PPrefSection;

typedef struct {
  char *key, *value;
} TItem;

typedef TItem *PItem;


typedef void (*prefs_observer_changed_func_t) (gpointer*,
		const gchar* /*section*/, const gchar* /*key*/);

#define PREFS_OBSERVER_CHANGED_FUNC(f) \
        ((prefs_observer_changed_func_t)(f))

typedef struct _PrefsObserver {
        prefs_observer_changed_func_t changed;

        gpointer object;
} PrefsObserver;

void prefs_register_observer (PrefsObserver*);
void prefs_unregister_observer (PrefsObserver*);

/******************************************************************************/
/* PUBLIC FUNCTIONS                                                           */
/******************************************************************************/

/* load/save */
void prefs_load (char *filename);
void prefs_save (char *filename);
void prefs_save_to_stream (FILE *prefs);

/* get/set */
char *prefs_get_string (char *section, const char *key);
void prefs_set_string (char *section, char *key, char *value);
void prefs_set_string_no_notify (char * section, char * key, char * value);

int prefs_get_int (char *section, char *key, int defaultval);
void prefs_set_int (char *section, char *key, int value);
void prefs_set_int_no_notify (char * section, char * key, int value);

void prefs_set_float (char * section, char * key, double value);
void prefs_set_float_no_notify (char * section, char * key, double value);

void prefs_store_glist (char *section, GList *list);
GList* prefs_restore_glist (char *section);

/* clear/remove */
void prefs_clear ();
void prefs_remove_section (char *section);

/* window layout management */
int prefs_visible_by_name (char *name);
void prefs_restore_window_info (GtkWindow *window);
void prefs_restore_window_info_with_name (GtkWindow * window, gchar * name);
void prefs_store_window_info (char *name, int x, int y, int width, int height,
                              int visible);

char * prefs_get_string_default (char * section, char * key,
                                 char * defval);

#endif
