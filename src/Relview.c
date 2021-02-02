/*
 * Relview.c
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

#include "Relview.h"

#include "Relation.h" /* rel_is_valid_name() */
#include "input_string_gtk.h" /* show_input_string() */
#include <unistd.h>
#include <gtk/gtk.h>
#include "IOHandler.h"
#include "XddFile.h" // xdd_get_handler
#include "Function.h"
#include "Program.h"
#include "Domain.h"
#include "Kure.h"
#include "Eps.h"
#include "version.h"
#include <lauxlib.h> // luaL_*

#include <stdlib.h> // getenv
#include <assert.h>
#include <unistd.h>
#include <sys/types.h> // getcwd
#include <pwd.h> // getpwuid
#include <stdarg.h>
#include <string.h>
#include "config.h" // PACKAGE_*

Verbosity g_verbosity;

void verbose_printf(Verbosity level, const char * fmt, ...)
{
	if (g_verbosity >= level) {
		va_list ap;
		va_start (ap, fmt);
		vprintf (fmt, ap);
		va_end (ap);
	}
}

#warning TODO: Remove me!
#define REL_MAX_NAME_LEN 128


#define RELVIEW_OBSERVER_NOTIFY(self,func,...) \
		OBSERVER_NOTIFY(observers,GSList,RelviewObserver,self,func,__VA_ARGS__)

#include "prefs.h"

typedef struct _LabelWrapper LabelWrapper;
static void _label_wrapper_destroy (LabelWrapper * self);

struct _Relview
{
	/* all elements are private. */
	gint verbosity;
	PrefsObserver prefs_observer;

	KureContext * context;

	gboolean is_init; /*! See rv_init. */
	int argc;
	const char ** argv;
	gchar * abs_dist_dir;

	GtkBuilder * gtk_builder;

	GList/*<IOHandler*>*/ * io_handlers;

	GSList/*<RelviewObserver*>*/ * observers;

	MenuManager * menu_manager;

	/* The workspace manages window positions, sizes and the initial
	 * visibility. Uses the preferences to save and restore these settings. */
	Workspace * workspace;

	/* Relations, Domains, Functions and Programs share the same namespace.
	 * This is realized using a single global object which registers every
	 * such object. The need for that namespace is due to Lua.  */
	Namespace * namespace;

	RelManager * rel_manager;
	DomManager * dom_manager;
	FunManager * fun_manager;
	ProgManager * prog_manager;

	GHashTable/*<gchar*,LabelWrapper*>*/ * labels;
	GHashTable/*<LabelAssoc*,LabelAssoc*>*/ * label_assocs;
};

static Relview * _rv_new ();
static Relview * _rv_new_with_context (KureContext * context);
static void _rv_prefs_listener (Relview * self, const
		gchar * section, const gchar * key);

Relview * g_rv; /* private */

/*!
 * Singleton function to access the global Relview-object. The object is
 * created on demand. */
Relview * rv_get_instance ()
{
	if (! g_rv)
		g_rv = _rv_new ();

	return g_rv;
}

/*!
 * Filter function for the global \ref Namespace.
 *
 * \see namespace_set_filter_func
 * \see NamespaceFilterFunc
 */
gboolean _filter_func (const gchar * name, Relview * self)
{
	static GRegex * pat = NULL;

	if ( ! pat) {
		GError * error = NULL;

		pat = g_regex_new ("^[a-zA-Z_][0-9a-zA-Z_]*$",
				G_REGEX_DOLLAR_ENDONLY,
				0, &error);
		if ( ! pat) {
			printf (__FILE__" (%d): regex error: %s\n", __LINE__, error->message);
			assert (NULL == pat);
		}
	}

	return (strcmp(name,"$") == 0)
			|| g_regex_match (pat, name, 0, NULL);
}

static guint _label_assoc_key_hash (gconstpointer key)
{
	LabelAssoc * self = (LabelAssoc*) key;
	return (guint)self->rel + (guint)self->viewable;
}

static gboolean _label_assoc_key_equal (gconstpointer * key1, gconstpointer * key2)
{
	LabelAssoc * x = (LabelAssoc*) key1, *y = (LabelAssoc*) key2;
	return x->rel == y->rel && x->viewable == y->viewable;
}

// References the given context.
Relview * _rv_new_with_context (KureContext * context)
{
	Relview * self = g_new0 (Relview, 1);
	self->verbosity = prefs_get_int("settings", "debug_information", FALSE) ? 1 : 0;

	/* Register the observer for the preferences */
	{
		self->prefs_observer.changed = PREFS_OBSERVER_CHANGED_FUNC(_rv_prefs_listener);
		self->prefs_observer.object = self;

		prefs_register_observer (&self->prefs_observer);
	}

	/* GtkBuilder will be created later, because creation depends on
	 * a data file, and data file lookups depend on this structure. */

	/* Create the default IO handlers. */
	self->io_handlers = NULL;

	self->observers = NULL;

	self->context = context;
	kure_context_ref(context);

	self->menu_manager = menu_manager_new();

	self->workspace = workspace_new();

	self->namespace = namespace_new();
	namespace_set_filter_func (self->namespace,
			(NamespaceFilterFunc)_filter_func, (gpointer) self);
	namespace_ref (self->namespace);

	self->rel_manager = rel_manager_new_with_namespace
			(self->context, self->namespace);
	self->dom_manager = dom_manager_new_with_namespace (self->namespace);
	self->fun_manager = fun_manager_new_with_namespace (self->namespace);
	self->prog_manager = prog_manager_new_with_namespace (self->namespace);

	/* Labels belong to the Relview objects and must not be destroyed by
	 * somebody else! */
	self->labels = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify)_label_wrapper_destroy);

	self->label_assocs = g_hash_table_new_full (_label_assoc_key_hash,
			_label_assoc_key_equal, NULL, g_free);

	return self;
}

Relview * _rv_new ()
{
	KureContext * c = kure_context_new ();
	Relview * self = _rv_new_with_context (c);
	kure_context_deref(c);
	return self;
}

void rv_destroy (Relview * rv)
{
	VERBOSE(VERBOSE_DEBUG, printf ("___rv_destroy (CLEANUP)________________________________\n");)

	g_hash_table_destroy (rv->labels);

	workspace_destroy (rv_get_workspace(rv));
	rel_manager_destroy(rv_get_rel_manager(rv));
	dom_manager_destroy(rv_get_dom_manager(rv));
}

/*! Update the Relview object if necessary. This is a callback function for
 * the prefs observer. Don't call is directly.
 *
 * \author stb
 * \date 12.08.2009
 */
void _rv_prefs_listener (Relview * self, const
		gchar * section, const gchar * key)
{
	if (g_str_equal (section, "settings")) {
		if (g_str_equal (key, "debug_information"))
			rv_set_verbosity (self, prefs_get_int("settings", "debug_information", FALSE) ? 1 : 0);

		if (g_str_equal (key, "use_default_labels"))
			RELVIEW_OBSERVER_NOTIFY(self, labelAssocsChanged, _0());
	}
}

/*!
 * Returns the current verbosity level. The higher the value, the more
 * verbose the system is. 0 means to be quiet. */
gint rv_get_verbosity (Relview * self)
{
	return self->verbosity;
}

/*!
 * Set the verbosity level by hand. See rv_get_verbosity for details.
 */
void rv_set_verbosity (Relview * self, gint level)
{
	self->verbosity = level;
}

/*!
 * Initializes the RelView-Singleton Instance. Necessary for some
 * other functionalities. Lets the argv string unchanged. */
void rv_init (int argc, char ** argv)
{
    Relview * self = rv_get_instance ();
    self->argc = argc;
    self->argv = argv;
    self->is_init = TRUE;

   	/* Determine the distribution's directory. */
   	{
   		gchar * bin_dir = g_path_get_dirname (self->argv[0]);
   		gchar * dist_dir = g_path_get_dirname (bin_dir);

   		if (g_path_is_absolute(dist_dir))
   			self->abs_dist_dir = g_strdup (dist_dir);
   		else {
   			gchar * cwd = g_get_current_dir ();
   			self->abs_dist_dir = g_build_filename (cwd, dist_dir, NULL);
   			g_free (cwd);
   		}

   		g_free (bin_dir);
   		g_free (dist_dir);
   	}

    /* Register the \ref IOHandler for .xdd, .prog, .eps and
     * label files. */
   	rv_register_io_handler (self, IO_HANDLER(xdd_get_handler()));
   	rv_register_io_handler (self, IO_HANDLER(prog_get_handler()));
   	rv_register_io_handler (self, IO_HANDLER(label_get_handler()));
   	rv_register_io_handler (self, IO_HANDLER(eps_get_handler()));
    rv_register_io_handler (self, IO_HANDLER(pdf_get_handler()));
}

/*!
 * Returns the fill path (abs+rel) if the file exists, NULL otherwise.
 * The returned string, if non null, must be freed with g_free after use.
 * 'mid' may be NULL. */
static gchar * _test_file_exists (const gchar * abs_path, const gchar * mid, const gchar * rel_path)
{
    gchar * path;
    gboolean res;

    if (mid) path = g_build_filename (abs_path, mid, rel_path, NULL);
    else path = g_build_filename (abs_path, rel_path, NULL);
    res = g_file_test(path, G_FILE_TEST_EXISTS);
    VERBOSE(VERBOSE_DEBUG, printf("Testing if '%s' exists ... %s\n", path, res?"yes":"no");)
    if (res) return path;
    else { g_free(path); return NULL; }
}

/*!
 * Checks if a file with the given name is somewhere around. Returns the
 * full path to it, or NULL in case if the file is not found. Filename
 * must be a relative path or basename. The returned string, if not NULL,
 * must be freed with g_free. */
gchar * rv_find_data_file (const gchar * fname)
{
    Relview * self = rv_get_instance ();
    gchar * cwd = NULL;

    assert (self->is_init);

    cwd = g_get_current_dir ();
    if (! cwd) {
    	g_warning ("rv_find_data_file: Unable to retrieve current working directory.");
        return NULL;
    }
    else {
        gchar * path = NULL, * bindir = NULL, *distdir = NULL;
        gboolean found = FALSE;

#define TEST(a,b,c) if(!found){path = _test_file_exists((a),(b),(c)); found = (path!=NULL);}
        /* Check the current working directory. */
        TEST(cwd, "data", fname);
        TEST(cwd, "share", fname);

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)

        /* Check the distribution's data dir. */
        TEST(self->abs_dist_dir, "data/"PACKAGE_NAME"-"TO_STRING(RELVIEW_MAJOR_VERSION)"."TO_STRING(RELVIEW_MINOR_VERSION), fname);
        TEST(self->abs_dist_dir, "share/"PACKAGE_NAME"-"TO_STRING(RELVIEW_MAJOR_VERSION)"."TO_STRING(RELVIEW_MINOR_VERSION), fname);

#undef TO_STRING
#undef _TO_STRING

        /* Check the installation data dir. */
        TEST(RELVIEW_DATADIR, NULL, fname);

        /* Check the directory of the executable. */
        bindir = g_path_get_dirname (self->argv[0]);
        distdir = g_path_get_dirname (bindir);
        if (bindir) {
            TEST(distdir, "data", fname);
            TEST(distdir, "share", fname);
        } else g_warning ("rv_find_data_file: Unable to retrieve bin-dir.");
#undef TEST

        g_free (cwd); g_free (bindir); g_free (distdir);
        return path;
    }
}

/*!
 * Looks up a given file. Search order is:
 *    1. RELVIEWSAVEPATH in the environment
 *    2. Current working directory.
 *    3. Home directory.
 *
 * Returns NULL is the file couldn't be found. Returns a newly allocated string
 * otherwise.
 */
gchar * rv_find_startup_file (const gchar * fname)
{
	gchar * save_path = getenv ("RELVIEWSAVEPATH");
	gchar cwd [PATH_MAX] = {0};
	const struct passwd * pw_entry = NULL;

	if (save_path) {
		gchar * full_path = g_build_filename(save_path, fname, NULL);
		if (g_file_test (full_path, G_FILE_TEST_EXISTS))
			return full_path;
		g_free (full_path);
	}

	if (getcwd(cwd, PATH_MAX-1)) {
		gchar * full_path = g_build_filename(cwd, fname, NULL);
		if (g_file_test (full_path, G_FILE_TEST_EXISTS))
			return full_path;
		g_free (full_path);
	}

	pw_entry = getpwuid(getuid());
	if (pw_entry) {
		gchar * full_path = g_build_filename(pw_entry->pw_dir, fname, NULL);
		if (g_file_test (full_path, G_FILE_TEST_EXISTS))
			return full_path;
		g_free (full_path);
	}

	return NULL;
}

GtkBuilder * rv_get_gtk_builder (Relview * self)
{
	if (! self->gtk_builder) {
		/* Build the user interface from GtkBuilder file. */
#define RELVIEW_UI_SPECS_FILE "relview.glade"
		gchar * file = rv_find_data_file (RELVIEW_UI_SPECS_FILE);
		GError * err = NULL;

		if ( !file) {
			/* The UI specs were not found. Give the user something to trace
			 * this problem. */
			self->verbosity = VERBOSE_ALL;
			rv_find_data_file (RELVIEW_UI_SPECS_FILE);

			g_error ("rv_get_gtk_builder: Sorry! I am unable to find the UI specs '"
					RELVIEW_UI_SPECS_FILE"'.");
			/* never reached. */
		}

		VERBOSE(VERBOSE_DEBUG, printf("Loading UI specs from '%s'.\n", file););

		self->gtk_builder = gtk_builder_new ();
		if (! gtk_builder_add_from_file (self->gtk_builder, file, &err)) {
			g_log ("relview", G_LOG_LEVEL_ERROR, "Unable to load GtkBuilder "
					"UI Specs from file %s. Reason: %s", RELVIEW_UI_SPECS_FILE,
					err->message);
			/* terminates */
		}
		g_free (file);

		gtk_builder_connect_signals (self->gtk_builder,NULL);

	}

	return self->gtk_builder;
}

KureContext * rv_get_context (Relview * self) { return self->context; }

const GSList * rv_get_default_plugin_dirs (Relview * self)
{
	static GSList * plugin_dirs = NULL;

    assert (self->is_init);

	if (! plugin_dirs) {
		const gchar * homedir = g_get_home_dir ();

		/* First is least, last is most important. */

		plugin_dirs = g_slist_prepend (plugin_dirs,
				g_build_filename (RELVIEW_DATADIR,"plugins",NULL));

#define _xstr_(x) #x
#define _xstr(x) _xstr_(x)
		plugin_dirs = g_slist_prepend (plugin_dirs,
				g_build_filename (homedir,".relview-"_xstr(RELVIEW_MAJOR_VERSION)"."_xstr(RELVIEW_MINOR_VERSION)"/plugins",NULL));
#undef _xstr
#undef _xstr_
	}

	return plugin_dirs;
}

void rv_register_io_handler (Relview * self, IOHandler * handler)
{
	if ( !g_list_find (self->io_handlers, handler)) {
		verbose_printf (VERBOSE_TRACE, __FILE__": Registering IO handler \"%s\".\n",
				io_handler_get_name(handler));
		self->io_handlers = g_list_append (self->io_handlers, handler);
		RELVIEW_OBSERVER_NOTIFY(self,IOHandlersChanged,_0());
	}
}

gboolean rv_unregister_io_handler (Relview * self, IOHandler * handler)
{
	GList * iter = g_list_find (self->io_handlers, handler);
	if ( !iter) return FALSE;
	else {
		self->io_handlers = g_list_delete_link (self->io_handlers, iter);
		io_handler_destroy (handler);
		RELVIEW_OBSERVER_NOTIFY(self,IOHandlersChanged,_0());

		if (g_verbosity >= VERBOSE_DEBUG) {
			GList * iter = self->io_handlers;
			printf ("IO Handlers:\n");
			for ( ; iter ; iter = iter->next) {
				printf ("\t\"%s\" (%p)\n", io_handler_get_name (iter->data), iter->data);
			}
		}

		return TRUE;
	}
}

GList/*<IOHandler*>*/ * rv_get_io_handler_for_action (Relview * self, IOHandlerActions a)
{
	GList * ret = NULL;
	GList * iter = self->io_handlers;
	for ( ; iter ; iter = iter->next) {
		if (io_handler_supports_action (IO_HANDLER(iter->data), a))
			ret = g_list_prepend (ret, iter->data);
	}

	return g_list_reverse (ret);
}

GList/*<IOHandler*>*/ * rv_get_io_handler_for_any_action (Relview * self, IOHandlerActions a)
{
	GList * ret = NULL;
	GList * iter = self->io_handlers;
	for ( ; iter ; iter = iter->next) {
		if (io_handler_supports_any_action (IO_HANDLER(iter->data), a))
			ret = g_list_prepend (ret, iter->data);
	}

	return g_list_reverse (ret);
}

IOHandler * rv_get_io_handler_for_file_and_action(Relview * self,
		const gchar * filename, IOHandlerActions action)
{
	GList * iter = self->io_handlers;
	for ( ; iter ; iter = iter->next) {
		IOHandler * cur = IO_HANDLER(iter->data);
		if (io_handler_matches (cur, filename)
				&& io_handler_supports_action(cur, action)) return cur;
	}

	return NULL;
}

MenuManager * rv_get_menu_manager (Relview * self) { return self->menu_manager; }

void rv_register_observer (Relview * self, RelviewObserver * o)
{
	if (NULL == g_slist_find (self->observers, o)) {
		self->observers = g_slist_prepend (self->observers, o);
	}
}

void rv_unregister_observer (Relview * self, RelviewObserver * o)
{ self->observers = g_slist_remove (self->observers, o); }


const gchar * rv_get_object_type_name (RvObjectType type)
{
	static gchar unknown [] = "Unknown";
	static struct {
		RvObjectType type;
		gchar * name;
	} table[] = { {RV_OBJECT_TYPE_RELATION, "Relation"},
			{ RV_OBJECT_TYPE_GRAPH, "Graph" },
			{ RV_OBJECT_TYPE_DOMAIN, "Domain" },
			{ RV_OBJECT_TYPE_FUNCTION, "Function" },
			{ RV_OBJECT_TYPE_PROGRAM, "Program" },
			{ RV_OBJECT_TYPE_LABEL, "Label" },
			{ RV_OBJECT_TYPE_OTHER, "Other" }, {0,NULL}}, *ptr;

	for (ptr = table ; ptr->name ; ptr++) {
		if (type == ptr->type) return ptr->name;
	}
	g_warning ("rv_get_object_type_name: Unknown type: %d.\n", type);
	return unknown;
}


/*******************************************************************************
 *                         RelView Language Operations                         *
 *                                                                             *
 *                              Tue, 13 Apr 2010                               *
 ******************************************************************************/

static KureDom * _dom_to_kure_dom (Relview * rv, lua_State * L, Dom * dom)
{
	KureDom * kdom;
	KureError * kerr = NULL;
	KureRel * R, *S;
	mpz_t a,b, x,y;
	const char * error_title = "Invalid or incomplete domain";

	R = kure_lua_exec(L, dom_get_first_comp_lua(dom), &kerr);
	if ( !R) {
		rv_user_error_with_descr (error_title, kerr->message,
				"Unable to load the first component of domain \"%s\". We "
				"ignore it and try without, but this could trigger some "
				"other errors.",
				dom_get_name(dom));
		kure_error_destroy(kerr);
		return NULL;
	}

	S = kure_lua_exec(L, dom_get_second_comp_lua(dom), &kerr);
	if ( !S) {
		rv_user_error_with_descr (error_title, kerr->message,
				"Unable to load the second component of domain \"%s\". We "
				"ignore it and try without, but this could trigger some "
				"other errors.",
				dom_get_name(dom));
		kure_error_destroy(kerr);
		kure_rel_destroy(R);
		return NULL;
	}

	mpz_init (a); mpz_init (b); mpz_init(x); mpz_init(y);
	kure_rel_get_rows(R, a);
	kure_rel_get_cols(R, x);

	kure_rel_get_cols(S, b);
	kure_rel_get_rows(S, y);

	if (mpz_cmp(a,x) != 0) {
		rv_user_error (error_title,
				"Number of rows/cols of the first component of domain "
				"\"%s\" differ. We ignore this and use the rows!",
				dom_get_name(dom));
	}
	if (mpz_cmp(b,y) != 0) {
		rv_user_error (error_title,
				"Number of rows/cols of the second component of domain "
				"\"%s\" differ. We ignore this and use the cols!",
				dom_get_name(dom));
	}

	kure_rel_destroy(R);
	kure_rel_destroy(S);

	switch (dom_get_type(dom)) {
	case DIRECT_PRODUCT: kdom = kure_direct_product_new(a,b); break;
	case DIRECT_SUM: kdom = kure_direct_sum_new(a,b); break;
	default:
		fprintf (stderr, "Unknown domain type.\n");
		abort();
	}

	mpz_clear(a); mpz_clear(b);
	return kdom;
}

lua_State * rv_lang_new_state (Relview * rv)
{
	lua_State * L = kure_lua_new (rv->context);

	FunManager * fm = fun_manager_get_instance();
	ProgManager * pm = prog_manager_get_instance();
	RelManager * rm = rv_get_rel_manager(rv);
	DomManager * dm = rv_get_dom_manager(rv);

#warning TODO: The error message dumps the Lua code. This conflicts with pre-compiled code!

	FOREACH_FUN(fm, cur, iter, {
		size_t size;
		const gchar * buf = fun_get_luacode (cur, &size);
		/* Note: Same as for programs below! */
		int error = luaL_loadbuffer (L, buf, size, buf);
		VERBOSE(VERBOSE_PROGRESS, printf ("Loading function \"%s\" into state.\n", fun_get_name (cur));)
		error = error || lua_pcall (L, 0, 0, 0);
		if (error) {
			const char * err_msg = lua_tostring (L,-1);
			rv_user_error("Unable to create Lua state",
					"Unable to load function \"%s\" into Lua. "
					"Reason: %s Code was \"%s\"", fun_get_name(cur), err_msg, buf);
			kure_lua_destroy(L);
			return NULL;
		}
	});

	FOREACH_PROG(pm, cur, iter, {
		size_t size;
		const gchar * name = prog_get_name (cur);
		const gchar * buf = prog_get_luacode (cur, &size);
		/* Note: It is necessary to pass the plain source code as the last
		 *       argument here. The last argument is the data which is used
		 *       in Lua's lua_Debug->source field! */
		int error = luaL_loadbuffer (L, buf, size, buf);
		VERBOSE(VERBOSE_PROGRESS, printf ("Loading program \"%s\" into state.\n", name);)
		error = error || lua_pcall (L, 0, 0, 0);
		if (error) {
			const char * err_msg = lua_tostring (L,-1);
			rv_user_error("Unable to create Lua state",
					"Unable to load program \"%s\" into Lua. "
					"Reason: %s Code was \"%s\"", name, err_msg, buf);
			kure_lua_destroy(L);
			return NULL;
		}
	});

	FOREACH_REL(rm, cur, iter, {
			VERBOSE(VERBOSE_PROGRESS, printf ("Loading relation \"%s\" into state.\n", rel_get_name(cur));)
		kure_lua_set_rel_copy(L, rel_get_name(cur), rel_get_impl(cur));
	});

	FOREACH_DOM(dm, cur, iter, {
		KureDom * kdom;
		VERBOSE(VERBOSE_PROGRESS, printf ("Loading domain \"%s\" into state.\n", dom_get_name(cur));)
		kdom = _dom_to_kure_dom(rv,L,cur);
		if (!kdom) {
			/* We ignore it. */
		}
		else {
			kure_lua_set_dom_copy(L, dom_get_name(cur), kdom);
			kure_dom_destroy(kdom);
		}
	});

	return L;
}

KureRel * rv_lang_eval (Relview * self, const char * expr, GError ** perr)
{
	static const char * var_name = "__var";

	lua_State * L = rv_lang_new_state (self);
	if ( !L) {
		g_set_error(perr, rv_error_domain(), 0, "Unable to create Lua state.");
		return NULL;
	}
	else {
		KureError * kerr = NULL;
		KureRel * impl = kure_lang_exec(L, expr, &kerr);
		if (!impl) {
			g_set_error_literal (perr, rv_error_domain(), 0, kerr->message);
			kure_error_destroy (kerr);
		}
		else kure_lua_destroy(L);

		return impl;
	}
}


GQuark rv_error_domain ()
{
	static GQuark domain = 0;
	if (!domain) domain = g_quark_from_string ("RelView-Error");
	assert (domain);
	return domain;
}


/*******************************************************************************
 *                          RelView User Interaction                           *
 *                                                                             *
 *                              Tue, 13 Apr 2010                               *
 ******************************************************************************/

/*! Asks the user for a valid relation name via a popup dialog.
 * The user is asked until he enters a valid relation name, or
 * he cancels the dialog. In the latter case FALSE will be returned.
 * If defIsEmpty is TRUE, "$" will be used, when the user left
 * name blank.
 *
 * \author stb
 * \param caption Will be shown as the dialog's title.
 * \param descr Description for the user, what to enter
 *              (e.g "Relation name:", "Vector name:").
 * \param name Will initially be set for the name in the dialog.
 *             The result will stored in the string, if the user
 *             doesn't cancel.
 * \param defIsEmpty If nothing, or only blanks are typed in by
 *                   the user, "$" is assumed for the relation
 *                   name.
 * \return FALSE, if the user canceled, TRUE otherwise.
 */
gboolean rv_ask_rel_name (const GString * caption, const GString * descr,
                          GString * /*inout*/ name,
                          gboolean defIsEmpty)
{
	RelManager * manager = rv_get_rel_manager(rv_get_instance());

	do {
		GtkBuilder * builder = rv_get_gtk_builder (rv_get_instance());
		GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogInputString"));
		GtkWidget * entry = GTK_WIDGET(gtk_builder_get_object(builder, "entryInput")),
			*label = GTK_WIDGET(gtk_builder_get_object(builder, "labelDescr"));
		gboolean ret = FALSE;
		GtkEntryCompletion * compl;
		GtkListStore * model;

		/* Set caption, descr. and default name. */
		gtk_window_set_title(GTK_WINDOW (dialog), caption->str);
		gtk_entry_set_max_length(GTK_ENTRY (entry), REL_MAX_NAME_LEN);
		gtk_entry_set_text(GTK_ENTRY (entry), name->str);
		gtk_label_set_text(GTK_LABEL (label), descr->str);

		/* Select the Text, so the user can easily replace it. */
		gtk_editable_select_region(GTK_EDITABLE(entry),
				0, gtk_entry_get_text_length(GTK_ENTRY(entry)));

		/* Build and setup the the completions ... */
		compl = gtk_entry_completion_new();
		model = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_entry_completion_set_model(compl, GTK_TREE_MODEL(model));
		gtk_entry_completion_set_text_column(compl, 0);
		{
			GtkTreeIter ti;

			/* Add all relations in the global manager to the list store. */
			FOREACH_REL(manager, cur, iter, {
				gtk_list_store_append(GTK_LIST_STORE(model), &ti);
				gtk_list_store_set(GTK_LIST_STORE(model), &ti, 0, rel_get_name(cur), -1);
			});

			gtk_entry_set_completion(GTK_ENTRY(entry), compl);
		}

		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW(dialog), rv_window_get_widget(rv_window_get_instance()));

		/* Run the dialog. */
		{
			gint resp = gtk_dialog_run(GTK_DIALOG (dialog));
			gtk_widget_hide(dialog);

			switch (resp) {
			case 1 /* see Glade */:
				g_string_assign(name, gtk_entry_get_text(GTK_ENTRY (entry)));
				ret = TRUE;
				break;
			default:
				ret = FALSE;
			}
		}

		/* Clean up some things. */
		gtk_entry_set_completion(GTK_ENTRY(entry), NULL);
		g_object_unref(G_OBJECT(model));
		g_object_unref(G_OBJECT(compl));

		/* If the user hasn't canceled the dialog, check the input, and
		 * bother the user on an invalid relation name. */
		if (ret) {
			/* remove leading and trailing whitespaces */
			name->str = g_strstrip (name->str);
			name->len = strlen(name->str);

			if (0 == name->len && defIsEmpty) {
				g_string_assign(name, "$");
				return TRUE;
			}

			if (namespace_is_valid_name(rv_get_namespace(rv_get_instance()),
					name->str)) {
				return TRUE;
			} else {
				gint respId;

				GtkWidget * dialog = gtk_message_dialog_new(NULL,
						GTK_DIALOG_MODAL /*flags*/,
						GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
						"The entered string \"%s\" is not a valid "
							"relation name.", name->str);
				gtk_dialog_add_buttons(GTK_DIALOG (dialog), "Try again", 1,
						NULL);
				gtk_message_dialog_format_secondary_text(
						GTK_MESSAGE_DIALOG (dialog),
						"Press \"Try again\" to enter another relation name, "
							"or press \"Cancel\" to cancel.");

				gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
				respId = gtk_dialog_run(GTK_DIALOG (dialog));
				gtk_widget_destroy(dialog);

				if (respId == GTK_RESPONSE_CANCEL) {
					return FALSE;
				} else
					/* try again */ continue;
			}
		}
		else {
			/* The has pressed the "Cancel" button. */
			return FALSE;
		}

	} while (TRUE);

	/* never reached. */
	g_assert (FALSE);

	return FALSE; /* avoid compiler warning. */
}

/* pstr may not be NULL. *pstr may be NULL. This will indicate an inital empty
 * relation name. */
gboolean rv_ask_rel_name_def (gchar ** pname, gboolean defIsEmpty)
{
  assert (pname != NULL);

  {
    GString * descr = g_string_new ("Relation name:"),
      * caption = g_string_new ("Enter relation name"),
      * name = g_string_new (*pname ? *pname : "");

    gboolean ret = rv_ask_rel_name (caption, descr, name, defIsEmpty);

    *pname = g_string_free (name, FALSE);
    g_string_free (descr, TRUE);
    g_string_free (caption, TRUE);

    return ret;
  }
}

// Replace, Keep, Keep all, Replace All. The latter are not always available.
// 'descr' may be NULL, but should not be empty. The 'descr' is added as
// secondary text to the dialog if given. If more is TRUE buttons "Replace all"
// and "Keep all" are displayed. In contrast, if more is FALSE, the result is
// compatible to RvReplaceAction.
RvReplacePolicy rv_user_ask_replace_object (Relview * rv, RvObjectType type,
		const gchar * name, const gchar * descr, gboolean more)
{
	gint respId;
	const gchar * type_name = rv_get_object_type_name (type);
	GtkWidget * parent = rv_window_get_widget(rv_window_get_instance());
	GtkWidget * dialog = gtk_message_dialog_new(parent,
			GTK_DIALOG_MODAL /*flags*/,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			"Replace?");
	if (more) gtk_dialog_add_button (GTK_DIALOG(dialog),
			"Keep all", RV_REPLACE_POLICY_KEEP_ALL);
	gtk_dialog_add_buttons(GTK_DIALOG (dialog),
			GTK_STOCK_NO, RV_REPLACE_POLICY_KEEP,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_YES, RV_REPLACE_ACTION_REPLACE, NULL);
	if (more) gtk_dialog_add_button (GTK_DIALOG(dialog),
			"Replace all", RV_REPLACE_POLICY_REPLACE_ALL);

	gtk_message_dialog_format_secondary_text(
				GTK_MESSAGE_DIALOG (dialog), "A %s with name \"%s\" already exists. "
				"Do you want to replace the existing %s? %s",
				type_name, name, type_name, descr?descr:"");

	gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	respId = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);

	if (respId == GTK_RESPONSE_CANCEL)
		respId = (more ? RV_REPLACE_POLICY_KEEP_ALL : RV_REPLACE_POLICY_KEEP);
	return respId;
}


// Replace, Keep, Keep all, Replace All. The latter are not always available.
// 'descr' may be NULL, but should not be empty. The 'descr' is added as
// secondary text to the dialog if given. If more is TRUE buttons "Replace all"
// and "Keep all" are displayed. In contrast, if more is FALSE, the result is
// compatible to RvReplaceAction.
RvReplacePolicy rv_user_ask_replace_object2 (Relview * rv, RvObject * obj,
		const gchar * descr, gboolean more)
{
	gint respId;
	RvObjectClass * c = rv_object_get_class(obj);
	const gchar * type_name = rv_object_class_get_type_name(c);
	const gchar * name = rv_object_get_name (obj);
	GtkWidget * parent = rv_window_get_widget(rv_window_get_instance());
	GtkWidget * dialog = gtk_message_dialog_new(parent,
			GTK_DIALOG_MODAL /*flags*/,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			"Replace?");
	if (more) gtk_dialog_add_button (GTK_DIALOG(dialog),
			"Keep all", RV_REPLACE_POLICY_KEEP_ALL);
	gtk_dialog_add_buttons(GTK_DIALOG (dialog),
			GTK_STOCK_NO, RV_REPLACE_POLICY_KEEP,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_YES, RV_REPLACE_ACTION_REPLACE, NULL);
	if (more) gtk_dialog_add_button (GTK_DIALOG(dialog),
			"Replace all", RV_REPLACE_POLICY_REPLACE_ALL);

	gtk_message_dialog_format_secondary_text(
				GTK_MESSAGE_DIALOG (dialog), "A %s with name \"%s\" already exists. "
				"Do you want to replace the existing %s? %s",
				type_name, name, type_name, descr?descr:"");

	gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	respId = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);

	if (respId == GTK_RESPONSE_CANCEL)
		respId = (more ? RV_REPLACE_POLICY_KEEP_ALL : RV_REPLACE_POLICY_KEEP);
	return respId;
}


gboolean rv_user_ask_remove_object (Relview * rv, RvObjectType type,
		const gchar * name, const gchar * descr)
{
	gint respId;
	const gchar * type_name = rv_get_object_type_name (type);
	GtkWidget * parent = rv_window_get_widget(rv_window_get_instance());
	GtkWidget * dialog = gtk_message_dialog_new(parent,
			GTK_DIALOG_MODAL /*flags*/,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			"Remove?");
	gtk_dialog_add_buttons(GTK_DIALOG (dialog),
			GTK_STOCK_NO, GTK_RESPONSE_NO,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_YES, GTK_RESPONSE_YES, NULL);

	gtk_message_dialog_format_secondary_text(
				GTK_MESSAGE_DIALOG (dialog), "Do you really want to remove "
				"the %s with name \"%s\"? %s",
				type_name, name, descr?descr:"");

	gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	respId = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);

	return (respId == GTK_RESPONSE_YES);
}


gchar * rv_user_new_relation (Relview * rv, const gchar * def_name,
			mpz_t cols, mpz_t rows)
{
	GtkBuilder * builder = rv_get_gtk_builder (rv);
	GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogRelationNew"));

	GtkEntry * entryRelName = GTK_ENTRY(gtk_builder_get_object(builder, "entryRelName")),
	      * entryRowCount = GTK_ENTRY(gtk_builder_get_object(builder, "entryRowCount")),
	      * entryColCount = GTK_ENTRY(gtk_builder_get_object(builder, "entryColCount"));
	gchar * ret = NULL;
	char * s = NULL;
	gint respId;

	/* Reset the focus. */
	gtk_widget_grab_focus (GTK_WIDGET(entryRelName));

	/* Restricting the length of relation names isn't necessary any more. */
	//gtk_entry_set_max_length (entryRelName, REL_MAX_NAME_LEN);

	gtk_entry_set_text (entryRelName, def_name ? def_name : "");

	if (mpz_cmp_si(cols, 0) != 0) {
		gmp_asprintf (&s, "%Zd", cols);
		gtk_entry_set_text (entryColCount, s);
		free (s);
	}
	else gtk_entry_set_text (entryColCount, "1");

	if (mpz_cmp_si (rows, 0) != 0) {
		gmp_asprintf (&s, "%Zd", rows);
		gtk_entry_set_text (entryRowCount, s);
		free (s);
	}
	else gtk_entry_set_text (entryRowCount, "1");

	gtk_window_set_transient_for (GTK_WINDOW(dialog),
			rv_window_get_widget(rv_window_get_instance()));

	respId = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	switch (respId) {
	case 1:
		mpz_set_str(cols, gtk_entry_get_text(entryColCount), 10/*base*/);
		mpz_set_str(rows, gtk_entry_get_text(entryRowCount), 10/*base*/);

		if ( !namespace_is_valid_name (rv_get_namespace(rv),
				gtk_entry_get_text(entryRelName))) {
			gint answerId;

			GtkWidget * message = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL /*flags*/,
					GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					"The entered string \"%s\" is not a valid "
						"relation name.", gtk_entry_get_text(entryRelName));
			gtk_dialog_add_buttons(GTK_DIALOG (message), "Try again", 1,	NULL);
			gtk_message_dialog_format_secondary_text(
					GTK_MESSAGE_DIALOG (message),
					"Press \"Try again\" to enter another relation name, "
						"or press \"Cancel\" to cancel.");

			gtk_window_set_position(GTK_WINDOW (message), GTK_WIN_POS_CENTER);
			answerId = gtk_dialog_run(GTK_DIALOG (message));
			gtk_widget_destroy(message);

			if (answerId == GTK_RESPONSE_CANCEL) {
				return NULL;
			}
			else {
				gchar * old_name = g_strdup (gtk_entry_get_text(entryRelName));
				ret = rv_user_new_relation(rv, old_name, cols, rows);
				g_free(old_name);
				return ret;
			}
		}
		else if (mpz_cmp_si (cols, 0) <= 0
				|| mpz_cmp_si (rows, 0) <= 0) /* strictly positive? */ {
			gint answerId;

			GtkWidget * message = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL /*flags*/,
					GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					"The entered dimension is invalid.");
			gtk_dialog_add_buttons(GTK_DIALOG (message), "Try again", 1,	NULL);
			gtk_message_dialog_format_secondary_text(
					GTK_MESSAGE_DIALOG (message),
					"Press \"Try again\" to change the dimension, "
						"or press \"Cancel\" to cancel.");

			gtk_window_set_position(GTK_WINDOW (message), GTK_WIN_POS_CENTER);
			answerId = gtk_dialog_run(GTK_DIALOG (message));
			gtk_widget_destroy(message);

			if (answerId == GTK_RESPONSE_CANCEL) {
				return NULL;
			}
			else {
				gchar * old_name = g_strdup (gtk_entry_get_text(entryRelName));
				ret = rv_user_new_relation(rv, old_name, cols, rows);
				g_free(old_name);
				return ret;
			}
		}
		else /* all valid. */ {
			ret = g_strdup (gtk_entry_get_text(entryRelName));
			VERBOSE(VERBOSE_INFO, gmp_printf ("rv_user_new_relation: rows=%Zd, cols=%Zd\n", cols, rows));
		}
		break;
	}

	return ret;
}

/* beachte fall, dass der name ungleich der temp-relation */
/* returns FALSE, if the user want to cancel. We assume the given name
 * is valid!
 *
 * Has added the relation to the global manager, if successful.
 */
gboolean rv_user_rename_or_not (Relview * self, Rel * rel)
{
	RelManager * manager = rv_get_rel_manager(self);
	Namespace * ns = rel_manager_get_namespace(manager);
	GString *caption = g_string_new("Enter relation name"), *descr =
			g_string_new("Relation name:"), *name = g_string_new(rel_get_name(rel));
	gboolean canceled = FALSE;
	RvObject * conflict = (RvObject*)namespace_get_by_name(ns, name->str);

	g_assert (rel_get_manager(rel) == NULL);

	while ( !g_str_equal(name->str, "$") && conflict)
	{
		int respId;
		const gchar * type_name = rv_object_class_get_type_name(rv_object_get_class(conflict));
		GtkWidget * parent = rv_window_get_widget(rv_window_get_instance());
		GtkWidget * dialog = gtk_message_dialog_new(parent,
				GTK_DIALOG_MODAL /*flags*/,
				GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
				"A %s with name \"%s\" already exists.",
				type_name, name->str);
		gtk_dialog_add_buttons(GTK_DIALOG (dialog), "Try again", 1,
				"Overwrite", 2, NULL);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (dialog),
				"Press \"Try again\" to enter another relation name, "
					"\"Overwrite\" to overwrite the current %s with name "
					"\"%s\" or press \"Cancel\" to do nothing.", type_name, name->str);
		gtk_window_set_position(GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
		respId = gtk_dialog_run(GTK_DIALOG (dialog));
		gtk_widget_destroy(dialog);

		if (respId == 1) /* try again */ {
			gboolean ret = rv_ask_rel_name(caption, descr, name, TRUE);

			if (!ret) /* user canceled */
			{
				canceled = TRUE;
				break;
			}
			else {
				rel_rename (rel, name->str);
				conflict = (RvObject*)namespace_get_by_name(ns, name->str);
			}
		}
		else if (respId == GTK_RESPONSE_CANCEL) {
			canceled = TRUE;
			break;
		}
		else if (respId == 2) /* overwrite */ {
			break;
		}
		else {
			g_assert (respId == 1 || respId == GTK_RESPONSE_CANCEL
					|| respId == 2);
		}
	} /* while */

	if (!canceled)
	{
		/* Delete the currently existing object. This can be necessary even
		 * if the user has not selected to overwrite it. E.g. in case of "$". */
		if (conflict)
			rv_object_destroy(conflict);

		rel_manager_insert(manager, rel);
	}

	g_string_free(name, TRUE);
	g_string_free(descr, TRUE);
	g_string_free(caption, TRUE);

	return !canceled;

}

void rv_user_message (const gchar * title, const gchar * text, ...)
{
	va_list ap;
	GtkWidget * message;
	gchar * real_text;

	va_start (ap, text);

	message = gtk_message_dialog_new (rv_window_get_widget(rv_window_get_instance()),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK, title);
	g_assert (message);

	if (text != NULL) {
		real_text = g_strdup_vprintf (text, ap);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s", real_text);
		g_free (real_text);
	}

	va_end (ap);

	gtk_window_set_position (GTK_WINDOW (message), GTK_WIN_POS_CENTER);
	gtk_dialog_run (GTK_DIALOG(message));
	gtk_widget_destroy (message);
}


#if 0
gchar * rv_user_ask_rel_name (const gchar * title)
{
	GString * caption = g_string_new ("Enter a relation name");
	GString * descr = g_string_new (title);
	GString * name = g_string_new ("");
	gboolean success;
	gchar * res = NULL;

	success = rv_ask_rel_name (caption, descr, name, TRUE);
	if (success) {
		res = name->str;
		g_string_free(name, FALSE);
	}
	else g_string_free(name, TRUE);

	g_string_free(descr, FALSE);
	g_string_free(caption, FALSE);

	return res;
}
#endif


void rv_user_error_markup (const gchar * title, const gchar * text, ...)
{
	va_list ap;
	GtkWidget * message;
	gchar * real_text;

	va_start (ap, text);

	message = gtk_message_dialog_new_with_markup (rv_window_get_widget(rv_window_get_instance()),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, title);
	g_assert (message);

	if (text != NULL) {
		real_text = g_strdup_vprintf (text, ap);
		gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (message), "%s", real_text);
		g_free (real_text);
	}

	va_end (ap);

	gtk_window_set_position (GTK_WINDOW (message), GTK_WIN_POS_CENTER);
	gtk_dialog_run (GTK_DIALOG(message));
	gtk_widget_destroy (message);
}

void rv_user_error (const gchar * title, const gchar * text, ...)
{
	va_list ap;
	GtkWidget * message;
	gchar * real_text;

	va_start (ap, text);

	message = gtk_message_dialog_new (rv_window_get_widget(rv_window_get_instance()),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, title);
	g_assert (message);

	if (text != NULL) {
		real_text = g_strdup_vprintf (text, ap);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s", real_text);
		g_free (real_text);
	}

	va_end (ap);

	gtk_window_set_position (GTK_WINDOW (message), GTK_WIN_POS_CENTER);
	gtk_dialog_run (GTK_DIALOG(message));
	gtk_widget_destroy (message);
}


void rv_user_error_with_descr (const char * title, const gchar * descr,
		const gchar * text, ...)
{
	va_list ap;
	GtkWidget * message;
	gchar * real_text;

	va_start (ap, text);

	message = gtk_message_dialog_new (rv_window_get_widget(rv_window_get_instance()),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, title);
	g_assert (message);

	if (text != NULL) {
		real_text = g_strdup_vprintf (text, ap);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
				"%s", real_text);
		g_free (real_text);
	}

	va_end (ap);

	/* Add the scrollable text box. */
	{
		GtkWidget * box = gtk_dialog_get_content_area (GTK_DIALOG(message));
		GtkWidget * textview = gtk_text_view_new ();
		GtkWidget * scrolled = gtk_scrolled_window_new (NULL, NULL);
		GtkTextBuffer * buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(textview));

		gtk_text_view_set_editable (GTK_TEXT_VIEW(textview),FALSE);
		gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);

		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_ETCHED_IN);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

		gtk_container_add(GTK_CONTAINER(scrolled), textview);
		gtk_box_pack_end_defaults(GTK_BOX(box), scrolled);
		gtk_widget_show_all (scrolled);

		gtk_text_buffer_set_text (buffer, descr, strlen(descr));
	}

	gtk_window_set_resizable (GTK_WINDOW(message), TRUE);
	gtk_window_set_position (GTK_WINDOW (message), GTK_WIN_POS_CENTER);
	gtk_window_resize(GTK_WINDOW(message), 550,300);
	gtk_dialog_run (GTK_DIALOG(message));
	gtk_widget_destroy (message);
}

gboolean rv_user_edit_domain (Relview * rv, GString * name,
		DomainType *ptype, GString * comp1, GString * comp2)
{
	if ( !name || !comp1 || !comp2 || !ptype) return FALSE;
	else {
		GtkBuilder * builder = rv_get_gtk_builder (rv);
		GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogEditDom"));
		GtkEntry *nameEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entryNameEditDom")),
				*firstEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entryFirstCompEditDom")),
				*secondEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entrySecondCompEditDom"));
		GtkComboBox * typeBox = GTK_COMBO_BOX(gtk_builder_get_object(builder, "comboboxTypeEditDom"));
		gint respId;


		/* Presets from the arguments. */
		gtk_entry_set_max_length (nameEntry, REL_MAX_NAME_LEN);
		gtk_entry_set_text (nameEntry, name->str);

		gtk_entry_set_text (firstEntry, comp1->str);
		gtk_entry_set_text (secondEntry, comp2->str);

		/* Select the given domain type from the combo box. */
		{
			GtkTreeModel * model = gtk_combo_box_get_model(typeBox);
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first(model,&iter)) {
				do {
					DomainType cur_type;
					gtk_tree_model_get (model, &iter, 1, &cur_type, -1);
					if (cur_type == *ptype) {
						gtk_combo_box_set_active_iter(typeBox, &iter);
						break;
					}
				} while (gtk_tree_model_iter_next(model, &iter));
			}
		}

		gtk_window_set_transient_for (GTK_WINDOW(dialog), rv_window_get_widget(rv_window_get_instance()));
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

		respId = gtk_dialog_run (GTK_DIALOG(dialog));
		gtk_widget_hide (dialog);

		if (1 /* see Glade */ == respId) {
			GtkTreeIter iter;
			gboolean has_sel = gtk_combo_box_get_active_iter (typeBox, &iter);
			g_string_assign (name, gtk_entry_get_text(nameEntry));
			g_string_assign (comp1, gtk_entry_get_text(firstEntry));
			g_string_assign (comp2, gtk_entry_get_text(secondEntry));

			if (! has_sel) {
				rv_user_error ("Warning", "Please select a type of cancel the operation.");
				return rv_user_edit_domain(rv,name,ptype,comp1,comp2);
			}
			else {
				GtkTreeModel * model = gtk_combo_box_get_model (typeBox);
				gtk_tree_model_get (model, &iter, 1, ptype, -1);
			}

			if (! namespace_is_valid_name(rv_get_namespace(rv), name->str)) {
				rv_user_error ("Invalid domain name", "The name \"%s\" is not "
						"a valid domain name. Please choose another name of cancel.",
						name->str);
				return rv_user_edit_domain(rv,name,ptype,comp1,comp2);
			}
			else return TRUE;
		}
		else /* Cancel of something */ {
			return FALSE;
		}
	}
}


// See \ref rv_user_edit_function
struct edit_fun_info
{
	gboolean is_connected;
	gulong key_handler, mouse_handler;
};

// See \ref rv_user_edit_function
static gboolean _clear_entry_and_disconnect (GtkEntry * entry,
		gpointer * event, struct edit_fun_info * info)
{
	if (info->is_connected) {
		g_signal_handler_disconnect (G_OBJECT(entry), info->key_handler);
		g_signal_handler_disconnect (G_OBJECT(entry), info->mouse_handler);

		gtk_entry_set_text(entry, "");
		info->is_connected = FALSE;
	}

	return FALSE; // propagate
}

gboolean rv_user_edit_function (Relview * rv, GString * def)
{
	if (! rv || ! def) return FALSE;
	else {
		GtkBuilder * builder = rv_get_gtk_builder (rv);
		GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object (builder, "dialogEditFun"));
		GtkEntry * defEntry = GTK_ENTRY(gtk_builder_get_object(builder, "entryFunEditFun"));
		gint respId;

		struct edit_fun_info info = { FALSE, 0,0 };

		/* Set the default focus. Even this is done in Glade, the focus remains
		 * if the widget is closed. E.g. if the user clicks "Close", the next
		 * time the widget is shown, "Close" has still the focus. */
		gtk_widget_grab_focus (GTK_WIDGET(defEntry));

		/* If there is no function, display a hint how to input a function.
		 * However, as soon as the user types a function, remove that hint. */
		if (g_str_equal(def->str, "")) {
			info.mouse_handler = g_signal_connect(G_OBJECT(defEntry), "button-press-event",
					G_CALLBACK(_clear_entry_and_disconnect), (gpointer)&info);
			info.key_handler = g_signal_connect(G_OBJECT(defEntry), "key-press-event",
					G_CALLBACK(_clear_entry_and_disconnect), (gpointer)&info);
			info.is_connected = TRUE;
			gtk_entry_set_text (defEntry, "<NAME>(<ARGS>) = <DEF>");
		}
		else gtk_entry_set_text (defEntry, def->str);

		gtk_window_set_transient_for (GTK_WINDOW(dialog), rv_window_get_widget(rv_window_get_instance()));
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

		respId = gtk_dialog_run (GTK_DIALOG(dialog));
		gtk_widget_hide (dialog);

		if (info.is_connected) {
			g_signal_handler_disconnect (G_OBJECT(defEntry), info.key_handler);
			g_signal_handler_disconnect (G_OBJECT(defEntry), info.mouse_handler);
			info.is_connected = FALSE;
		}

		if (1 /*see Glade*/ == respId) /* Save was pressed. */ {
			GError * err = NULL;
			Fun * tmp_fun;
			g_string_assign (def, gtk_entry_get_text(defEntry));

			tmp_fun = fun_new (def->str, &err);
			if (! tmp_fun) {
				rv_user_error_with_descr ("Invalid Definition", err->message,
						"Your definition \"%s\" is not a valid function. "
						, def->str);
				g_error_free (err);

				return rv_user_edit_function (rv, def);
			}
			else {
				fun_destroy (tmp_fun);
				return TRUE;
			}
		}
		else return FALSE;
	}
}

gboolean rv_user_get_random_fill_percentage (Relview * rv, gfloat * /*out*/ pvalue)
{
	static gboolean first_call = TRUE;

	GtkBuilder * builder = rv_get_gtk_builder(rv);
	GtkWidget * dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogRandomFill"));
	GtkWidget * hscale = GTK_WIDGET(gtk_builder_get_object(builder, "hscaleProbRandom"));
	gboolean ret = FALSE;

	assert (pvalue != NULL);

	/* Even if the default value is set in Glade it doesn't seem to be set in
	 * the scale at startup. Hence, we set it here manually. */
	if (first_call) {
		first_call = FALSE;
		gtk_range_set_value (GTK_RANGE(hscale), 23.0);
	}

	{
		gint resp;

		gtk_window_set_transient_for (GTK_WINDOW(dialog), rv_window_get_widget(rv_window_get_instance()));
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

		resp = gtk_dialog_run(GTK_DIALOG (dialog));
		gtk_widget_hide(dialog);

		switch (resp) {
		case 1 /* see Glade */: {
			GtkAdjustment * adj = gtk_range_get_adjustment(GTK_RANGE (hscale));
			*pvalue = gtk_adjustment_get_value(adj) / 100.0;
			ret = TRUE;
			break;
		}
		default:
			ret = FALSE;
		}
	}

	return ret;
}


gchar * rv_user_format_large_int (mpz_t ents)
{
	char * s = NULL;
	gchar * ret = NULL;

	if (mpz_cmp_si (ents, (1<<30)) <= 0) {
		gmp_asprintf (&s, "%Zd", ents);
	}
	else {
		/* Only GMP float in conjunction with %Ef in gmp_asprintf can output
		 * numbers in scientific notation. */
		mpf_t f;
		mpf_init (f);
		mpf_set_z (f, ents);
		gmp_asprintf (&s, "%FE", f);
		mpf_clear (f);
	}

	if ( !s) ret = g_strdup ("");
	else ret = g_strdup (s);

	free (s);
	return ret;
}


gchar * rv_user_format_impl_dim (KureRel * impl)
{
	mpz_t n;
	gchar * rowsStr, *colsStr, *ret;
	mpz_init (n);
	kure_rel_get_rows (impl, n);
	rowsStr = rv_user_format_large_int (n);
	kure_rel_get_cols (impl, n);
	colsStr = rv_user_format_large_int (n);
	mpz_clear (n);
	ret = g_strdup_printf ("%s x %s", rowsStr, colsStr);
	g_free (rowsStr);
	g_free (colsStr);
	return ret;
}


static gchar * _replace_extension_by (const gchar * basename, const gchar * new_ext)
{
	gchar * last_dot = strrchr (basename, '.');
	if (! last_dot) return g_strconcat(basename, ".", new_ext, NULL);
	else {
		gchar * s = g_strdup (basename), *ret = NULL;
		gint len = last_dot - basename;
		s[len] = '\0';

		ret = g_strconcat (s, ".", new_ext, NULL);
		g_free (s);
		return ret;
	}
}


/* Belongs to rv_user_export_object. Called when the user selected an extension
 * from the list of possible extensions in the export file chooser dialog. */
static gboolean _update_extension(GtkWidget * treeview, GdkEventButton *event,
		GtkWidget * chooser)
{
	gchar * filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
	if (! filename) {
		g_warning ("Unable to update extension.");
		return FALSE;
	}
	else {
		GtkTreeSelection * sel = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeview));
        GtkTreeIter iter = {0};
        GtkTreeModel * model;

		if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
			IOHandler * handler = NULL;

			gtk_tree_model_get(model, &iter, 2/*col*/, &handler, -1);
			if (handler) {
				/* Remove the current extension, if there is one and add the
				 * new one. */
				gchar * basename = g_path_get_basename (filename);
				gchar * new_ext = io_handler_get_any_extension (handler);

				gchar * new_basename = _replace_extension_by (basename, new_ext);
				gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(chooser), new_basename);

				g_free (new_ext);
				g_free (new_basename);
				g_free (basename);
			} else { /* no handler */ }
		}

		g_free(filename);
		return FALSE;
	}
}


gboolean rv_user_export_object (IOHandlerActions action,
						   	  const gchar * title,
						   	  gpointer obj,
						   	  gchar* (*default_filename)(gpointer) /* due to "$" to "dollar" */,
						   	  gboolean (*export) (IOHandler*,const gchar*,gpointer,GError**))
{
	Relview * rv = rv_get_instance ();
	GList/*<IOHandler*>*/ * handlers = rv_get_io_handler_for_action (rv, action), *iter;
	if (! handlers) {
		rv_user_error ("Missing IO handler", "Sorry. No IO handlers available for this operation.");
		return FALSE;
	}
	else {
		gboolean success = FALSE;
		GtkBuilder * builder = rv_get_gtk_builder (rv);
		GtkWidget * extra_widget
		= GTK_WIDGET(gtk_builder_get_object (builder,"expanderFileExtensions"));
		GtkWidget * tv = gtk_bin_get_child (GTK_BIN(extra_widget));
		GtkWidget * chooser = gtk_file_chooser_dialog_new (title,
				rv_window_get_widget(rv_window_get_instance()),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				NULL);
		GtkListStore * list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)));
		gulong sig_handler_id;
		GtkTreeIter pos;
		gchar * def_filename = default_filename(obj);

		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(chooser), def_filename);
		g_free (def_filename);

		/* There may be options from previous uses of the extra widget. */
		gtk_list_store_clear (list_store);

		/* Default setting is to derive the extension from the given
					 * filename automatically. */
		gtk_list_store_append (list_store, &pos);
		gtk_list_store_set(list_store,&pos, 0, "By extension",
				1, "", 2, NULL, -1);

		for (iter = handlers ; iter ; iter = iter->next) {
			IOHandler * h = IO_HANDLER(iter->data);
			GtkTreeIter pos;
			gtk_list_store_append (list_store, &pos);
			gtk_list_store_set(list_store,&pos, 0, io_handler_get_name(h),
					1, io_handler_get_extensions(h),
					2, h,
					-1);
		}

		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(chooser), extra_widget);

		/* Update the filename's extension when the user selected an
		 * extension from the list. */
		sig_handler_id = g_signal_connect (G_OBJECT(tv), "button-release-event",
				G_CALLBACK(_update_extension), chooser);

		if (gtk_dialog_run(GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
		{
			gchar * filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
			gboolean really_export = TRUE;

			if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
				GtkMessageDialog * message = gtk_message_dialog_new
						(rv_window_get_widget(rv_window_get_instance()), GTK_DIALOG_MODAL,
								GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "File exists");
				gtk_message_dialog_format_secondary_text (message,
						"File \"%s\" already exists. Overwrite?", filename);
				if (gtk_dialog_run(GTK_DIALOG(message)) != GTK_RESPONSE_YES)
					really_export = FALSE;
				gtk_widget_destroy (GTK_WIDGET(message));
			}

			if (really_export) {
				IOHandler * handler = rv_get_io_handler_for_file_and_action (rv, filename, action);

				verbose_printf (VERBOSE_INFO, "Saving to \"%s\" ...\n", filename);

				if (! handler) {
					rv_user_error ("Unable to Export", "No appropriate IO handler "
							"available for filename \"%s\".", filename);
				}
				else {
					GError * err = NULL;
					gboolean success = export (handler, filename, obj, &err);
					if (! success) {
						rv_user_error ("Unable to Export", "Reason: %s", err->message);
						g_error_free (err);
					}
					else success = TRUE;
				}
			}

			g_free(filename);
		}

		/* Prevent the extra_widget from being destroyed. */
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(chooser), NULL);
		gtk_widget_destroy (chooser);

		/* Remove the signal handler from the extra widget */
		g_signal_handler_disconnect (G_OBJECT(tv), sig_handler_id);

		g_list_free (handlers);
		return success;
	}
}


gboolean rv_user_import_object (IOHandlerActions action,
						   	  const gchar * title,
						   	  gboolean (*import) (IOHandler*,const gchar*,GError**))
{
	Relview * rv = rv_get_instance ();
	gboolean success = FALSE;

	/* Query the available handlers. */
	GList/*<IOHandler*>*/ * handlers = rv_get_io_handler_for_action (rv, action), *iter;
	if (! handlers) {
		rv_user_message("No available handlers", "Sorry, there currently are no "
				"available handlers to import relations.");
		return FALSE;
	}
	else {
		GtkBuilder * builder = rv_get_gtk_builder (rv);
		GtkWidget * extra_widget = GTK_WIDGET(gtk_builder_get_object (builder,"expanderFileExtensions"));
		GtkWidget * tv = gtk_bin_get_child (GTK_BIN(extra_widget));
		GtkWidget * chooser	= gtk_file_chooser_dialog_new (title,
				rv_window_get_widget(rv_window_get_instance()),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
		GtkListStore * list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tv)));
		gulong sig_handler_id;
		GtkTreeIter pos;

		/* Don't suggest anything. */
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(chooser), "");

		/* Update the handlers in the tree view. */
		gtk_list_store_clear (list_store);

		gtk_list_store_append (list_store, &pos);
		gtk_list_store_set(list_store,&pos, 0, "By extension",
				1, "", 2, NULL, -1);

		for (iter = handlers ; iter ; iter = iter->next) {
			IOHandler * h = IO_HANDLER(iter->data);
			gtk_list_store_append (list_store, &pos);
			gtk_list_store_set(list_store,&pos, 0, io_handler_get_name(h),
					1, io_handler_get_extensions(h),
					2, h,
					-1);
		}

		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(chooser), extra_widget);

		/* Update the filename's extension when the user selected an
		 * extension from the list. */
		sig_handler_id = g_signal_connect (G_OBJECT(tv), "button-release-event",
				G_CALLBACK(_update_extension), chooser);

		if (gtk_dialog_run(GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
		{
			gchar * filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
			IOHandler * handler = rv_get_io_handler_for_file_and_action (rv, filename, action);

			verbose_printf (VERBOSE_INFO, "Loading from file \"%s\" ...\n", filename);

			if (! handler) {
				rv_user_error ("Unable to Import", "No appropriate IO handler "
						"available for filename \"%s\".", filename);
			}
			else {
				GError * err = NULL;
				gboolean success = import (handler, filename, &err);
				if (! success) {
					rv_user_error ("Unable to Import", "Reason: %s", err->message);
					g_error_free (err);
				}
				else success = TRUE;
			}

			g_free(filename);
		}

		/* Prevent the extra_widget from being destroyed. */
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(chooser), NULL);
		gtk_widget_destroy (chooser);

		/* Remove the signal handler from the extra widget */
		g_signal_handler_disconnect (G_OBJECT(tv), sig_handler_id);

		g_list_free (handlers);
	}

	return success;
}


gboolean rv_user_ask_label (Relview * rv, GString * /*input*/ labelName)
{
	gboolean ret = TRUE;
	GtkDialog * dialog = gtk_dialog_new_with_buttons ("Choose label",
			rv_window_get_widget(rv_window_get_instance()),
			GTK_DIALOG_MODAL,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	GtkVBox * vbox = gtk_vbox_new (FALSE, 2);
	GtkComboBox * cbLabels = gtk_combo_box_new_text ();
	gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER(dialog), 5/*px*/);
	gtk_box_pack_end (GTK_BOX(vbox), GTK_WIDGET(cbLabels), TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX(vbox), GTK_WIDGET(gtk_label_new("Choose label:")), TRUE, TRUE, 0);
	gtk_box_pack_start_defaults (GTK_BOX(gtk_dialog_get_content_area(dialog)), GTK_WIDGET(vbox));

	/* Fill the combo box. */
	{
		GList/*<Label*>*/ * labels = rv_label_list (rv), *iter = NULL;
		int selNo=0, i = 1;

		gtk_combo_box_append_text (cbLabels, "(none)");
		for ( iter = labels ; iter ; iter = iter->next, ++i) {
			const gchar * curName = label_get_name((Label*)iter->data);
			if (g_str_equal(curName, labelName->str))
				selNo = i;
			gtk_combo_box_append_text (cbLabels, curName);
		}
		gtk_combo_box_set_active (cbLabels, selNo);
		g_list_free (labels);
	}

	gtk_widget_show_all (GTK_WIDGET(dialog));
	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		gchar * selName = gtk_combo_box_get_active_text (cbLabels);
		if ( g_str_equal(selName, "(none)") || !rv_label_exists(rv, selName))
			g_string_assign (labelName, "");
		else g_string_assign (labelName, selName);
		g_free (selName);
	}
	else ret = FALSE;

	gtk_widget_destroy (GTK_WIDGET(dialog));
	return ret;
}

Workspace * rv_get_workspace (Relview * self) {	return self->workspace; }
Namespace * rv_get_namespace (Relview * self) { return self->namespace; }
RelManager * rv_get_rel_manager (Relview * self) { return self->rel_manager; }
DomManager * rv_get_dom_manager (Relview * self) { return self->dom_manager; }
FunManager * rv_get_fun_manager (Relview * self) { return self->fun_manager; }
ProgManager * rv_get_prog_manager (Relview * self) { return self->prog_manager; }

/*******************************************************************************
 *                                  RvObject                                   *
 *                                                                             *
 *                              Mon, 11 Oct 2010                               *
 ******************************************************************************/

void 			rv_object_destroy (RvObject * self) { self->c->destroy (self); }
const gchar * 	rv_object_get_name (RvObject * self) { return self->c->getName (self); }
RvObjectClass *	rv_object_get_class (RvObject * self) { return self->c; }

const gchar * 	rv_object_class_get_type_name (RvObjectClass * self) { return self->type_name; }


/*******************************************************************************
 *                                   Labels                                    *
 *                     (Labels for Relations and Graphs.)                      *
 *                                                                             *
 *                               Wed, 9 Feb 2011                               *
 ******************************************************************************/

static void _label_assocs_clear_invalid_containing_label (Relview * self, Label * ptr);


/*!
 * Labels have to be wrapped in order to have the \ref Relview object available
 * when a label is destroyed implicitely by the hash table.
 */
struct _LabelWrapper {
	Relview * rv;
	Label * label;
};

void _label_wrapper_destroy (LabelWrapper * self)
{
	verbose_printf (VERBOSE_DEBUG, __FILE__": _label_wrapper_destroy: Removing "
			"wrapper for label \"%s\" (%p).\n", label_get_name(self->label), self->label);

	_label_assocs_clear_invalid_containing_label (self->rv, self->label);
	label_destroy (self->label);
	g_free (self);
}


Label * rv_label_get_by_name (Relview * self, const gchar * name)
{
	LabelWrapper * wrapper = g_hash_table_lookup (self->labels, name);
	if (wrapper) return wrapper->label;
	else return NULL;
}




gboolean rv_label_exists (Relview * self, const gchar * name) {
	return rv_label_get_by_name(self,name) != NULL; }


gboolean rv_label_add (Relview * self, Label * label)
{
	if (label) {
		const gchar * name = label_get_name (label);

		if ( !rv_label_exists(self, name)) {
			LabelWrapper * wrapper = g_new0 (LabelWrapper, 1);
			wrapper->rv = self;
			wrapper->label = label;
			g_hash_table_insert (self->labels, g_strdup(name), wrapper);
			return TRUE;
		}
	}

	return FALSE;
}


void rv_label_remove_by_name (Relview * self, const gchar * name)
{
	Label * label = rv_label_get_by_name (self, name);

	if (label && !label_is_persistent(label)) {
		g_hash_table_remove (self->labels, name);
	}
}


GList/*<Label*>*/ * rv_label_list (Relview * self) {

	GList/*<LabelWrapper*>*/ * t = g_hash_table_get_values (self->labels), *iter;
	GList/*<Label>*/ * s = NULL;
	for (iter = t ; iter ; iter = iter->next)
		s = g_list_prepend (s, ((LabelWrapper*)iter->data)->label);
	g_list_free (t);
	return g_list_reverse (s);
}


/*!
 * Callback used in \ref rv_label_clear. Returns TRUE if the given label
 * is not persistent. The key and user_data is ignored.
 */
static gboolean _remove_if_not_persistent (gpointer key, gpointer value, gpointer user_data) {
	return !label_is_persistent (((LabelWrapper*)value)->label); }

void rv_label_clear (Relview * self)
{
	g_hash_table_foreach_remove(self->labels, _remove_if_not_persistent, NULL);
}


/*******************************************************************************
 *                              Label Association                              *
 *              (Associate a label to a Relation on a viewable.)               *
 *                                                                             *
 *                              Mon, 14 Feb 2011                               *
 ******************************************************************************/


LabelAssoc rv_label_assoc_get (Relview * self, Rel * rel, gpointer viewable)
{
	LabelAssoc assoc, key = {0}, *ptr = NULL;
	key.rel = rel;
	key.viewable = viewable;


	ptr = (LabelAssoc*)g_hash_table_lookup (self->label_assocs, &key);
	if (ptr) {
		assoc = *ptr;
	}
	else {
		/* Set default labels if necessary. Changes to this settings in the
		 * preferences are covered by Relview's preference observer. */
		const gchar * default_label_name = "Identity";
		gint use_default_labels = prefs_get_int ("settings", "use_default_labels", TRUE);
		assoc = key;

		if (use_default_labels) {
			assoc.labels[1] = assoc.labels[0] = rv_label_get_by_name (self, default_label_name);
			if ( !assoc.labels[0])
				g_warning ("rv_label_assoc_lookup: Defaut label \"%s\" "
						"does not exist.", default_label_name);
		}
		else {
			/* Both labels are NULL. */
		}
	}

	return assoc;
}


void rv_label_assoc_set (Relview * self, const LabelAssoc * assoc)
{
	LabelAssoc * ptr = g_hash_table_lookup (self->label_assocs, assoc);
	if (ptr) /* replace */ {
		*ptr = *assoc;
	}
	else {
		LabelAssoc * copy = g_new (LabelAssoc, 1);
		*copy = *assoc;
		g_hash_table_insert (self->label_assocs, copy, copy);
	}

	RELVIEW_OBSERVER_NOTIFY(self, labelAssocsChanged);
}


/*!
 * Remove all associations containing a given label.
 */
void _label_assocs_clear_invalid_containing_label (Relview * self, Label * ptr)
{
	GHashTableIter iter;
	LabelAssoc * key;
	int removed = 0;

	g_hash_table_iter_init (&iter, self->label_assocs);
	while (g_hash_table_iter_next (&iter, (gpointer*)&key, NULL)) {
		if (key->labels[0] == ptr || key->labels[1] == ptr) {
			g_hash_table_iter_remove (&iter);
			verbose_printf (VERBOSE_DEBUG, __FILE__": Removed label assoc. "
					"((\"%s\",%p),(%s,%s))\n", rel_get_name(key->rel), key->viewable,
					key->labels[0]?label_get_name(key->labels[0]):"(null)",
							key->labels[1]?label_get_name(key->labels[1]):"(null)");
			removed ++;
		}
	}

	if (removed > 0)
		RELVIEW_OBSERVER_NOTIFY(self, labelAssocsChanged, _0());
}
