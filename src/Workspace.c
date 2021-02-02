/*
 * Workspace.c
 *
 *  Copyright (C) 2010 Stefan Bolus, University of Kiel, Germany
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

/*!
 * Manages positions and visibility of the windows.
 *
 * Saving and Restoring the window positions and sizes is not that trivial
 * with GTK. First, if a window is going to be hidden, it's position is lost
 * and can't be recovered. It seems irrelevant how you show/hide it.
 *
 * If you register a handler for signal 'hide', gtk_window_get_position
 * returns wrong position of the window. Maybe because the window is hidden
 * already. Instead, gdk_window_get_root origin must be used and it seems
 * there is no GTK alternative for that.
 *
 * To move a window to a given position, gtk_window_move can be used.
 *
 * To resize a window, \ref gtk_widget_set_size_request can't be used, because
 * it sets a MINIMUM size. Use \ref gtk_window_set_default_size instead.
 */

#include "Workspace.h"
#include "Relview.h"
#include "prefs.h"

#include <assert.h>

/*!
 * Maintains information on the windows in the \ref Workspace. Some of them
 * either don't belong to the \ref GtkWindow or are lost in some situations,
 * e.g. when a window is hidden.
 */
typedef /*private*/ struct _StoredWindow
{
	GtkWindow * window;
	gchar * name;
	GdkRectangle rc;

	/* True, if the window is visible by default. Depends on the user
	 * preferences. */
	gboolean is_visible_by_def;

	/* Handler. These are necessary to disconnect the handlers if a window
	 * is removed. */
	gulong show_id, hide_id, destroy_id;
} StoredWindow;


/*!
 * Each window in a workspace has a unique name to identify it later on.
 */
/*private*/ struct _Workspace
{
	GtkWindowGroup * window_group;
	GHashTable/*<const gchar*,StoredWindow*>*/ * map;
};


static StoredWindow * _stored_window_new (const gchar * name, GtkWindow * window)
{
	StoredWindow * self = g_new(StoredWindow, 1);
	self->name = g_strdup (name);
	self->window = window;
	self->show_id = self->hide_id = self->destroy_id = 0;

	return self;
}

static void _stored_window_destroy (StoredWindow * self)
{
	VERBOSE(VERBOSE_PROGRESS, printf ("_stored_window_destroy: Window \"%s\"\n", self->name);)

	if (self->show_id != 0) g_signal_handler_disconnect(G_OBJECT(self->window), self->show_id);
	if (self->hide_id != 0) g_signal_handler_disconnect(G_OBJECT(self->window), self->hide_id);
	if (self->destroy_id != 0) g_signal_handler_disconnect(G_OBJECT(self->window), self->destroy_id);

	g_free (self->name);
	g_free (self);
}

/*!
 * Create a new \ref Workspace. This method should be called once.
 */
Workspace * workspace_new ()
{
	Workspace * self = g_new (Workspace, 1);
	self->window_group = gtk_window_group_new ();
	assert (self->window_group != NULL);
	self->map = g_hash_table_new_full(g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)_stored_window_destroy);
	return self;
}

/*!
 * Destroys the \ref Workspace, but doesn't destroy the windows itself.
 */
void workspace_destroy (Workspace * self)
{
	g_object_unref (G_OBJECT(self->window_group));
	g_hash_table_destroy(self->map);
	g_free (self);
}


/*!
 * Returns the \ref StoredWindow associated with the given \ref GtkWindow.
 */
StoredWindow * _workspace_get_stored_by_ptr (Workspace * self, gpointer * window)
{
	GHashTableIter iter = {0};
	const gchar * key;
	StoredWindow * stored = NULL;

	g_hash_table_iter_init (&iter, self->map);
	while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&stored)) {
		if ((gpointer)stored->window == window)
			return stored;
	}
	return NULL;
}

/*!
 * Returns TRUE if the given rectangle could correspond to a valid window area
 * inside the given screen. A tolerance can be set to allow the window to
 * slightly reach outside the screen.
 */
static gboolean _is_valid_rc (GdkScreen * screen, GdkRectangle * rc, gint tolerance)
{
	gboolean is_valid = TRUE;

	/* The window is allowed to slightly range beyond the screen. */
	is_valid = (is_valid && rc->x > -tolerance);
	is_valid = (is_valid && rc->y > -tolerance);

	if (screen) {
		gint x_overlap = rc->width - (gdk_screen_get_width(screen) - rc->x);
		gint y_overlap = rc->height - (gdk_screen_get_height(screen) - rc->y);

		is_valid = (is_valid && x_overlap <= tolerance && y_overlap <= tolerance);
	}

	return is_valid;
}


static gboolean _prefs_explode_window_info (const char * s,
                                       int * pleft, int * ptop,
                                       int * pwidth, int * pheight,
                                       int * pvisible)
{
	return (5 == sscanf (s, "left %d, top %d, width %d, height %d, visible %d",
			pleft, ptop, pwidth, pheight, pvisible));
}

void _on_hide (GtkWidget * window, Workspace * self)
{
	/* Update the window position. The size doesn't need to be saved. */
	StoredWindow * stored = _workspace_get_stored_by_ptr (self, (gpointer)window);
	assert (stored != NULL);
	gdk_window_get_root_origin (window->window, &stored->rc.x, &stored->rc.y);
}

void _on_show (GtkWidget * window, Workspace * self)
{
	/* Restore the window position. */
	StoredWindow * stored = _workspace_get_stored_by_ptr (self, (gpointer)window);
	assert (stored != NULL);

	gtk_window_move (GTK_WINDOW(window), stored->rc.x, stored->rc.y);
}

gboolean _on_destroy (GtkWidget * widget, GdkEvent * event, Workspace * self)
{
	StoredWindow * stored = _workspace_get_stored_by_ptr (self, (gpointer)widget);
	if (stored) {
		g_hash_table_remove(self->map, stored->name);
	}

	return FALSE; /*propagate*/
}

/*!
 * In order to use this function to initially restore the window correct,
 * it must not be shown before.
 */
void workspace_add_window (Workspace * self, GtkWindow * window, const gchar * key)
{
	/* Ensure the GdkWindow object is valid. Otherwise we run into problems
	 * when saving window dimensions if a window never showed up. */
	gtk_widget_realize (GTK_WIDGET(window));

	/* We don't distinguish between the same window or another window. Both
	 * situations should be avoided. */
	if (g_hash_table_lookup(self->map, key)) {
		g_warning ("workspace_add_window: There is a window with name \"%s\" already.", key);
	}
	else {
		StoredWindow * stored = _stored_window_new (key, (gpointer)window);
		gchar * value;

		gtk_window_group_add_window (self->window_group, window);
		g_hash_table_insert(self->map, stored->name, stored);

		value = prefs_get_string("windows"/*section*/, key);
		if (!value) {
			/* If a window is not registered and is never shown, we could save
			 * garbage in the preferences if we wouldn't initialize the stored
			 * window here. */
			stored->rc.x = stored->rc.y = 0;
		}
		else if (*value == '\0') {
			/* Field is empty. */
			stored->rc.x = stored->rc.y = 0;
		}
		else {
			/* Restore the size from the preferences. */
			gboolean ret = _prefs_explode_window_info(value, &stored->rc.x, &stored->rc.y,
					&stored->rc.width, &stored->rc.height, &stored->is_visible_by_def);

			if ( !ret || ! _is_valid_rc (gtk_window_get_screen(window), &stored->rc, 10/*tolerance*/)) {
				GdkRectangle rc = stored->rc;

				g_warning ("workspace_add_window: "
						"Invalid stored information for window \"%s\": (x=%d,y=%d,w=%d,h=%d). "
						"Using default settings instead.", stored->name, rc.x, rc.y,
						rc.width, rc.height);

				stored->rc.x = stored->rc.y = 0;
				stored->rc.width = stored->rc.height = 0;
			}
			else {
				if (stored->rc.width > 0 || stored->rc.height > 0) {
					/* We have to set the size here. This is not done by the 'show' signal. */
					gtk_window_set_default_size (window, stored->rc.width, stored->rc.height);
				}

#if GTK_MINOR_VERSION >= 18
				if (gtk_widget_get_visible (GTK_WIDGET (window))) {
#else
				if (GTK_WIDGET_VISIBLE(GTK_WIDGET (window))) {
#endif
					g_warning("workspace_add_window: Window \"%s\" (%p) is visible already.",
							key, window);
				}
			}

			g_free(value);
		}

		/* Ensure that we are notified every time the visibility changes. See also
		 * the general remarks for positioning a window above. */
		stored->hide_id = g_signal_connect (G_OBJECT(window), "hide",
				GTK_SIGNAL_FUNC(_on_hide), self);
		stored->show_id = g_signal_connect (G_OBJECT(window), "show",
				GTK_SIGNAL_FUNC(_on_show), self);

		/* Remove dying windows. gtk_widget_set_events cannot be used here
		 * since the widget is usually realized at this point. */
		gtk_widget_add_events (GTK_WIDGET(window), GDK_STRUCTURE_MASK);
		stored->destroy_id = g_signal_connect (G_OBJECT(window), "destroy-event",
				GTK_SIGNAL_FUNC(_on_destroy), self);
	}
}

/*!
 * Returns a list of all windows in - visible or not - the \ref Workspace. The
 * returned list contains \ref GtkWindow objects and must be freed if no
 * longer needed.
 */
GList/*<GtkWindow*>*/ * workspace_get_windows(Workspace * self)
{
	return gtk_window_group_list_windows(self->window_group);
}


/*!
 * Returns if the given visible is visible by default. The result doesn't
 * change during the program lifetime.
 */
gboolean workspace_is_window_visible_by_default (Workspace * self, GtkWindow * window)
{
	StoredWindow * stored = _workspace_get_stored_by_ptr (self, (gpointer)window);
	if (!stored) {
		g_warning ("workspace_is_window_visible_by_default: "
				"GtkWindow %p not registered. Returning FALSE.", window);
		return FALSE;
	}
	else return stored->is_visible_by_def;
}


static void _stored_window_update (StoredWindow * self, Workspace * workspace)
{
	GdkWindow * gdk_window = GTK_WIDGET(self->window)->window;

	/* In case the window is unrealized, ignore this call. */
	if (gdk_window) {
		gboolean is_visible;

#if GTK_MINOR_VERSION >= 18
		is_visible = gtk_widget_get_visible (GTK_WIDGET(self->window));
#else
		is_visible = GTK_WIDGET_VISIBLE (GTK_WIDGET(self->window));
#endif

		/* If the window is visible, there is a chance that the user has moved it
		 * so the position is now invalid. Since in this case, obtaining the
		 * position is not a problem, update it now. To be consistent, use the
		 * GDK routine again. */
		if (is_visible) {
			gdk_window_get_root_origin (GTK_WIDGET(self->window)->window,
					&self->rc.x, &self->rc.y);
		}

		gdk_window_get_size (GTK_WIDGET(self->window)->window, &self->rc.width,
				&self->rc.height);
	}
	else {
		/* We use the values which has been loaded when the windows was added
		 * to the workspace. */
	}
}


/*!
 * Saves the workspace to the preferences.
 */
void workspace_save (Workspace * self)
{
	GHashTableIter iter = {0};
	const gchar * key;
	StoredWindow * stored = NULL;

	g_hash_table_iter_init (&iter, self->map);
	while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&stored)) {
		gboolean is_visible;
		gchar * value;

#if GTK_MINOR_VERSION >= 18
		is_visible = gtk_widget_get_visible (GTK_WIDGET(stored->window));
#else
		is_visible = GTK_WIDGET_VISIBLE (GTK_WIDGET(stored->window));
#endif

		_stored_window_update (stored, self);

		if (stored->rc.width <= 1 || stored->rc.height <= 1) {
			/* Do not save the window settings if they are invalid. */
			verbose_printf (VERBOSE_DEBUG, __FILE__": workspace_save: Window "
					"\"%s\" has invalid size.\n", stored->name);

			prefs_set_string ("windows", stored->name, "");
		}
		else {
			value = g_strdup_printf ("left %d, top %d, width %d, height %d, visible %d",
					stored->rc.x, stored->rc.y,
					stored->rc.width, stored->rc.height, is_visible);

			verbose_printf (VERBOSE_DEBUG, __FILE__": workspace_save: Saving %s"
					"window \"%s\" with values:\n\t%s\n",
					GTK_WIDGET(stored->window)->window?"":"unrealized ", stored->name, value);

			prefs_set_string ("windows", stored->name, value);

			g_free (value);
		}
	}
}

