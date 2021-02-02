/*
 * Relation.h
 *
 *  Copyright (C) 2009,2010 Stefan Bolus, University of Kiel, Germany
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

#ifndef RELATION_H
#  define RELATION_H

#include <gtk/gtk.h>
#include "Namespace.h"
#include "RelviewTypes.h"

// Forward declarations
typedef struct _Relation Rel;
typedef struct _RelManager RelManager;
typedef struct _RelManagerObserver RelManagerObserver;
typedef struct _RelIterator RelIterator;

#include "Graph.h"
#include "Kure.h"

// NOTE: Each RelManager has ONE associated KureContext.

typedef void (*RelManagerObserver_onDeleteFunc) (gpointer, RelManager*);
typedef void (*RelManagerObserver_changedFunc) (gpointer, RelManager*);

/*interface*/ struct _RelManagerObserver
{
	/*!
	 * Will be called before the manager is destroyed.
	 */
	RelManagerObserver_onDeleteFunc onDelete;

	/*!
	 * Will be called if a function is added of removed from the manager. One
	 * call can cover multiple changed.
	 */
	RelManagerObserver_changedFunc changed;

	gpointer object;
};

RelManager * 	rel_manager_new (KureContext * context);
void 			rel_manager_destroy (RelManager * self);
gboolean 		rel_manager_insert (RelManager * self, Rel * obj);
gboolean 		rel_manager_delete_by_name (RelManager * self, const gchar * name);
Rel * 			rel_manager_get_by_name (RelManager * self, const gchar * name);
KureContext * 	rel_manager_get_context (RelManager * self);
Rel *			rel_manager_create_rel (RelManager*, const char * name, mpz_t rows, mpz_t cols);
Rel *			rel_manager_create_rel_si (RelManager*, const char * name, int rows, int cols);


/*!
 * Returns a list of all objects in the manager. The list must be freed, but
 * the contained names must not.
 */
GList/*<const gchar*>*/ * rel_manager_get_names (RelManager * self);
void 			rel_manager_steal (RelManager * self, Rel * d);

/*!
 * Steal all objects from victim which don't collide with ours, i.e. only steal
 * object with a name which doesn't already exist in 'self'. The remaining
 * objects remain in 'victim'. Returns the numbers of objects stolen. Emits
 * 'changed' on both managers if things have changed.
 */
gint			rel_manager_steal_all (RelManager * self, RelManager * victim);

/*!
 * Returns TRUE if a relation with that name is in the \ref RelManager. If you
 * want to check if an object with that name exists in the global namespace,
 * use the global \ref Namespace directly.
 *
 * \see rv_get_namespace
 * \see namespace_is_in
 */
gboolean 		rel_manager_exists (RelManager * self, const gchar * name);
void 			rel_manager_register_observer (RelManager * self, RelManagerObserver * o);
void 			rel_manager_unregister_observer (RelManager * self, RelManagerObserver * o);
void 			rel_manager_changed (RelManager * self);
void 			rel_manager_block_notify (RelManager * self);
void 			rel_manager_unblock_notify (RelManager * self);
gboolean		rel_manager_is_empty (RelManager * self);
gint 			rel_manager_size (RelManager * self);
gint 			rel_manager_delete_with_filter (RelManager * self,
						gboolean (*filter) (Rel*,gpointer), gpointer user_data);
gboolean 		rel_manager_is_valid_name (RelManager * self, const gchar * name);
Namespace *		rel_manager_get_namespace (RelManager * self);

/*!
 * Returns TRUE if the given name COULD be inserted into the given
 * \ref RelManager.
 */
gboolean 		rel_manager_is_insertable_name (RelManager * self, const gchar * name);

RelIterator * 	rel_manager_iterator (RelManager * self);
void 			rel_iterator_next (RelIterator * self);
gboolean 		rel_iterator_is_valid (RelIterator * self);
Rel * 			rel_iterator_get (RelIterator * self);
void 			rel_iterator_destroy (RelIterator * self);

/*!
 * \example
 *    FOREACH_REL(self,cur,iter,{
 *       printf ("name: %s\n", rel_get_name(cur));
 *    });
 */
#define FOREACH_REL(manager,var,iter,code) {\
	RelIterator * (iter) = rel_manager_iterator(manager);\
	for ( ; rel_iterator_is_valid(iter) ; rel_iterator_next(iter)) {\
		Rel * (var) = rel_iterator_get(iter);\
		code; } rel_iterator_destroy(iter); }

// NOTE: obsolete!
//#define REL_MAX_NAME_LEN        MAXNAMELENGTH


// You need a KureContext to create a relation!
Rel * 			rel_new_from_impl (const gchar * name, KureRel * impl);
void 			rel_destroy (Rel * self);
KureRel *		rel_get_impl (Rel*);

/*!
 * Steals the relation's implementation. Uses an empty relation of size 1x1
 * in the same \ref KureContext as replacement.
 *
 * \return Returns the implementation stolen from the relation.
 */
KureRel * 		rel_steal_impl (Rel * self);

const gchar * 	rel_get_name (const Rel*);
gboolean		rel_has_name (const Rel*, const gchar * name);
gboolean		rel_is_empty (const Rel*); // == O?
gboolean		rel_is_1x1 (const Rel*);
RelManager *    rel_get_manager (Rel*);

/*!
 * Assigns the relation of the source to the given \ref Rel object. Note that
 * ONLY the relation bits and it's dimension is assigned. All remaining stuff
 * remains untouched. E.g. the hidden status or the associated. Even the name
 * doesn't change.
 *
 * On error, the destination doesn't change. Returns the destination.
 */
Rel * 			rel_assign (Rel*, const Rel * src);

/*!
 * Returns TRUE if the given name is valid relation name. Uses the
 * \ref Namespace associated to the current manager. If there is no manager
 * any relation name is allowed. If you rely on a given namespace, use
 * \ref namespace_is_valid_name instead.
 */
gboolean        rel_is_valid_name       (const gchar * name, RelManager * manager);

Rel * 			rel_new_from_xgraph (KureContext * context, XGraph * gr);

//rellistptr 		relation_new_from_graph (XGraph * gr);
//rellistptr 		rel_new_from_xgraph (XGraph * gr);

gboolean        rel_allow_display        (Rel * rel);
void            rel_changed         (Rel * rel);
//Rel *           relation_assign_by_ref   (Rel * self, Rel * r);
void            rel_clear           (Rel * rel);

gboolean		rel_is_hidden			  (const Rel * self);
void	 		rel_set_hidden 			  (Rel * self, gboolean yesno);

void			rel_get_rows (const Rel*, mpz_t rows);
void			rel_get_cols (const Rel*, mpz_t cols);

gboolean		rel_same_dim (const Rel * a, const Rel * b);

/* If another relation with the target name exists, FALSE is returned. If the
 * relation already has that name, TRUE is returned. */
gboolean		rel_rename (Rel * self, const gchar * new_name);


typedef void (*relation_observer_renamed_func_t) (gpointer,Rel*,const char * old_name);
typedef void (*relation_observer_changed_func_t) (gpointer,Rel*);
typedef void (*relation_observer_on_delete_func_t) (gpointer,Rel*);

#define RELATION_OBSERVER_RENAMED_FUNC(f) \
        ((relation_observer_renamed_func_t)(f))
#define RELATION_OBSERVER_CHANGED_FUNC(f) \
        ((relation_observer_changed_func_t)(f))
#define RELATION_OBSERVER_ON_DELETE_FUNC(f) \
        ((relation_observer_on_delete_func_t)(f))

typedef struct _RelationObserver {
        relation_observer_renamed_func_t renamed;
        relation_observer_changed_func_t changed;
        relation_observer_on_delete_func_t onDelete;

        gpointer object;
} RelObserver, RelationObserver;

/*!
 * Adds a observer to the relation, if it is not already registered.
 */
#define relation_register_observer rel_register_observer
void rel_register_observer (Rel * obj, RelationObserver * observer);

#define relation_unregister_observer rel_unregister_observer
void rel_unregister_observer (Rel * obj, RelationObserver * observer);


#endif /* Relation.h */
