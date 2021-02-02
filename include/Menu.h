/*
 * Menu.h
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

#ifndef MENU_H_
#define MENU_H_

#include <gtk/gtk.h>

typedef struct _MenuEntry MenuEntry;
typedef struct _MenuDomain MenuDomain;
typedef struct _MenuManager MenuManager;

/*! The first is the user-data, the second gpointer is the domain dependent
 * object. */
typedef gboolean 	(*MenuIsEnabledFunc) (gpointer,MenuEntry*,gpointer);
typedef void 		(*MenuOnActivateFunc) (gpointer,MenuEntry*,gpointer);

typedef gpointer 	(*MenuDomainExtractFunc) (GtkMenuItem*,gpointer);

MenuEntry *		menu_entry_new (const gchar * label, MenuIsEnabledFunc,
					MenuOnActivateFunc, gpointer user_data);
MenuEntry *		menu_entry_new_full (const gchar * label, MenuIsEnabledFunc,
					MenuOnActivateFunc, gpointer user_data,
					GDestroyNotify user_data_dtor);
void 			menu_entry_destroy (MenuEntry*);
const gchar * 	menu_entry_get_label (const MenuEntry*);
MenuDomain *	menu_entry_get_domain (MenuEntry*);
GtkWidget *		menu_entry_get_menuitem (MenuEntry*);

/*!
 * Returns FALSE, if the entry is not associated with a \ref MenuDomain.
 */
gboolean		menu_entry_is_enabled (MenuEntry*);


void			menu_entry_activate (MenuEntry*,gpointer);

/*!
 * The prefix should not end with a trailing slash.
 */
MenuDomain *	menu_domain_new (const gchar * prefix, MenuDomainExtractFunc);
MenuDomain *	menu_domain_new_full (const gchar * prefix, MenuDomainExtractFunc,
					gpointer user_data, GDestroyNotify user_data_dtor);
MenuManager *	menu_domain_get_manager (MenuDomain*);
const gchar *	menu_domain_get_prefix (const MenuDomain*);

/*!
 * References the given \ref GtkMenuShell using \ref g_object_ref.
 */
gboolean		menu_domain_add_subdomain (MenuDomain *, const gchar * subpath,
					GtkMenuShell * submenu);

/*!
 * After one of the following operation the \ref MenuDomain is responsible
 * to destroy the passed \ref MenuEntry.
 *
 * The sub-path must not contain the domains prefix. May be NULL or an empty
 * string.
 *
 * Each \ref MenuEntry may be registered just once! Internally, there is a
 * mapping from the pointers to the subpaths.
 */
gboolean		menu_domain_add (MenuDomain*, const gchar * subpath, MenuEntry*);

/*!
 * The \ref MenuEntry belongs to the caller afterwards. It is up to the caller
 * to destroy it. Returns NULL if the \ref MenuEntry is not registered in the
 * domain.
 */
gboolean		menu_domain_steal (MenuDomain*,MenuEntry*);

void 			menu_domain_destroy (MenuDomain*);

/*!
 * Returns a list of the registered \ref MenuEntry objects. May be NULL
 * if no entry was registered yet.
 */
GList/*<MenuEntry*>*/ * menu_domain_get_entries (MenuDomain*);

#if !defined IN_RV_PLUGIN
MenuManager *	menu_manager_new ();
gboolean		menu_manager_add_domain (MenuManager*, MenuDomain*);
#endif

MenuDomain *	menu_manager_get_domain (MenuManager*, const gchar * path_prefix);
gboolean		menu_manager_steal (MenuManager*,MenuDomain*);

GList/*<MenuDomain*>*/ * menu_manager_get_domains (MenuManager*);

#endif /* MENU_H_ */
