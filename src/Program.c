/*
 * Program.c
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

#include "Program.h"

#include "RelviewTypes.h"
#include "Function.h"
#include "Domain.h"
#include "Observer.h"

#include "Kure.h"
#include "Relview.h"

#include <string.h> // strlen

/*!
 * NamedProfile adapters are used for \ref Namespace objects to provide a
 * unified interface to the managed objects.
 */
static NamedProfile _prog_named_profile = { (NamedProfile_getNameFunc)prog_get_name };

struct _Prog
{
// private:
	RvObjectClass * c; /*!< Must be the first member. */

	ProgManager * manager;

	gchar *name;
	gchar *sig; /*!< The prog.'s signature. e.g. "f(x,y)". */
	gchar *def;
	gint num_args;

	gchar * luacode;

	gboolean is_hidden;
};


static RvObjectClass * _prog_class ();

void _prog_destroy (RvObject * self)
{
	if (rv_object_type_check (self, _prog_class())) {
		prog_destroy ((Prog*)self);
	}
}

static const gchar * _prog_get_name (RvObject * self)
{
	if (rv_object_type_check (self, _prog_class()))
		return prog_get_name((Prog*)self);
	else return NULL;
}

RvObjectClass * _prog_class ()
{
	static RvObjectClass * c = NULL;
	if (!c) {
		c = g_new0 (RvObjectClass,1);

		c->getName = _prog_get_name;
		c->destroy = _prog_destroy;
		c->type_name = g_strdup ("ProgramClass");
	}

	return c;
}


static gchar * _get_prog_def (const gchar * prog_term);


#define PROG_MANAGER_OBSERVER_NOTIFY(obj,func,...) \
        OBSERVER_NOTIFY(observers,GSList,ProgManagerObserver,obj,func, __VA_ARGS__)

struct _ProgManager
{
	GHashTable/*<gchar*,Prog*>*/ * objs;

	GSList/*<ProgManagerObserver*>*/ * observers;
	gint block_notify;

	Namespace * ns;
};

struct _ProgIterator
{
	GHashTableIter iter;
	gboolean is_valid;
	Prog * cur;
};

static void _prog_dtor (Prog * self);

ProgManager * prog_manager_get_instance ()
{
	return rv_get_prog_manager(rv_get_instance());
}


/*!
 * This is the GDestroyFunc for the manager's hash table. See
 * \ref prog_manager_new_with_namespace. Must not be called if
 * the object was removed from the hash table already,
 */
static void _prog_manager_destroy_prog (Prog * obj)
{
	ProgManager * self = prog_get_manager(obj);
	namespace_remove_by_name(prog_manager_get_namespace(self), prog_get_name(obj));
	obj->manager = NULL;
	prog_destroy (obj);
}

ProgManager * prog_manager_new_with_namespace (Namespace * ns)
{
	ProgManager * self = g_new0 (ProgManager,1);
	self->objs = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify) _prog_manager_destroy_prog);
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

ProgManager * prog_manager_new () { return prog_manager_new_with_namespace(NULL); }

void prog_manager_destroy (ProgManager * self)
{
	PROG_MANAGER_OBSERVER_NOTIFY(self,onDelete,_0());

	prog_manager_block_notify (self);
	g_hash_table_destroy (self->objs);
	g_slist_free (self->observers);
	prog_manager_unblock_notify (self);

	prog_manager_changed (self);
}

gboolean prog_manager_insert (ProgManager * self, Prog * d)
{
	if (prog_get_manager (d)) {
		g_warning ("prog_manager_insert: Domain \"%s\" already belongs "
				"to a manager. Not inserted!", prog_get_name(d));
		return FALSE;
	}
	else if (namespace_insert(self->ns, (gpointer)d, &_prog_named_profile)) {
		g_hash_table_insert (self->objs, g_strdup(prog_get_name(d)), d);
		d->manager = self;
		prog_manager_changed (self);
		return TRUE;
	}
	else return FALSE;
}

Prog * prog_manager_get_by_name (ProgManager * self, const gchar * name) {
	return g_hash_table_lookup (self->objs, name); }

gboolean prog_manager_delete_by_name (ProgManager * self, const gchar * name)
{
	gboolean ret = g_hash_table_remove (self->objs, name);
	if (ret)
		prog_manager_changed (self);
	return ret;
}

GList/*<const gchar*>*/ * prog_manager_get_names (ProgManager * self) {
	return g_hash_table_get_keys (self->objs); }

void prog_manager_steal (ProgManager * self, Prog * d)
{
	if (self != prog_get_manager(d)) return;
	else if (g_hash_table_steal (self->objs, prog_get_name(d))) {
		d->manager = NULL;
		prog_manager_changed (self);
	}
}

gint prog_manager_steal_all (ProgManager * self, ProgManager * victim)
{
	GList/*<const gchar*>*/ * names = prog_manager_get_names (victim), *iter;
	gint stolenCount = 0;

	prog_manager_block_notify(self);
	prog_manager_block_notify(victim);

	for (iter = names ; iter ; iter = iter->next) {
		const gchar * name = (const gchar*) iter->data;
		if (! prog_manager_exists (self, name)) {
			Prog * obj = prog_manager_get_by_name (victim, name);
			prog_manager_steal (victim, obj);

			/* If we cannot insert the relation into the new \ref DomManager,
			 * push it back to the victim. */
			if ( !prog_manager_insert (self, obj))
				g_assert(prog_manager_insert (victim, obj));
			else
				stolenCount ++;
		}
	}

	prog_manager_unblock_notify(victim);
	prog_manager_unblock_notify(self);

	prog_manager_changed(victim);
	prog_manager_changed(self);

	g_list_free(names);
	return stolenCount;
}

gboolean prog_manager_exists (ProgManager * self, const gchar * name)
{ return g_hash_table_lookup (self->objs, name) != NULL; }

void prog_manager_register_observer (ProgManager * self, ProgManagerObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void prog_manager_unregister_observer (ProgManager * self, ProgManagerObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }


void prog_manager_changed (ProgManager * self) {
	PROG_MANAGER_OBSERVER_NOTIFY(self,changed,_0()); }

void prog_manager_block_notify (ProgManager * self)
{ self->block_notify ++; }

void prog_manager_unblock_notify (ProgManager * self)
{ self->block_notify = MAX(0, self->block_notify-1); }

gboolean prog_manager_is_empty (ProgManager * self) { return g_hash_table_size(self->objs) == 0; }
gint prog_manager_size (ProgManager * self) { return (gint) g_hash_table_size(self->objs); }
gint prog_manager_delete_with_filter (ProgManager * self,
		gboolean (*filter) (Prog*,gpointer), gpointer user_data)
{
	GHashTableIter iter;
	gint deleteCount = 0;
	Prog * cur;

	prog_manager_block_notify(self);
	g_hash_table_iter_init(&iter, self->objs);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer*)&cur)) {
		if (filter(cur,user_data)) {
			g_hash_table_iter_remove (&iter);
			deleteCount ++;
		}
	}
	prog_manager_unblock_notify(self);

	if (deleteCount > 0)
		prog_manager_changed(self);

	return deleteCount;
}

Namespace *	prog_manager_get_namespace (ProgManager * self) { return self->ns; }

ProgIterator * prog_manager_iterator (ProgManager * self)
{
	ProgIterator * iter = g_new0 (ProgIterator,1);
	g_hash_table_iter_init (&iter->iter, self->objs);
	iter->is_valid = g_hash_table_iter_next (&iter->iter, NULL, (gpointer*) &iter->cur);
	return iter;
}
void prog_iterator_next (ProgIterator * self) {
	self->is_valid = g_hash_table_iter_next (&self->iter, NULL, (gpointer*)&self->cur); }
gboolean prog_iterator_is_valid (ProgIterator * self) { return self->is_valid; }
Prog * prog_iterator_get (ProgIterator * self) { return self->cur; }
void prog_iterator_destroy (ProgIterator * self) { g_free (self); }


/****************************************************************************/
/* NAME: get_prog_def                                                       */
/* FUNKTION: ermittelt aus dem kompletten progterm namen (.....)            */
/* UEBERGABEPARAMETER: char * (der progterm)                                */
/* RUECKGABEWERT: char * (name (ARg, ARg ....)                              */
/* ERSTELLT VON: Peter Schneider                                            */
/* AM: 05.03.96                                                             */
/* LETZTE AENDERUNG AM: 05.03.96                                            */
/****************************************************************************/
gchar * _get_prog_def (const gchar * prog_term)
{
	gchar * pattern = "[a-zA-Z_-][a-zA-Z0-9_-]*\\s*\\([^)]*\\)";
	GRegex * regex;
	GMatchInfo * match_info;
	gchar * ret = NULL;

	regex = g_regex_new(pattern, G_REGEX_MULTILINE, 0, NULL);
	g_assert (regex != NULL);
	if ( !g_regex_match(regex, prog_term, 0, &match_info)) {
		g_warning ("_get_prog_def: Unable to parse \"%s\" for "
				"program definition. Regex was \"%s\".", prog_term, pattern);
		ret = g_strdup ("(error)");
	}
	else {
		ret = g_match_info_fetch (match_info, 0);
		g_match_info_free(match_info);
	}
	g_regex_unref(regex);

	return ret;
}

// From Function.c
extern gchar * _fundef_extract_name (const char * def);
extern int _fundef_count_params (const char * def);


Prog * prog_new_with_lua (const gchar * def, const gchar * luacode, GError ** perr)
{
	Prog * self = g_new0 (Prog,1);
	self->c			= _prog_class();
	self->def       = g_strdup (def);
	self->is_hidden = FALSE;
	self->name      = _fundef_extract_name(def);
	self->num_args  = _fundef_count_params(def);
	self->sig 		= _get_prog_def(def);
	self->luacode   = g_strdup (luacode);

	return self;
}

Prog * prog_new  (const gchar * def, GError ** perr)
{
	KureError * err = NULL;
	char * luacode = kure_lang_to_lua (def, &err);
	if ( !luacode) {
		g_set_error(perr, 0,0, err->message);
		kure_error_destroy(err);
		return NULL;
	}
	else {
		Prog * self = prog_new_with_lua (def, luacode, perr);
		self->c = _prog_class();
		free (luacode);
		return self;
	}
}


void _prog_dtor (Prog * self)
{
	if (self->manager)
		prog_manager_steal (self->manager, self);

	g_free (self->name);
	g_free (self->def);
	g_free (self->sig);
	g_free (self->luacode);
}

void prog_destroy (Prog * self)
{
	_prog_dtor (self);
	g_free (self);
}

ProgManager * 	prog_get_manager (Prog * self) { return self->manager; }
const gchar * 	prog_get_name (const Prog * self) { return self->name; }
const gchar *   prog_get_signature (const Prog * self) { return self->sig; }
gboolean 		prog_is_hidden (const Prog * self) { return self->is_hidden; }
void 			prog_set_hidden (Prog * self, gboolean yesno) { self->is_hidden = yesno; }
const gchar *	prog_get_def (const Prog * self) { return self->def; }
gint			prog_get_arg_count (Prog * self) { return self->num_args; }


const gchar * prog_get_luacode (const Prog * self, size_t * psize)
{
	if (psize) *psize = strlen (self->luacode);
	return self->luacode;
}

void prog_dump (const Prog * self)
{
	printf ("Program \"%s\":\n"
			"\t     def : \"%s\"\n"
			"\t     sig : \"%s\"\n"
			"\t #params : %d\n"
			"\t  hidden : \"%s\"\n"
			"\t luacode : \"%s\"\n", self->name, self->def,
			self->sig, self->num_args, self->is_hidden ? "yes" : "no",
					self->luacode);
}
