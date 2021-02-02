/*
 * compute.c
 *
 *  Copyright (C) 2008,2009,2010 Stefan Bolus, University of Kiel, Germany
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

#include "Relation.h"

#include "Relview.h" // rv_error_domain, Namespace
#include "Domain.h"
#include "Observer.h" // _0, _1, ...

#include <stdio.h>
#include <malloc.h>
#include <string.h>

#define ObjManager DomManager
#define Obj Dom

/*!
 * NamedProfile adapters are used for \ref Namespace objects to provide a
 * unified interface to the managed objects.
 */
static NamedProfile _dom_named_profile = { (NamedProfile_getNameFunc)dom_get_name };

#define DOM_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,DomManagerObserver,obj,func, __VA_ARGS__)

struct _DomManager
{
	GHashTable/*<gchar*,Dom*>*/ * doms;

	GSList/*<DomManagerObserver*>*/ * observers;
	gint block_notify;

	Namespace * ns;
};

/*!
 * Domains are types. It's actual size is computed at demand.
 * Thus, it cannot associated to a KureDomain which is supposed
 * to represent a fixed type.
 */
struct _Domain
{
// private:
	RvObjectClass * c; /*!< Must be the first member. */

	DomManager * manager; /*!< Can be NULL */

	gchar *name;
	gchar *comp1, *comp2; // Relview-Code
	DomainType type;

	/*! Lua cannot evaluate things like "x" or "r". To overcome this special
	 * case on can use a chunk of the form "return <expr>" which does the job.
	 */
	gchar *comp1_luacode, *comp2_luacode;

	gboolean is_hidden;
};

static RvObjectClass * _dom_class ();

void _dom_destroy (RvObject * self)
{
	if (rv_object_type_check (self, _dom_class())) {
		dom_destroy ((Domain*)self);
	}
}

static const gchar * _dom_get_name (RvObject * self)
{
	if (rv_object_type_check (self, _dom_class()))
		return dom_get_name((Dom*)self);
	else return NULL;
}

RvObjectClass * _dom_class ()
{
	static RvObjectClass * c = NULL;
	if (!c) {
		c = g_new0 (RvObjectClass,1);

		c->getName = _dom_get_name;
		c->destroy = _dom_destroy;
		c->type_name = g_strdup ("DomainClass");
	}

	return c;
}


struct _DomIterator
{
	GHashTableIter iter;
	gboolean is_valid;
	Dom * cur;
};


static void _dom_dtor (Dom * self);

DomManager * dom_manager_get_instance ()
{
	g_warning ("dom_manager_get_instance() should not be used anymore.\n");
	return rv_get_dom_manager (rv_get_instance());
}

/*!
 * This is the GDestroyFunc for the manager's hash table. See
 * \ref dom_manager_new_with_namespace. Must not be called if
 * the object was removed from the hash table already,
 */
static void _dom_manager_destroy_dom (Dom * obj)
{
	DomManager * self = dom_get_manager(obj);
	namespace_remove_by_name(dom_manager_get_namespace(self), dom_get_name(obj));
	obj->manager = NULL;
	dom_destroy (obj);
}

DomManager * dom_manager_new_with_namespace (Namespace * ns)
{
	DomManager * self = g_new0 (DomManager,1);

	self->doms = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify) _dom_manager_destroy_dom);
	self->observers = NULL;
	self->block_notify = 0;

	if (NULL == ns) {
		/* Use local namespace for this manager. */
		self->ns = namespace_new ();
	}
	else self->ns = ns;

	namespace_ref (self->ns);

	return self;
}

DomManager * dom_manager_new ()
{
	return dom_manager_new_with_namespace (NULL);
}


void dom_manager_destroy (DomManager * self)
{
	DOM_MANAGER_OBSERVER_NOTIFY(self,onDelete,_0());

	dom_manager_block_notify (self);
	g_hash_table_destroy (self->doms);
	g_slist_free (self->observers);
	dom_manager_unblock_notify (self);

	dom_manager_changed (self);

	namespace_unref (self->ns);

	g_free (self);
}

gboolean dom_manager_insert (DomManager * self, Dom * d)
{
	if (dom_get_manager (d)) {
		g_warning ("dom_manager_insert: Domain \"%s\" already belongs "
				"to a manager. Not inserted!", dom_get_name(d));
		return FALSE;
	}
	else if (namespace_insert(self->ns, (gpointer)d, &_dom_named_profile)) {
		g_hash_table_insert (self->doms, g_strdup(dom_get_name(d)), d);
		d->manager = self;
		dom_manager_changed (self);
		return TRUE;
	}
	else return FALSE;
}

Dom * dom_manager_get_by_name (DomManager * self, const gchar * name) {
	return g_hash_table_lookup (self->doms, name); }

gboolean dom_manager_delete_by_name (DomManager * self, const gchar * name)
{
	gboolean ret = g_hash_table_remove (self->doms, name);
	if (ret)
		dom_manager_changed (self);
	return ret;
}

GList/*<const gchar*>*/ * dom_manager_get_names (DomManager * self) {
	return g_hash_table_get_keys (self->doms);
}

void dom_manager_steal (DomManager * self, Dom * d)
{
	if (self != dom_get_manager(d)) return;
	else if (g_hash_table_steal (self->doms, dom_get_name(d))) {
		namespace_remove_by_name(self->ns, dom_get_name(d));
		d->manager = NULL;
		dom_manager_changed (self);
	}
}

gint dom_manager_steal_all (DomManager * self, DomManager * victim)
{
	GList/*<const gchar*>*/ * names = dom_manager_get_names (victim), *iter;
	gint stolenCount = 0;

	dom_manager_block_notify(self);
	dom_manager_block_notify(victim);

	for (iter = names ; iter ; iter = iter->next) {
		const gchar * name = (const gchar*) iter->data;
		if (! dom_manager_exists (self, name)) {
			Dom * obj = dom_manager_get_by_name (victim, name);
			dom_manager_steal (victim, obj);

			/* If we cannot insert the relation into the new \ref DomManager,
			 * push it back to the victim. */
			if ( !dom_manager_insert (self, obj))
				g_assert(dom_manager_insert (victim, obj));
			else
				stolenCount ++;
		}
	}

	dom_manager_unblock_notify(victim);
	dom_manager_unblock_notify(self);

	dom_manager_changed(victim);
	dom_manager_changed(self);

	g_list_free(names);
	return stolenCount;
}

gboolean dom_manager_exists (DomManager * self, const gchar * name)
{ return g_hash_table_lookup (self->doms, name) != NULL; }

void dom_manager_register_observer (DomManager * self, DomManagerObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void dom_manager_unregister_observer (DomManager * self, DomManagerObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }


void dom_manager_changed (DomManager * self)
{
	DOM_MANAGER_OBSERVER_NOTIFY(self,changed,_0());
}

void dom_manager_block_notify (DomManager * self)
{ self->block_notify ++; }

void dom_manager_unblock_notify (DomManager * self)
{ self->block_notify = MAX(0, self->block_notify-1); }

gboolean dom_manager_is_empty (DomManager * self) { return g_hash_table_size(self->doms) == 0; }
gint dom_manager_size (DomManager * self) { return (gint) g_hash_table_size(self->doms); }
gint dom_manager_delete_with_filter (DomManager * self,
		gboolean (*filter) (Dom*,gpointer), gpointer user_data)
{
	GHashTableIter iter;
	gint deleteCount = 0;
	Dom * cur;

	dom_manager_block_notify(self);
	g_hash_table_iter_init(&iter, self->doms);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer*)&cur)) {
		if (filter(cur,user_data)) {
			g_hash_table_iter_remove (&iter);
			deleteCount ++;
		}
	}
	dom_manager_unblock_notify(self);

	if (deleteCount > 0)
		dom_manager_changed(self);

	return deleteCount;
}

Namespace * dom_manager_get_namespace (DomManager * self) { return self->ns; }

DomIterator * dom_manager_iterator (DomManager * self)
{
	DomIterator * iter = g_new0 (DomIterator,1);
	g_hash_table_iter_init (&iter->iter, self->doms);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}
void dom_iterator_next (DomIterator * self) {
	self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }
gboolean dom_iterator_is_valid (DomIterator * self) { return self->is_valid; }
Dom * dom_iterator_get (DomIterator * self) { return self->cur; }
void dom_iterator_destroy (DomIterator * self) { g_free (self); }

static gchar * _to_lua_anon_func (const char * expr, GError ** perr)
{
	char * luacode;
	KureError * kerr = NULL;

	luacode = kure_lang_expr_to_lua (expr, &kerr);
	if (!luacode) {
		g_set_error_literal(perr, rv_error_domain(), 0, kerr->message);
		kure_error_destroy(kerr);
		return NULL;
	}
	else {
		gchar * ret = g_strconcat ("return ", luacode, NULL);
		free (luacode);

		return ret;
	}
}

Dom * dom_new (const gchar * name, DomainType type,
			const gchar * first_comp, const gchar * second_comp, GError ** perr)
{
	if (!name || !first_comp || !second_comp) {
		g_set_error (perr, rv_error_domain(), 0, "dom_new: Invalid argument(s).");
		return NULL;
	}
	else {
		Dom * self = g_new0 (Dom,1);

		self->c = _dom_class();

		self->comp1_luacode = _to_lua_anon_func (first_comp, perr);
		if ( !self->comp1_luacode) {
			g_prefix_error(perr, "In the first component of domain \"%s\".\n"
					"Component was \"%s\".\n", name, first_comp);
			g_free (self);
			return NULL;
		}
		self->comp2_luacode = _to_lua_anon_func(second_comp, perr);
		if ( !self->comp2_luacode) {
			g_prefix_error(perr, "In the second component of domain \"%s\".\n"
					"Component was \"%s\".\n", name, second_comp);
			g_free (self->comp1_luacode);
			g_free (self);
			return NULL;
		}

		self->name        = g_strdup (name);
		self->comp1       = g_strdup (first_comp);
		self->comp2       = g_strdup (second_comp);
		self->type        = type;
		self->is_hidden = FALSE;

		VERBOSE(VERBOSE_INFO, printf ("_Dom (from def) \"%s\"_:___\n"
				"\tcomp1 : \"%s\" (lua: \"%s\")\n"
				"\tcomp2 : \"%s\" (lua: \"%s\")\n"
				"\ttype  : %d\n",
				self->name, self->comp1, self->comp1_luacode, self->comp2,
				self->comp2_luacode, self->type);)

		return self;
	}
}

Dom * dom_move (Dom * self, Dom * src)
{
	DomManager * manager = src->manager;
	self->manager = NULL;
	_dom_dtor (self);
	*self = *src;
	self->manager = manager;

	g_free (src);
	return self;
}

void _dom_dtor (Dom * self)
{
	if (self->manager)
		dom_manager_steal (self->manager, self);

	g_free (self->name);
	g_free (self->comp1);
	g_free (self->comp2);
	g_free (self->comp1_luacode);
	g_free (self->comp2_luacode);
}

void dom_destroy (Dom * self) {
	_dom_dtor (self);
	g_free (self);
	/* TODO: Don't know how to destroy the rest. */
}

DomManager * dom_get_manager (Dom * self) { return self->manager; }
const gchar * dom_get_name (const Dom * self) { return self->name; }
gboolean dom_is_hidden (const Dom * self) { return self->is_hidden; }
void dom_set_hidden (Dom * self, gboolean yesno) { self->is_hidden = yesno; }
gchar * dom_get_term (const Dom * self)
{
	switch (self->type) {
	case DIRECT_PRODUCT:
		return g_strconcat (self->comp1, " X ", self->comp2, NULL);
	case DIRECT_SUM:
		return g_strconcat (self->comp1, " + ", self->comp2, NULL);
	default:
		g_warning ("dom_get_term: Domain \"%s\" has an unknown type: %d.\n",
				dom_get_name(self), self->type);
		return g_strconcat (self->comp1, " (unknown) ", self->comp2, NULL);
	}
}

const gchar *	dom_get_first_comp (const Dom * self) { return self->comp1; }
const gchar *	dom_get_second_comp (const Dom * self) { return self->comp2; }

const gchar *	dom_get_first_comp_lua (const Dom * self) { return self->comp1_luacode; }
const gchar *	dom_get_second_comp_lua (const Dom * self) { return self->comp2_luacode; }

DomainType		dom_get_type (const Dom * self) { return self->type; }

