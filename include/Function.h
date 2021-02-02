/*
 * Function.h
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

#ifndef FUNCTION_H_
#define FUNCTION_H_

#include "RelviewTypes.h"
#include "Namespace.h"

typedef struct _Function Fun;
typedef struct _FunManager FunManager;
typedef struct _FunManagerObserver FunManagerObserver;
typedef struct _FunIterator FunIterator;


typedef void (*FunManagerObserver_onDeleteFunc) (gpointer, FunManager*);
typedef void (*FunManagerObserver_changedFunc) (gpointer, FunManager*);

/*interface*/ struct _FunManagerObserver
{
	/*!
	 * Will be called before the manager is destroyed.
	 */
	FunManagerObserver_onDeleteFunc onDelete;

	/*!
	 * Will be called if a function is added of removed from the manager. One
	 * call can cover multiple changed.
	 */
	FunManagerObserver_changedFunc changed;

	gpointer object;
};


FunManager * 	fun_manager_get_instance ();
FunManager * 	fun_manager_new ();
void 			fun_manager_destroy (FunManager * self);
gboolean 		fun_manager_insert (FunManager * self, Fun * obj);
gboolean 		fun_manager_delete_by_name (FunManager * self, const gchar * name);
Fun * 			fun_manager_get_by_name (FunManager * self, const gchar * name);
Namespace *		fun_manager_get_namespace (FunManager * self);

/*!
 * Returns a list of all objects in the manager. The list must be freed, but
 * the contained names must not.
 */
GList/*<const gchar*>*/ * fun_manager_get_names (FunManager * self);
void 			fun_manager_steal (FunManager * self, Fun * d);

/*!
 * Steal all objects from victim which don't collide with ours, i.e. only steal
 * object with a name which doesn't already exist in 'self'. The remaining
 * objects remain in 'victim'. Returns the numbers of objects stolen. Emits
 * 'changed' on both managers if things have changed.
 */
gint			fun_manager_steal_all (FunManager * self, FunManager * victim);
gboolean 		fun_manager_exists (FunManager * self, const gchar * name);
void 			fun_manager_register_observer (FunManager * self, FunManagerObserver * o);
void 			fun_manager_unregister_observer (FunManager * self, FunManagerObserver * o);
void 			fun_manager_changed (FunManager * self);
void 			fun_manager_block_notify (FunManager * self);
void 			fun_manager_unblock_notify (FunManager * self);
gboolean		fun_manager_is_empty (FunManager * self);
gint 			fun_manager_size (FunManager * self);
gint 			fun_manager_delete_with_filter (FunManager * self,
						gboolean (*filter) (Fun*,gpointer), gpointer user_data);

FunIterator * 	fun_manager_iterator (FunManager * self);
void 			fun_iterator_next (FunIterator * self);
gboolean 		fun_iterator_is_valid (FunIterator * self);
Fun * 			fun_iterator_get (FunIterator * self);
void 			fun_iterator_destroy (FunIterator * self);

/*!
 * \example
 *    FOREACH_FUN(self,cur,iter,{
 *       printf ("name: %s\n", fun_get_name(cur));
 *    });
 */
#define FOREACH_FUN(manager,var,iter,code) {\
	FunIterator * (iter) = fun_manager_iterator(manager);\
	for ( ; fun_iterator_is_valid(iter) ; fun_iterator_next(iter)) {\
		Fun * (var) = fun_iterator_get(iter);\
		code; } fun_iterator_destroy(iter); }

#define fun_new_from_def fun_new

/*!
 * Creates a function from a textual definition. A trailing dot is added if
 * necessary.
 */
Fun * 			fun_new  (const gchar * def, GError ** perr);
Fun * 			fun_new_with_lua (const gchar * def, const gchar * luacode, GError ** perr);

/*!
 * 'victim' gets destroyed. 'self' is returned. Only changes the contents.
 * The relation to a manager is changed.
 */
Fun * 			fun_move (Fun * self, Fun * victim);
void			fun_destroy (Fun * self);
FunManager *	fun_get_manager (Fun * self);
void			fun_set_hidden (Fun * self, gboolean yesno);
const gchar * 	fun_get_name (const Fun * self);
const gchar * 	fun_get_expr (const Fun * self);
const gchar * 	fun_get_def (const Fun * self);
const gchar *	fun_get_term (const Fun * self);
gint			fun_get_arg_count (Fun * self);
gboolean 		fun_is_hidden (const Fun * self);
gboolean		fun_is_local (Fun * self);
void			fun_set_local (Fun * self, gboolean yesno);

const gchar *   fun_get_luacode (const Fun * self, size_t * psize);

void 			fun_dump (const Fun * self);

//#include "funktion.h"

#endif /* FUNCTION_H_ */
