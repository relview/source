/*
 * Menu.c
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

#include "Menu.h"
#include <string.h>

struct _MenuEntry
{
	GtkWidget * menuitem;
	MenuDomain * domain; /*! May be NULL */

	gchar * label;
	MenuIsEnabledFunc is_enabled;
	MenuOnActivateFunc on_activate;

	GDestroyNotify user_data_dtor;
	gpointer user_data;

	gboolean perm_disabled; /*! Is the entry permanently disabled? */
};


struct _MenuDomain
{
	MenuManager * manager;
	gchar * prefix;

	/*! Maps a \ref MenuEntry to its subpath. */
	GHashTable/*<MenuEntry*,gchar*>*/ * ents;

	/*! Maps sub-paths (e.g. "sub/menu") to the corresponding Gtk object. */
	GHashTable/*<gchar*,GtkMenuShell*>*/ * subs;

	/*! See \ref menu_domain_new. */
	MenuDomainExtractFunc extract;
	gpointer ex_udata;
	GDestroyNotify ex_udata_dtor;
};

struct _MenuManager
{
	/*!
	 * Maps the domains' prefixes (e.g. "/i/m/a/menu") to the corresponding
	 * \ref MenuDomain object.
	 */
	GHashTable/*<gchar*,MenuDomain*>*/ * doms;
};



static gchar * _normalize_prefix (const gchar * _prefix)
{
	gchar * prefix = g_strstrip(g_strdup (_prefix));
	int len = strlen (prefix);

	/* Remove any trailing slash. */
	while (len > 0 && prefix[len-1] == '/') {
		prefix[len-1] = '\0';
		len --;
	}

	return prefix;
}

static gchar * _normalize_subdomain (const gchar * _subdomain)
{
	gchar * prefix = g_strstrip(g_strdup (_subdomain));
	int len = strlen (prefix);

	/* Remove any trailing slash. */
	while (len > 0 && prefix[len-1] == '/') {
		prefix[len-1] = '\0';
		len --;
	}

	return prefix;
}

static void _on_activate_gtk_wrapper (GtkMenuItem *menuitem, gpointer user_data)
{
	MenuEntry * ent = (MenuEntry*) user_data;
	MenuDomain * domain = menu_entry_get_domain(ent);

	g_assert (domain != NULL);

	if (ent->on_activate) {
		gpointer object = domain->extract (menuitem, domain->ex_udata);
		ent->on_activate (ent->user_data, ent, object);
	}
	else {
		g_warning ("MenuEntry with label \"%s\" in domain \"%s\" "
				"has no \"activate\" signal handler.",
				menu_entry_get_label(ent), menu_domain_get_prefix(domain));
	}
}

/*******************************************************************************
 *                                  MenuEntry                                  *
 *                                                                             *
 *                              Tue, 13 Apr 2010                               *
 ******************************************************************************/

void menu_entry_dtor (MenuEntry * self)
{
	if (self->domain) {
		menu_domain_steal (self->domain, self);
	}
	gtk_widget_destroy(self->menuitem);
	g_free (self->label);

	if (self->user_data_dtor)
		self->user_data_dtor (self->user_data);
}

MenuEntry *	menu_entry_new (const gchar * label, MenuIsEnabledFunc is_enabled,
					MenuOnActivateFunc on_activate, gpointer user_data)
{
	return menu_entry_new_full (label, is_enabled, on_activate, user_data, NULL);
}

MenuEntry *	menu_entry_new_full (const gchar * label, MenuIsEnabledFunc is_enabled,
					MenuOnActivateFunc on_activate, gpointer user_data,
					GDestroyNotify user_data_dtor)
{
	if (!label) return NULL;
	else {
		MenuEntry * self = g_new0 (MenuEntry, 1);
		self->label = g_strdup (label);
		self->is_enabled = is_enabled;
		self->on_activate = on_activate;
		self->user_data = user_data;
		self->user_data_dtor = user_data_dtor;

		self->menuitem = gtk_menu_item_new_with_label(label);
		gtk_widget_show (self->menuitem);
		g_signal_connect (G_OBJECT(self->menuitem), "activate",
				G_CALLBACK(_on_activate_gtk_wrapper), self);

		return self;
	}
}

void menu_entry_destroy (MenuEntry * self)
{
	menu_entry_dtor(self);
	g_free (self);
}

const gchar * menu_entry_get_label (const MenuEntry * self) {
	return self->label; }

MenuDomain * menu_entry_get_domain (MenuEntry * self) {
	return self->domain; }

GtkWidget * menu_entry_get_menuitem (MenuEntry * self) {
	return self->menuitem; }

gboolean menu_entry_is_enabled (MenuEntry * self)
{
	if (!self || !self->domain) return FALSE;
	else {
		return self->is_enabled (self->user_data, self,
				self->domain->extract(GTK_MENU_ITEM(self->menuitem),
						self->domain->ex_udata));
	}
}

void menu_entry_activate (MenuEntry * self, gpointer object)
{
	if (self)
		self->on_activate (self->user_data, self, object);
}

/*******************************************************************************
 *                                 MenuDomain                                  *
 *                                                                             *
 *                              Tue, 13 Apr 2010                               *
 ******************************************************************************/

MenuDomain * menu_domain_new (const gchar * _prefix, MenuDomainExtractFunc extract)
{
	return menu_domain_new_full (_prefix, extract, NULL, NULL);
}

MenuDomain * menu_domain_new_full (const gchar * _prefix, MenuDomainExtractFunc extract,
		gpointer user_data, GDestroyNotify user_data_dtor)
{
	gchar * prefix = _normalize_prefix(_prefix);
	if (!prefix) return NULL;
	else {
		MenuDomain * self = g_new0 (MenuDomain, 1);
		self->ents = g_hash_table_new_full (g_direct_hash, g_direct_equal,
				(GDestroyNotify)menu_entry_destroy, (GDestroyNotify)g_free);
		self->prefix = _normalize_prefix(prefix);
		self->subs = g_hash_table_new_full (g_str_hash, g_str_equal,
				(GDestroyNotify)g_free, (GDestroyNotify)g_object_unref);
		self->extract = extract;
		self->ex_udata = user_data;
		self->ex_udata_dtor = user_data_dtor;
		return self;
	}
}

void menu_domain_dtor (MenuDomain * self)
{
	if (self->manager)
		menu_manager_steal (self->manager, self);

	g_hash_table_destroy(self->ents);
	g_hash_table_destroy(self->subs);
	g_free (self->prefix);

	if (self->ex_udata_dtor)
		self->ex_udata_dtor (self->ex_udata);
}

void menu_domain_destroy (MenuDomain * self)
{
	menu_domain_dtor(self);
	g_free (self);
}

MenuManager * menu_domain_get_manager (MenuDomain * self)
{ return self->manager; }

const gchar * menu_domain_get_prefix (const MenuDomain * self)
{
	return self->prefix;
}

gboolean menu_domain_add_subdomain (MenuDomain * self, const gchar * _subpath,
					GtkMenuShell * submenu)
{
	gchar * subdomain = _normalize_subdomain(_subpath);
	if ( !subdomain)
		return FALSE;
	else {
		gboolean ret = FALSE;

		if ( !g_hash_table_lookup(self->subs, subdomain))
		{
			g_object_ref(G_OBJECT(submenu));
			g_hash_table_insert(self->subs, subdomain, submenu);
			ret = TRUE;
		}

		return ret;
	}
}

gboolean menu_domain_add (MenuDomain * self, const gchar * _subpath,
		MenuEntry * ent)
{
	gchar * subpath = _normalize_subdomain(_subpath);
	if (!subpath) return FALSE;
	else {
		GtkMenuShell * shell;
		shell = (GtkMenuShell*) g_hash_table_lookup(self->subs, subpath);
		if ( !shell) {
			g_warning ("menu_domain_add: Unknown sub-domain \"%s\".", subpath);
			g_free (subpath);
			return FALSE;
		}
		else {
			g_hash_table_insert(self->ents, ent, subpath);
			gtk_menu_shell_append(GTK_MENU_SHELL(shell), ent->menuitem);

			ent->domain = self;
			return TRUE;
		}
	}
}

GList/*<MenuEntry*>*/ * menu_domain_get_entries (MenuDomain * self)
{ return g_hash_table_get_keys(self->ents); }


gboolean menu_domain_steal (MenuDomain * self, MenuEntry * ent)
{
	gchar * subpath = g_hash_table_lookup(self->ents, ent);
	if ( !subpath) return FALSE;
	else {
		if ( !g_hash_table_steal(self->ents, ent)) return FALSE;
		else {
			/* Finally, we have to remove the entries menu-item from the
			 * associated shell. */
			GtkMenuShell * shell = (GtkMenuShell*)
					g_hash_table_lookup(self->subs, subpath);
			g_assert (shell != NULL);
			gtk_container_remove (GTK_CONTAINER(shell), ent->menuitem);

			g_free (subpath);
			ent->domain = NULL;

			return TRUE;
		}
	}
}

/*******************************************************************************
 *                                 MenuManager                                 *
 *                                                                             *
 *                              Tue, 13 Apr 2010                               *
 ******************************************************************************/

MenuManager * menu_manager_new ()
{
	MenuManager * self = g_new0 (MenuManager, 1);
	self->doms = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify)menu_domain_destroy);
	return self;
}

MenuDomain * menu_manager_get_domain (MenuManager * self, const gchar * path_prefix)
{
	gchar * prefix = _normalize_prefix (path_prefix);
	if ( !prefix) return NULL;
	else {
		MenuDomain * ret = g_hash_table_lookup(self->doms, prefix);
		g_free (prefix);
		return ret;
	}
}

gboolean menu_manager_add_domain (MenuManager * self, MenuDomain * dom)
{
	gchar * prefix = _normalize_prefix(menu_domain_get_prefix(dom));
	if (!prefix) return FALSE;
	else {
		g_hash_table_insert(self->doms, prefix, dom);
		dom->manager = self;
		return TRUE;
	}
}

gboolean menu_manager_steal (MenuManager * self, MenuDomain * dom)
{
	/* g_hash_table_steal doesn't call the destroy function for the key, so
	 * we have to get it first and dispose of it ourselves. */

	gpointer orig_key = NULL, value = NULL;

	if (g_hash_table_lookup_extended(self->doms, dom->prefix, &orig_key, &value)) {
		g_assert (value == dom);
		if ( !g_hash_table_steal(self->doms, orig_key)) return FALSE;
		else {
			dom->manager = NULL;
			g_free (orig_key);
			return TRUE;
		}
	}
	else return FALSE;
}
