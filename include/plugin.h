/* Included by the plugin. */

/*
 * plugin.h
 *
 *  Created on: Jul 30, 2009
 *      Author: stefan
 */

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <glib.h>
#include <gmodule.h> // G_MODULE_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

#define IN_RV_PLUGIN 1

/* Provide Kure binding. */
#include "Kure.h"

/* Basic Relview Data Types, Enumerations and the global Relview object. */
#include "RelviewTypes.h"
#include "Observer.h"
#include "Namespace.h"
#include "Relview.h"

/* Complex Data Types and Managers */
#include "Relation.h"
#include "Graph.h"
#include "Domain.h"
#include "Function.h"
#include "Program.h"

/* Register Menus inside Relview. */
#include "Menu.h"

/* Manage handlers to load/save file formats. */
#include "IOHandler.h"

/* Load/Save files. */
#include "FileLoader.h"

#include "GraphWindow.h" // graph_window_get_graph for menu entries.
#include "RelationWindow.h"

typedef gint rvp_plugin_id_t;

void rvp_relation_window_set_relation (Rel * rel);
void rvp_disable_plugin (rvp_plugin_id_t);


typedef struct _RvPlugInInfo RvPlugInInfo;
struct _RvPlugInInfo {
    /* if init returns FALSE, the module will be ignored. */
    gboolean (*init) (gint*, rvp_plugin_id_t); // may be NULL.
    void (*quit) (void); // may be NULL.
    void (*enable) (const gchar *); // must not be NULL.
    void (*disable) (void); // may be NULL.

    gchar * name; // must not be NULL
    gchar * descr; // may be NULL
};

typedef RvPlugInInfo * (*RvPlugInMainFunc) (void);

#define RV_PLUGIN_MAIN() \
        G_MODULE_EXPORT RvPlugInInfo * rv_plugin_main () \
        { return &PLUG_IN_INFO; }

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_H_ */
