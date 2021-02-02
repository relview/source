/*
 * Namespace.c
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

#include "Namespace.h"
#include <stdio.h>

/*!
 * A \ref Namespace can be used to store object which share the same space of
 * names. Inside \ref Relview it is used to avoid overlapping names of
 * relations, functions, programs and domain, because inside LUA they actually
 * share the same namespace.
 */

typedef /*private*/ struct _Entry
{
	gpointer obj;
	const NamedProfile * profile;
} Entry;

struct _Namespace
{
	GHashTable/*<gchar*,Entry*>*/ * map;

	NamespaceFilterFunc filter_func;
	gpointer filter_user_data;

	gint ref_count;
};

Namespace *	namespace_new_full (GEqualFunc str_equal_func)
{
	Namespace * self = g_new0 (Namespace,1);

	if (str_equal_func == NULL)
		str_equal_func = g_str_equal;
	self->map = g_hash_table_new_full (g_str_hash, str_equal_func,
			(GDestroyNotify)g_free, (GDestroyNotify)g_free);

	namespace_set_filter_func (self, NULL, NULL);

	self->ref_count = 0;

	return self;
}

Namespace * namespace_new ()
{
	return namespace_new_full(g_str_equal);
}

void namespace_ref (Namespace * self) { self->ref_count ++; }

void namespace_unref (Namespace * self)
{
	self->ref_count--;
	if (self->ref_count <= 0) {
		namespace_destroy (self);
	}
}

void namespace_destroy (Namespace * self)
{
	if (self->ref_count > 0)
		g_warning ("namespace_destroy: Namespace %p has non-zero (%d) "
				"reference count.", self, self->ref_count);
	g_hash_table_destroy(self->map);
	g_free (self);
}

gboolean namespace_set_filter_func (Namespace * self,
		NamespaceFilterFunc func, gpointer user_data)
{
	/* If the new pattern is non-NULL, check all names currently in the
	 * Namespace whether they match it. If not, leave the current pattern
	 * unchanged. */
	if (NULL != func) {
		GHashTableIter iter = {0};
		const gchar * name;
		Entry * ent;

		g_hash_table_iter_init (&iter, self->map);
		while (g_hash_table_iter_next (&iter, (gpointer*)&name, (gpointer*)&ent)) {
			if ( !func(name, user_data))
				return FALSE;
		}
	}

	self->filter_func = func;
	if (func)
		self->filter_user_data = user_data;
	else
		self->filter_user_data = NULL;

	return TRUE;
}

gboolean namespace_is_valid_name (Namespace * self, const gchar * name)
{
	if ( !self->filter_func) return TRUE;
	else return self->filter_func(name, self->filter_user_data);
}

GList/*<const gchar*>*/ * namespace_get_names (const Namespace * self)
{
	return g_hash_table_get_keys(self->map);
}

gboolean namespace_add (Namespace * self, gpointer obj, const NamedProfile * profile)
{
	if (!obj || !profile) return FALSE;
	else {
		const gchar * name = profile->getName(obj);

		if ( !namespace_is_valid_name (self, name)) return FALSE;
		else if (namespace_is_in (self, name)) {
			return FALSE;
		}
		else {
			Entry * ent = g_new0 (Entry, 1);

			ent->obj = obj;
			ent->profile = profile;

			g_hash_table_insert(self->map, (gpointer)g_strdup(name), (gpointer)ent);
			return TRUE;
		}
	}
}

static gchar * _namespace_get_name_by_obj (Namespace * self, gpointer obj)
{
	GHashTableIter iter = {0};
	gchar * name;
	Entry * ent;

	g_hash_table_iter_init (&iter, self->map);
	while (g_hash_table_iter_next (&iter, (gpointer*)&name, (gpointer*)&ent)) {
		if (ent->obj == obj)
			return name;
	}
	return NULL;
}

void namespace_remove (Namespace * self, gpointer obj)
{
	const gchar * name = _namespace_get_name_by_obj (self, obj);
	if (name)
		namespace_remove_by_name (self, name);
}

void namespace_remove_by_name (Namespace * self, const gchar * name)
{
	g_hash_table_remove (self->map, (gpointer) name);
}

gboolean namespace_is_in (Namespace * self, const gchar * name)
{
	if (!name) return FALSE;
	else return (NULL != g_hash_table_lookup(self->map, name));
}

/*!
 * The object's name has been changed. Returns FALSE if the changed name
 * is already in the \ref Namespace. In that case, the object is removed to
 * maintain consistency, i.e. unique names.
 *
 * If FALSE is returned, the object is no longer in the namespace.
 */
gboolean namespace_name_changed (Namespace * self, gpointer obj)
{
	gchar * old_name = _namespace_get_name_by_obj (self, obj);

	/* It wasn't in the namespace. */
	if (!old_name) return FALSE;
	else {
		Entry * ent = g_hash_table_lookup (self->map, old_name);
		gboolean success;

		g_hash_table_steal(self->map, old_name);
		g_free (old_name);

		success = namespace_add(self, ent->obj, ent->profile);
		g_free (ent);

		return success;
	}
}


gpointer namespace_get_by_name (Namespace * self, const gchar * name)
{
	Entry * ent = g_hash_table_lookup(self->map, name);
	if (ent) return ent->obj;
	else return NULL;
}
