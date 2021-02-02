/*
 * Program.h
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

#ifndef PROGRAM_H_
#define PROGRAM_H_

#include "RelviewTypes.h"
#include "Function.h"
#include "Domain.h"

typedef struct _Prog Prog;
typedef struct _ProgManager ProgManager;
typedef struct _ProgManagerObserver ProgManagerObserver;
typedef struct _ProgIterator ProgIterator;


typedef void (*ProgManagerObserver_onDeleteFunc) (gpointer, ProgManager*);
typedef void (*ProgManagerObserver_changedFunc) (gpointer, ProgManager*);

/*interface*/ struct _ProgManagerObserver
{
	/*!
	 * Will be called before the manager is destroyed.
	 */
	ProgManagerObserver_onDeleteFunc onDelete;

	/*!
	 * Will be called if a program is added of removed from the manager. One
	 * call can cover multiple changed.
	 */
	ProgManagerObserver_changedFunc changed;

	gpointer object;
};


ProgManager * 	prog_manager_get_instance ();
ProgManager * 	prog_manager_new_with_namespace (Namespace * ns);
ProgManager * 	prog_manager_new ();
void 			prog_manager_destroy (ProgManager * self);
gboolean 		prog_manager_insert (ProgManager * self, Prog * obj);
gboolean 		prog_manager_delete_by_name (ProgManager * self, const gchar * name);
Prog * 			prog_manager_get_by_name (ProgManager * self, const gchar * name);

/*!
 * Returns a list of all objects in the manager. The list must be freed, but
 * the contained names must not.
 */
GList/*<const gchar*>*/ * prog_manager_get_names (ProgManager * self);
void 			prog_manager_steal (ProgManager * self, Prog * d);

/*!
 * Steal all objects from victim which don't collide with ours, i.e. only steal
 * object with a name which doesn't already exist in 'self'. The remaining
 * objects remain in 'victim'. Returns the numbers of objects stolen. Emits
 * 'changed' on both managers if things have changed.
 */
gint			prog_manager_steal_all (ProgManager * self, ProgManager * victim);
gboolean 		prog_manager_exists (ProgManager * self, const gchar * name);
void 			prog_manager_register_observer (ProgManager * self, ProgManagerObserver * o);
void 			prog_manager_unregister_observer (ProgManager * self, ProgManagerObserver * o);
void 			prog_manager_changed (ProgManager * self);
void 			prog_manager_block_notify (ProgManager * self);
void 			prog_manager_unblock_notify (ProgManager * self);
gboolean		prog_manager_is_empty (ProgManager * self);
gint 			prog_manager_size (ProgManager * self);
gint 			prog_manager_delete_with_filter (ProgManager * self,
						gboolean (*filter) (Prog*,gpointer), gpointer user_data);
Namespace *		prog_manager_get_namespace (ProgManager*);

ProgIterator * 	prog_manager_iterator (ProgManager * self);
void 			prog_iterator_next (ProgIterator * self);
gboolean 		prog_iterator_is_valid (ProgIterator * self);
Prog * 			prog_iterator_get (ProgIterator * self);
void 			prog_iterator_destroy (ProgIterator * self);

/*!
 * \example
 *    FOREACH_PROG(self,cur,iter,{
 *       printf ("name: %s\n", prog_get_name(cur));
 *    });
 */
#define FOREACH_PROG(manager,var,iter,code) {\
	ProgIterator * (iter) = prog_manager_iterator(manager);\
	for ( ; prog_iterator_is_valid(iter) ; prog_iterator_next(iter)) {\
		Prog * (var) = prog_iterator_get(iter);\
		code; } prog_iterator_destroy(iter); }

#define prog_new_from_def prog_new
Prog * 			prog_new  (const gchar * def, GError ** perr);
Prog * 			prog_new_with_lua (const gchar * def, const gchar * luacode, GError ** perr);

/*!
 * 'victim' gets destroyed. 'self' is returned. Only changes the contents.
 * The relation to a manager is changed.
 */
Prog * 			prog_move (Prog * self, Prog * victim);
void			prog_destroy (Prog * self);
ProgManager *	prog_get_manager (Prog * self);
void			prog_set_hidden (Prog * self, gboolean yesno);
const gchar * 	prog_get_name (const Prog * self);
const gchar *   prog_get_signature (const Prog * self);
const gchar *	prog_get_term (const Prog * self);
gint			prog_get_arg_count (Prog * self);
gboolean 		prog_is_hidden (const Prog * self);

const gchar *   prog_get_luacode (const Prog * self, size_t * psize);

void 			prog_dump (const Prog * self);

#endif /* PROGRAM_H_ */
