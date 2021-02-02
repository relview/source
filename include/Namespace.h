/*
 * Namespace.h
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

#ifndef NAMESPACE_H_
#define NAMESPACE_H_

#include <glib.h>

typedef struct _Namespace Namespace;

typedef const gchar * (*NamedProfile_getNameFunc) (gconstpointer);

typedef gboolean (*NamespaceFilterFunc) (const gchar*, gpointer);

/*!
 * \ref NamedProfiled provide a common interface for differing object with
 * non-conforming routines.
 */
typedef struct _NamedProfile {
	NamedProfile_getNameFunc getName;

	/* All objects must share the same comparison function. Different compare
	 * functions (e.g. case-insensitive) would have been the only reason for
	 * such a function. */
	//gboolean hasName (gpointer, const gchar*);

	//void addObserver (gpointer, Namespace*, void (*) (Namespace*,gpointer,const gchar*));
	//void removeObserver (gpointer, Namespace*);
} NamedProfile;


/*!
 * Creates a new \ref Namespace. The default string comparison function is
 * \ref g_str_equal, i.e. string comparison is done byte-wise and case-
 * sensitive. Initially, there is no filter set.
 *
 * The newly created namespace has a reference count of 0.
 *
 * \see namespace_ref
 * \see namespace_unref
 */
Namespace * 	namespace_new ();

/*!
 * References the \ref Namespace;
 */
void			namespace_ref (Namespace*);

/*!
 * Dereferences the \ref Namespace. If the reference count hits zero, the
 * namespace is destroyed.
 */
void 			namespace_unref (Namespace*);

/*!
 * Creates a new \ref Namespace with the given string comparison function and
 * the given pattern.
 *
 * The newly created namespace has a reference count of 0.
 *
 * \see namespace_set_pattern
 * \see namespace_ref
 * \see namespace_unref
 */
Namespace *		namespace_new_full (GEqualFunc str_equal_func);

/*!
 * Destroying a namespace with a reference count higher than 0 will cause a
 * warning message. The namespace is destroyed anyway.
 */
void 			namespace_destroy (Namespace * self);

/*!
 * Returns FALSE and doesn't set the pattern if there are violating objects in
 * the \ref Namespace.
 *
 * \param func May be NULL.
 */
gboolean 		namespace_set_filter_func (Namespace * self,
		NamespaceFilterFunc func, gpointer user_data);

/*!
 * Returns TRUE iff the name matches currently set filter of the
 * \ref Namespace. In case of no filter, TRUE is always returned.
 *
 * \note Doesn't test if the name is already in the \ref Namespace.
 *       Use \ref namespace_is_in for that purpose.
 */
gboolean		namespace_is_valid_name (Namespace * self, const gchar * name);

/*!
 * Pointer for the names may differ from pointer in the objects itself. The
 * returned list must be freed using \ref g_list_free afterwards.
 */
GList/*<const gchar*>*/	*	namespace_get_names (const Namespace * self);

#define namespace_insert namespace_add
gboolean 		namespace_add (Namespace * self, gpointer obj, const NamedProfile * profile);
void			namespace_remove (Namespace * self, gpointer obj);
void 			namespace_remove_by_name (Namespace * self, const gchar * name);

/*!
 * Returns TRUE iff the given name is already in the given \ref Namespace.
 */
#define namespace_exists namespace_is_in
gboolean		namespace_is_in (Namespace * self, const gchar * name);

/*!
 * The object's name has been changed.
 */
gboolean		namespace_name_changed (Namespace * self, gpointer obj);

/*!
 * Returns the object associated to the given name. Returns NULL if there is
 * no such object.
 */
gpointer namespace_get_by_name (Namespace * self, const gchar * name);

#endif /* NAMESPACE_H_ */
