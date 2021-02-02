/*
 * Domain.h
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


#ifndef _SRC_DOMAIN_H
#define _SRC_DOMAIN_H

#include "RelviewTypes.h"
#include "Namespace.h"

typedef struct _Domain Domain, Dom;
typedef struct _DomManager DomManager;
typedef struct _DomManagerObserver DomManagerObserver;
typedef struct _DomIterator DomIterator;



typedef void (*DomManagerObserver_onDeleteFunc) (gpointer, DomManager*);
typedef void (*DomManagerObserver_changedFunc) (gpointer, DomManager*);

/*interface*/ struct _DomManagerObserver
{
	/*!
	 * Will be called before the manager is destroyed.
	 */
	DomManagerObserver_onDeleteFunc onDelete;

	/*!
	 * Will be called if a domain is added of removed from the manager. One
	 * call can cover multiple changed.
	 */
	DomManagerObserver_changedFunc changed;

	gpointer object;
};


DomManager * 	dom_manager_get_instance ();
DomManager * 	dom_manager_new ();
DomManager * 	dom_manager_new_with_namespace (Namespace*);
void 			dom_manager_destroy (DomManager * self);
gboolean 		dom_manager_insert (DomManager * self, Dom * d);
gboolean 		dom_manager_delete_by_name (DomManager * self, const gchar * name);
Dom * 			dom_manager_get_by_name (DomManager * self, const gchar * name);
Namespace * 	dom_manager_get_namespace (DomManager * self);

/*!
 * Returns a list of all domains in the manager. The list must be freed, but
 * the contained names must not.
 */
GList/*<const gchar*>*/ * dom_manager_get_names (DomManager * self);
void 			dom_manager_steal (DomManager * self, Dom * d);

/*!
 * Steal all domains from victim which don't collide with ours, i.e. only steal
 * domain with a name which doesn't already exist in 'self'. The remaining
 * domains remain in 'victim'. Returns the numbers of domains stolen. Emits
 * 'changed' on both managers if things have changed.
 */
gint			dom_manager_steal_all (DomManager * self, DomManager * victim);
gboolean 		dom_manager_exists (DomManager * self, const gchar * name);
void 			dom_manager_register_observer (DomManager * self, DomManagerObserver * o);
void 			dom_manager_unregister_observer (DomManager * self, DomManagerObserver * o);
void 			dom_manager_changed (DomManager * self);
void 			dom_manager_block_notify (DomManager * self);
void 			dom_manager_unblock_notify (DomManager * self);
gboolean		dom_manager_is_empty (DomManager * self);
gint 			dom_manager_size (DomManager * self);
gint 			dom_manager_delete_with_filter (DomManager * self,
						gboolean (*filter) (Dom*,gpointer), gpointer user_data);

DomIterator * 	dom_manager_iterator (DomManager * self);
void 			dom_iterator_next (DomIterator * self);
gboolean 		dom_iterator_is_valid (DomIterator * self);
Dom * 			dom_iterator_get (DomIterator * self);
void 			dom_iterator_destroy (DomIterator * self);

/*!
 * \example
 *    FOREACH_DOM(self,cur,iter,{
 *       printf ("name: %s\n", dom_get_name(cur));
 *    });
 */
#define FOREACH_DOM(manager,var,iter,code) {\
	DomIterator * (iter) = dom_manager_iterator(manager);\
	for ( ; dom_iterator_is_valid(iter) ; dom_iterator_next(iter)) {\
		Dom * (var) = dom_iterator_get(iter);\
		code; } dom_iterator_destroy(iter); }

Dom * 			dom_new (const gchar * name, DomainType type,
						 const gchar * first_comp, const gchar * second_comp,
						 GError ** perr);
/*!
 * Create a new domain from a domain definition, that is, a term of the form
 *    <name> = "<op>(" <exp> "," <exp> ")"
 * where <op> is either PROD or SUM. This may change in the future.
 *
 *
 * \param def See above.
 * \param perr Optional. If NULL is returned, the corresponding error is set.
 */
Dom *			dom_new_from_def (const gchar * def, GError ** perr);
//Dom * 			dom_new_from_root (nodeptr root, GError ** perr);

/*!
 * 'victim' gets destroyed. 'self' is returned. Only changes the contents.
 * The relation to a manager is changed.
 */
Dom * 			dom_move (Dom * self, Dom * victim);
void			dom_destroy (Dom * self);
const gchar *	dom_get_first_comp (const Dom * self);
const gchar *	dom_get_second_comp (const Dom * self);
//nodeptr			dom_get_first_root (Dom * self);
//nodeptr			dom_get_second_root (Dom * self);
DomManager *	dom_get_manager (Dom * self);
void			dom_set_hidden (Dom * self, gboolean yesno);
const gchar * 	dom_get_name (const Dom * self);
gboolean 		dom_is_hidden (const Dom * self);
//gboolean		dom_is_local (Dom * self);
//void			dom_set_local (Dom * self, gboolean yesno);

const gchar *	dom_get_first_comp_lua (const Dom * self);
const gchar *	dom_get_second_comp_lua (const Dom * self);

/*!
 * Returns the term of the given domain, that is, the right-hand side of the
 * equation sign. E.g. "(Rv) x T^", where "(Rv)" is the first and "T^" is the
 * second component. The returned string must be freed after use.
 */
gchar * 		dom_get_term (const Dom * self);

/*!
 * Can't be used as a replacement for the old 'type' field which used def.
 * from the parser. Use \ref dom_get_parser_type for that.
 */
DomainType		dom_get_type (const Dom * self);

gint			dom_get_parser_type (const Dom * self);

//char * get_first_komp          ( const gchar * );
//char * get_second_komp         ( const gchar * );

#endif
