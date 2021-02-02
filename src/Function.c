/*
 * Function.c
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

#include "Function.h"

#include "Relview.h"
#include "Observer.h" // _0, _1, ...

#include "Kure.h"
#include <string.h>
#include <ctype.h> // isspace

/*!
 * NamedProfile adapters are used for \ref Namespace objects to provide a
 * unified interface to the managed objects.
 */
static NamedProfile _fun_named_profile = { (NamedProfile_getNameFunc)fun_get_name };

#warning "NOTE: What happens if the function has changed name. Who notifies the namespace?"

/*!
 * Functions are defined in the RelView-language.
 */
struct _Function
{
// private:
	RvObjectClass * c; /*!< Must be the first member. */

	FunManager * manager;

	gchar * def; /*!< The complete definition, e.g. "f(x)=x." */
	gchar * name; /*!< The name, e.g. "f". */
	gchar * expr; /*!< The right-hand side of the '=', e.g. "x^". */

	gint num_args; /*!< The number of arguments. E.g. 2 for "f(x,y)=...". */

	gchar * luacode; /*!< The (maybe pre-compiled) Lua code. This is always a
	                  * function of the same name. */

	gboolean is_hidden;
};


static RvObjectClass * _fun_class ();

void _fun_destroy (RvObject * self)
{
	if (rv_object_type_check (self, _fun_class())) {
		fun_destroy ((Fun*)self);
	}
}

static const gchar * _fun_get_name (RvObject * self)
{
	if (rv_object_type_check (self, _fun_class()))
		return fun_get_name((Fun*)self);
	else return NULL;
}

RvObjectClass * _fun_class ()
{
	static RvObjectClass * c = NULL;
	if (!c) {
		c = g_new0 (RvObjectClass,1);

		c->getName = _fun_get_name;
		c->destroy = _fun_destroy;
		c->type_name = g_strdup ("FunctionClass");
	}

	return c;
}

#define FUN_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,FunManagerObserver,obj,func, __VA_ARGS__)

struct _FunManager
{
	GHashTable/*<gchar*,Fun*>*/ * objs;

	GSList/*<FunManagerObserver*>*/ * observers;
	gint block_notify;

	Namespace * ns;
};

struct _FunIterator
{
	GHashTableIter iter;
	gboolean is_valid;
	Fun * cur;
};

static void _fun_dtor (Fun * self);

FunManager * fun_manager_get_instance ()
{
	return rv_get_fun_manager(rv_get_instance());
}

/*!
 * This is the GDestroyFunc for the manager's hash table. See
 * \ref dom_manager_new_with_namespace. Must not be called if
 * the object was removed from the hash table already,
 */
static void _fun_manager_destroy_fun (Fun * obj)
{
	FunManager * self = fun_get_manager(obj);
	namespace_remove_by_name(fun_manager_get_namespace(self), fun_get_name(obj));
	obj->manager = NULL;
	fun_destroy (obj);
}

FunManager * fun_manager_new_with_namespace (Namespace * ns)
{
	FunManager * self = g_new0 (FunManager,1);
	self->objs = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify) _fun_manager_destroy_fun);
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

FunManager * fun_manager_new ()
{
	return fun_manager_new_with_namespace(NULL);
}

void fun_manager_destroy (FunManager * self)
{
	FUN_MANAGER_OBSERVER_NOTIFY(self,onDelete,_0());

	fun_manager_block_notify (self);
	g_hash_table_destroy (self->objs);
	g_slist_free (self->observers);
	fun_manager_unblock_notify (self);

	fun_manager_changed (self);

	namespace_unref (self->ns);
	g_free (self);
}

gboolean fun_manager_insert (FunManager * self, Fun * d)
{
	if (fun_get_manager (d)) {
		g_warning ("fun_manager_insert: Domain \"%s\" already belongs "
				"to a manager. Not inserted!", fun_get_name(d));
		return FALSE;
	}
	else if (namespace_insert(self->ns, (gpointer)d, &_fun_named_profile)) {
		g_hash_table_insert (self->objs, g_strdup(fun_get_name(d)), d);
		d->manager = self;
		fun_manager_changed (self);
		return TRUE;
	}
	else return FALSE;
}

Fun * fun_manager_get_by_name (FunManager * self, const gchar * name) {
	return g_hash_table_lookup (self->objs, name); }

gboolean fun_manager_delete_by_name (FunManager * self, const gchar * name)
{
	gboolean ret = g_hash_table_remove (self->objs, name);
	if (ret)
		fun_manager_changed (self);
	return ret;
}

GList/*<const gchar*>*/ * fun_manager_get_names (FunManager * self) {
	return g_hash_table_get_keys (self->objs); }

void fun_manager_steal (FunManager * self, Fun * d)
{
	if (self != fun_get_manager(d)) return;
	else if (g_hash_table_steal (self->objs, fun_get_name(d))) {
		namespace_remove_by_name(self->ns, fun_get_name(d));
		d->manager = NULL;
		fun_manager_changed (self);
	}
}

gint fun_manager_steal_all (FunManager * self, FunManager * victim)
{
	GList/*<const gchar*>*/ * names = fun_manager_get_names (victim), *iter;
	gint stolenCount = 0;

	fun_manager_block_notify(self);
	fun_manager_block_notify(victim);

	for (iter = names ; iter ; iter = iter->next) {
		const gchar * name = (const gchar*) iter->data;
		if (! fun_manager_exists (self, name)) {
			Fun * obj = fun_manager_get_by_name (victim, name);
			fun_manager_steal (victim, obj);

			/* If we cannot insert the relation into the new \ref DomManager,
			 * push it back to the victim. */
			if ( !fun_manager_insert (self, obj))
				g_assert(fun_manager_insert (victim, obj));
			else
				stolenCount ++;
		}
	}

	fun_manager_unblock_notify(victim);
	fun_manager_unblock_notify(self);

	fun_manager_changed(victim);
	fun_manager_changed(self);

	g_list_free(names);
	return stolenCount;
}

gboolean fun_manager_exists (FunManager * self, const gchar * name)
{ return g_hash_table_lookup (self->objs, name) != NULL; }

void fun_manager_register_observer (FunManager * self, FunManagerObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void fun_manager_unregister_observer (FunManager * self, FunManagerObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }


void fun_manager_changed (FunManager * self) {
	FUN_MANAGER_OBSERVER_NOTIFY(self,changed,_0()); }

void fun_manager_block_notify (FunManager * self)
{ self->block_notify ++; }

void fun_manager_unblock_notify (FunManager * self)
{ self->block_notify = MAX(0, self->block_notify-1); }

gboolean fun_manager_is_empty (FunManager * self) { return g_hash_table_size(self->objs) == 0; }
gint fun_manager_size (FunManager * self) { return (gint) g_hash_table_size(self->objs); }
gint fun_manager_delete_with_filter (FunManager * self,
		gboolean (*filter) (Fun*,gpointer), gpointer user_data)
{
	GHashTableIter iter;
	gint deleteCount = 0;
	Fun * cur;

	fun_manager_block_notify(self);
	g_hash_table_iter_init(&iter, self->objs);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer*)&cur)) {
		if (filter(cur,user_data)) {
			g_hash_table_iter_remove (&iter);
			deleteCount ++;
		}
	}
	fun_manager_unblock_notify(self);

	if (deleteCount > 0)
		fun_manager_changed(self);

	return deleteCount;
}

Namespace *	fun_manager_get_namespace (FunManager * self) { return self->ns; }

FunIterator * fun_manager_iterator (FunManager * self)
{
	FunIterator * iter = g_new0 (FunIterator,1);
	g_hash_table_iter_init (&iter->iter, self->objs);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}
void fun_iterator_next (FunIterator * self) {
	self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }
gboolean fun_iterator_is_valid (FunIterator * self) { return self->is_valid; }
Fun * fun_iterator_get (FunIterator * self) { return self->cur; }
void fun_iterator_destroy (FunIterator * self) { g_free (self); }


gchar * _fundef_extract_name (const char * def)
{
	/* The name begins at the first non-space character and ends with the
	 * mandatory '('. Any whitespace is ignored. */
	const char * brace = strchr (def, '(');
	if (! brace) return g_strdup ("unknown");
	else {
		gchar * name = g_strndup (def, brace - def);
		return g_strstrip (name);
	}
}

int _fundef_count_params (const char * def)
{
	/* The number of arguments is equal to the number of commas plus 1 between
	 * the opening and the closing brace. If there is no comma, no parameter is
	 * present iff. only whitespaces are found. */
	const char * po = strchr (def, '('), *pc = strchr (def, ')');
	if (!po || !pc) {
		g_warning ("_fundef_count_params: Unable to count parameters "
				"for def. \"%s\".", def);
		return 0;
	}
	else {
		int commas = 0;
		gboolean nonwhite = FALSE;
		const char * p = po + 1/*(*/;
		for ( ; p < pc ; ++p) {
			if (*p == ',') commas ++;
			else if (!isspace(*p)) nonwhite = TRUE;
		}
		if (commas > 0) return commas + 1;
		else if (nonwhite) return 1;
		else return 0;
	}
}

static gchar * _fundef_extract_expr (const char * def)
{
	/* The expr begins after the equation sign and doesn't include the
	 * trailing dot. */
	const char * peq = strchr (def, '='), *pdot = strrchr (def, '.'), *pexpr;
	if (!peq || !pdot) {
		g_warning ("_fundef_extract_expr: Unable to recognize the expr "
				"of def. \"%s\".", def);
		return g_strdup ("unknown");
	}
	else {
		pexpr = peq+1;
		return g_strstrip (g_strndup (pexpr, pdot-pexpr));
	}
}

Fun * fun_new_with_lua (const gchar * def, const gchar * luacode, GError ** perr)
{
	Fun * self = g_new0 (Fun,1);
	self->c			= _fun_class();
	self->def       = g_strdup (def);
	self->is_hidden = FALSE;
	self->name      = _fundef_extract_name(def);
	self->num_args  = _fundef_count_params(def);
	self->expr      = _fundef_extract_expr(def);
	self->luacode   = g_strdup (luacode);

	return self;
}

Fun * fun_new  (const gchar * _def, GError ** perr)
{
	KureError * err = NULL;
	gsize len = strlen (_def);
	gchar * def = g_strstrip (g_strndup (_def, len+2/*'.'*/));
	char * luacode;
	Fun * self = NULL;

	len = strlen (def);
	if ( def[len-1] != '.') def[len] = '.';

	luacode = kure_lang_to_lua (def, &err);
	if ( !luacode) {
		g_set_error(perr, 0,0, err->message);
		kure_error_destroy(err);
	}
	else {
		self = fun_new_with_lua (def, luacode, perr);
		free (luacode);
	}

	fun_dump(self);

	g_free (def);
	return self;
}


void fun_dump (const Fun * self)
{
	printf ("Function \"%s\":\n"
			"\t     def : \"%s\"\n"
			"\t    expr : \"%s\"\n"
			"\t #params : %d\n"
			"\t  hidden : \"%s\"\n"
			"\t luacode : \"%s\"\n", self->name, self->def,
			self->expr, self->num_args, self->is_hidden ? "yes" : "no",
					self->luacode);
}

void _fun_dtor (Fun * self)
{
	if (self->manager)
		fun_manager_steal (self->manager, self);

	g_free (self->def);
	g_free (self->name);
	g_free (self->expr);
	g_free (self->luacode);
}

void fun_destroy (Fun * self)
{
	_fun_dtor (self);
	g_free (self);
}

FunManager * 	fun_get_manager (Fun * self) { return self->manager; }
const gchar * 	fun_get_name (const Fun * self) { return self->name; }
const gchar * 	fun_get_term (const Fun * self) { return self->def; }
const gchar * 	fun_get_def (const Fun * self) { return self->def; }
const gchar *	fun_get_expr (const Fun * self) { return self->expr; }
gint			fun_get_arg_count (Fun * self) { return self->num_args; }
gboolean 		fun_is_hidden (const Fun * self) { return self->is_hidden; }
void 			fun_set_hidden (Fun * self, gboolean yesno) { self->is_hidden = yesno; }

const gchar * fun_get_luacode (const Fun * self, size_t * psize)
{
	if (psize) *psize = strlen (self->luacode);
	return self->luacode;
}


