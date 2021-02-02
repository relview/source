/*
 * main.c
 *
 *  Copyright (C) 1995-2010 Peter Schneider, Werner Lehmann and Stefan Bolus,
 *  University of Kiel, Germany
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

#include "config.h" // PACKAGE(_*)
#include "Relview.h"
#include "Relation.h"
#include "Function.h"
#include "Graph.h"
#include "Domain.h"
#include "Program.h"
#include "file_ops.h"
#include "label.h"
#include "FileWindow.h"
#include "RelviewWindow.h"
#include "prefs.h"
#include "msg_boxes.h"
#include "testwindow_gtk.h"
#include "EvalWindow.h"
#include "multiopwindow_gtk.h"
#include "random.h"
#include "GraphWindow.h"
#include "RelationWindow.h"
#include "FileLoader.h"
#include "pluginImpl.h" // PluginManager
#include "DirWindow.h"
#include "LabelWindow.h"
#include "PluginWindow.h"
#include "IterWindow.h"
#include "version.h"
#include "Graph.h" // xgraph_create_default

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <gtk/gtk.h>

#define PREF_FILE ".relview-prefs" /* relative path => from home dir */

/* List of names of file we try to load when the Relview is started. See also
 * rv_find_startup_file. */
static gchar * _start_up_files [] = { "start_up.xdd", "start_up.prog", NULL };


/*!
 * Command line options. Currently not in use.
 */
typedef struct _Options
{
	int dummy;
} Options;



/*!
 * Shows version information for the application.
 */
static void _show_version_information (FILE * fp)
{
	int bits = sizeof(void*) * 8;

	fprintf (fp, "Relview %d.%d.%d (%d-bits, CUDD "CUDD_VERSION")\n"
			"Copyright (C) 1993-2016 Rudolf Berghammer, University of Kiel, Germany\n"
			"License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
			"This is free software: you are free to change and redistribute it.\n"
			"There is NO WARRANTY, to the extent permitted by law.\n"
			"\n"
			"Visit <http://www.informatik.uni-kiel.de/~progsys/relview> for the most "
			"recent version.\n"
			"\n"
			"Credits (in alphabetical order):\n"
			"   Ralf Behnke, Stefan Bolus, Mitja Kulczynski, Werner Lehmann, Barbara\n"
            "   Leoniuk, Stefan Meier, Ulf Milanese, Peter Schneider, Bjoern Teegen,\n"
            "   Sascha Ulbrand, Hans-Rudolf Wittek\n"
			"\n", RELVIEW_MAJOR_VERSION, RELVIEW_MINOR_VERSION,  RELVIEW_MICRO_VERSION, bits);
}


/*!
 * Shows the given config value or the list of all available keys.
 * Returns false if the given key was invalid. The "help" key is
 * valid.
 */
gboolean _show_config_value (const char * config_key, FILE * fp)
{
	static const char path_separator = ':';

	if (g_str_equal(config_key, "help")) {
		fprintf (fp, "default-plugin-dirs, startup-files, prefs-file, version\n");
	}
	else if (g_str_equal (config_key, "default-plugin-dirs")) {
		const GSList/*<const gchar*>*/ * dirs = rv_get_default_plugin_dirs (rv_get_instance ()), *iter;
		for (iter = dirs ; iter ; iter = iter->next) {
			if (iter != dirs) fputc (path_separator, fp);
			fprintf (fp, "%s", (const gchar*)iter->data);
		}
		fputc ('\n', fp);
	}
	else if (g_str_equal(config_key, "startup-files")) {
		gboolean first = TRUE;
		gchar **start_up_file_ptr = _start_up_files;
		for ( ; *start_up_file_ptr ; ++start_up_file_ptr, first = FALSE) {
			if ( !first) fputc (path_separator, fp);
			fprintf (fp, "%s", *start_up_file_ptr);
		}
		fputc ('\n', fp);
	}
	else if (g_str_equal(config_key, "prefs-file")) {
		fprintf (fp, "%s\n", PREF_FILE);
	}
	else if (g_str_equal(config_key, "version")) {
		fprintf (fp, "%d.%d.%d\n", RELVIEW_MAJOR_VERSION,
				RELVIEW_MINOR_VERSION, RELVIEW_MICRO_VERSION);
	}
	else {
		/* Invalid option. Do nothing. */
		return FALSE;
	}
	return TRUE;
}


/*!
 * Parses the command line (options) and returns a corresponding options.
 * Terminates the program on some option. Returns true on success and false
 * on error. Does not throw an exception.
 *
 * \return True on success, false otherwise.
 */
static gboolean _parse_options (Options * opts, int argc, char ** argv)
{
	GError * error = NULL;
	GOptionContext * context = g_option_context_new (PACKAGE_NAME);
	gchar * font_id = NULL, * font_name = NULL;
	gboolean show_version = FALSE;
	gint verbosity = 0; // quiet
	gboolean be_quiet = FALSE;
	gchar * config_key = NULL;

	GOptionEntry entries[] = {
			{ "font", 'f', 0, G_OPTION_ARG_STRING, &font_id, "Load the given font from the preferences. [default=(none)]", "<id>" },
			{ "verbose", 'v', 0, G_OPTION_ARG_INT, &verbosity, "The higher the value, the more chatty is the program. [default=2]", "{0,1,..}" },
			{ "quiet", 0, 0, G_OPTION_ARG_NONE, &be_quiet, "Suppress any non-relevant output. Same as --verbose=0.", NULL },
			{ "version", 0, 0, G_OPTION_ARG_NONE, &show_version, "Show version information and exit.", NULL },
			{ "config", 'c', 0, G_OPTION_ARG_STRING, &config_key, "Show the given config value. [default=help]", NULL },
			{ NULL }
	};

	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_set_description (context, "Bug reports to stb <A> informatik.uni-kiel <D> de\n\n");

	if (!g_option_context_parse(context, &argc, &argv, &error))
	{
		fprintf (stderr, "Unable to parse command line. Reason: %s\n", error->message);
	   	g_error_free (error);
	   	return FALSE;
   }

	if (show_version) {
		_show_version_information (stderr);
		exit (0);
	}

	if (config_key) {
		gboolean success = _show_config_value (config_key, stdout);
		exit (success ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	/* Verbosity. Ignore --verbose=... and -v ... if --quiet is given. */
	if (be_quiet)
		g_verbosity = VERBOSE_QUIET;
	else {
		g_verbosity = (Verbosity) verbosity;
	}

	/* Load the given font or the default font. Fonts are mapped to font-specs
	 * in the preferences. If no font is goven, the font with name "default" is
	 * loaded. If no such font exists in the preferences (e.g. there are no
	 * preferences), don't load any font, but use the system default font. */
	if (font_id != NULL) {
		font_name = prefs_get_string ("fonts", font_id);
		if ( !font_name) {
			fprintf (stderr, "Invalid argument to --file/-f. Font with ID "
					"'%s' does not exist in your preferences.", font_id);
			g_free (font_id);
			return FALSE;
		}

		g_free (font_id);
	}
	else font_name = prefs_get_string ("fonts", "default");

	if (font_name) {
		gchar *rcstring = g_strdup_printf ("style \"large\" { font_name = \"%s\" }\n"
				"widget \"*\" style \"large\"", font_name);
		gtk_rc_parse_string (rcstring);

		g_free (rcstring);
		g_free (font_name);
	}

	return TRUE;
}


/****************************************************************************/
/*       NAME : init_gtk (was: init_xv)                                     */
/*    PURPOSE : Initializes variables, resources, GTK.                      */
/*    CREATED : 1995.02.05 PS                                               */
/*   MODIFIED : 1995.08.14 PS: draw_mode and popup menus                    */
/*              2000.06.01 WL: GTK+ port. The creation of windows, dialogs  */
/*                             and menus spread about nearly 2000 lines.    */
/*                             This has been divided into single modules.   */
/*                             Code only relevant for one window was moved  */
/*                             into dedicated subdirectories.               */
/****************************************************************************/
static void _create_windows (Relview * rv)
{
	Rel * dollar = NULL;
	XGraph * dollarGraph = NULL;

	_srandom (prefs_get_int ("settings", "randomseed", 0));

	/* First, create the directory window, so the default relation and the
	 * default graph are handled correctly. */
	dir_window_get_instance ();

	/* Create the default relation $ and the default graph $. */
	dollar = rel_manager_create_rel_si(rv_get_rel_manager(rv), "$", 32, 32);
	relation_window_set_relation(relation_window_get_instance(), dollar);

	dollarGraph = xgraph_create_default("$", DEFAULT_GRAPH_RADIUS);
	xgraph_manager_insert (xgraph_manager_get_instance(), dollarGraph);
	graph_window_set_graph(graph_window_get_instance(), dollarGraph);

	/* Create the global plug-in manager. This has to be done before the
	 * plugin window is created. */
	plugin_manager_create_instance();
	plugin_window_get_instance ();

	rv_window_get_instance();

	/* Create all remaining windows. */
	eval_window_get_instance ();
	test_window_init ();
	label_window_get_instance();
	iter_window_get_instance ();
	rvops_init ();
	file_window_get_instance ();
}

/****************************************************************************/
/*       NAME : free_resources                                              */
/*    PURPOSE : Frees all allocated resources -> prepares app shutdown      */
/*    CREATED : 13-JUN-2000 WL                                              */
/****************************************************************************/
void free_resources ()
{
#warning "free_resources deactivated."
	return;

  //    defdom_window_destroy ();
  //deffun_window_destroy ();
	iter_window_destroy_instance();
  label_window_destroy (label_window_get_instance());
  // TODO: Currently not handled.
#if 0
  multiop_window_destroy (& basicop_info);
  multiop_window_destroy (& resquot_info);
  multiop_window_destroy (& closure_info);
  multiop_window_destroy (& domain_info);
  multiop_window_destroy (& dirprod_info);
  multiop_window_destroy (& dirsum_info);
  multiop_window_destroy (& powerset_info);
#endif
  test_window_destroy ();
  // TODO: Must be replaced.
  //eval_window_destroy ();
  //dir_window_destroy ();
  graph_window_destroy_instance ();
  relation_window_destroy_instance ();
  //file_chooser_destroy ();

  rv_window_destroy_instance ();

  return;
}

static void _load_plugins (Relview * rv)
{
	/* A plugin isn't loaded only if the user disabled it and it is disabled when
	 * the program is closed. The state is saved to the preferences. By default,
	 * all unknown plugins are loaded. */

	PluginManager * manager = plugin_manager_get_instance ();
	const GList/*<Plugin*>*/ * plugins = plugin_manager_get_plugins (manager), *iter;

	for (iter = plugins ; iter ; iter = iter->next) {
		Plugin * cur = (Plugin*) iter->data;
		if (prefs_get_int("plugins", plugin_get_file(cur), TRUE)) {
			VERBOSE(VERBOSE_INFO, printf ("Enabling plugin '%s' (file %s).\n",
					plugin_get_name(cur), plugin_get_file(cur));)
			plugin_enable (cur);
		}
		else {
			VERBOSE(VERBOSE_INFO, printf("Plugin '%s' (file %s) is disabled "
					"by default.\n", plugin_get_name(cur), plugin_get_file(cur));)
		}
	}
}

static void _save_plugins_to_prefs (Relview * rv)
{
	PluginManager * manager = plugin_manager_get_instance ();
	const GList/*<Plugin*>*/ * plugins = plugin_manager_get_plugins (manager), *iter;

	for (iter = plugins ; iter ; iter = iter->next) {
		Plugin * cur = (Plugin*) iter->data;
		prefs_set_int ("plugins", plugin_get_file(cur),
				plugin_is_enabled(cur) ? 1 : 0);
	}
}


/*!
 * Default label function for the identity.
 *
 * \author stb
 * \date 07-MAR-08
 * \return Returns buf.
 */
static const gchar * _labelfunc_identity (int n, GString * s, void * user_data)
{
	g_string_printf (s, "%d", n);
	return s->str;
}


/*!
 * Returns the maximum length (in charaters) of all labels in a given range.
 * See \ref LabelMaxLenFunc for details.
 */
static int _labelfunc_identify_maxlen (int first, int last, void * user_data)
{
	return (int)ceil(log10(last));
}


/*!
 * Creates and adds the default labels to the \ref Relview object.
 *
 * \author stb
 * \date 09/Feb/2011
 */
void _create_default_labels (Relview * rv)
{
	Label * l = NULL;

	l = (Label*)label_func_new ("Identity",
			_labelfunc_identity, _labelfunc_identify_maxlen, NULL, NULL);
	label_set_persistent (l, TRUE);
	rv_label_add (rv, l);
}


/****************************************************************************/
/*       NAME : main                                                        */
/*    PURPOSE : Reads relations from a *.xrv file and displays them as graph*/
/*              in a window.                                                */
/*    CREATED : 1994.10.14 PS                                               */
/*   MODIFIED : 1995.01.25 PS                                               */
/*              2000.05.09 WL: GTK+ port. Changed to show the GTK main      */
/*                             window                                       */
/*              2001.03.28 UMI: First try to load .start_up.ddd, then .xrv  */
/*              2008.03.07 STB: Use default labels (default_label_list())   */
/****************************************************************************/
int main (int argc, char ** argv)
{
  Options opts = {0}; /* Set default values later. */
  Relview * rv = NULL;
  FileLoader * loader = NULL;
  gchar **start_up_file_ptr = _start_up_files;

  /* In order to filter both output streams (stderr ans stdout) with e.g. sed
   * or awk it's necessary to use at most line buffered stdout stream. */
  setvbuf (stdout, NULL, _IOLBF, 0);

  gtk_set_locale ();

#if ! defined G_THREADS_ENABLED
#  error "No GLib thread support available on your system. Sorry!"
#endif
  g_thread_init (NULL);
  gdk_threads_init();

  if (! g_module_supported()) {
	  g_log ("relview", G_LOG_LEVEL_ERROR,
			  "Loading modules is not supported on your system. Sorry.\n");
	  /* terminates. */
  }

  gtk_init (& argc, & argv);

  /* Load the preferences. */
  prefs_load (PREF_FILE);

  /* Creates the RelView object. Sets the command line arguments and installs
   * default IO handler. */
  rv_init (argc, argv);
  rv = rv_get_instance();

  /* Parse our own command-line options. Note that some options require the
   * preferences (e.g. '--font') */
  if ( !_parse_options(&opts, argc, argv)) {
	  prefs_clear ();
	  exit (1);
  }

  /* Create default labels. */
  _create_default_labels (rv);

  /* Create the windows in the workspace. */
  _create_windows (rv);

  /* ------------------------------------------------------ Load Plugins --- */

  _load_plugins (rv);

  /* ----------------------------------------------- Load start-up Files --- */

  loader = file_loader_new(rv);

  /* By default, we replace multiple occurrences of a named object. The last
   * occurrence remains. */
  file_loader_set_replace_policy (loader, RV_REPLACE_POLICY_REPLACE_ALL);

  for ( ; *start_up_file_ptr ; ++start_up_file_ptr) {
	  gchar * path = rv_find_startup_file (*start_up_file_ptr);
	  if (path) {
		  GError * err = NULL;

		  VERBOSE(VERBOSE_INFO, printf ("Loading start-up file \"%s\" ...\n", path););

		  if ( !file_loader_load_file (loader, path, &err)) {
			  g_warning ("Error loading \"%s\". Reason: %s\n", path, err->message);
			  g_error_free (err);
		  }

		  g_free (path);
	  }
  }

  FOREACH_REL(rv_get_rel_manager(rv), cur, iter, { rel_set_hidden(cur, TRUE); });
  FOREACH_FUN(rv_get_fun_manager(rv), cur, iter, { fun_set_hidden(cur, TRUE); });
  FOREACH_PROG(rv_get_prog_manager(rv), cur, iter, { prog_set_hidden(cur, TRUE); });
  FOREACH_DOM(rv_get_dom_manager(rv), cur, iter, { dom_set_hidden(cur, TRUE); });

  /* Unhide the default relation. Unnecessary? */
  rel_set_hidden(rel_manager_get_by_name(rv_get_rel_manager(rv), "$"), FALSE);

  dir_window_update (dir_window_get_instance());

  /* Create the Relview main window. */
  rv_window_get_instance ();

  /* ----------------------------------------------------- GTK Main Loop --- */

  gdk_threads_enter ();
  gtk_main ();
  gdk_threads_leave ();

  /* ------------------------------------------------- Clean-up and Exit --- */

  rv_destroy (rv);

  _save_plugins_to_prefs (rv);

  prefs_save (PREF_FILE);
  prefs_clear ();

  free_resources ();

  return 0;
}
