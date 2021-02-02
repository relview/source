/* Input and output routines for Xdd files. Xdd files are the future
 * replacement for the non-binary portable ddd files. Xdd files use
 * XML for storing data, while ddd files store the internal data
 * structures in binary form. Thus, changes in the C data structures
 * break backward-compatibility with previous program versions. Even
 * compiler changes or different optimization options could break
 * compatibilty with ddd files between different program versions.
 */

/* Define to get some debug messages while writing/reading Xdd files. */
//#define _XDD_VERBOSE 1

#define _XDD_DTD_URL "http:///www.informatik.uni-kiel.de/~progsys/relview/xdd.dtd"

#include "XddFile.h"

#include <errno.h>
#include "Relview.h"
#include "version.h"
#include "Relation.h"
#include "Graph.h"
#include "Domain.h"
#include "Function.h"
#include "Program.h" /* prog_root */

#include "Relation.h" /* rv_is_valid_name */
#include "FileLoader.h" // DefaultReplacePolicyHandler

#include <string.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define MAXNAMELENGTH 256 // TODO: Remove me!


struct _XddLoader
{
	Relview * rv;

	IOHandler_ReplaceCallback replace_clbk;
	gpointer replace_clbk_user_data;
};

struct _XddSaver
{
	Relview * rv;
};


typedef enum _XddRetCode {
	ERROR = 0, SUCCESS = 1, IGNORE = 2
} XddRetCode;

/* --------------------------------------------------- Static Prototypes --- */

static xmlNodePtr _xmlFindChild(xmlNodePtr el, const xmlChar * childName);
FILE * _tmpfile2(gchar ** pfname);

/* ----------------------------------------------------------- Xdd Utils --- */

/*!
 * Implements IOHandler::load. See \ref xdd_get_handler.
 */
gboolean _xdd_handler_load (XddHandler * self, const gchar *  filename,
		IOHandler_ReplaceCallback replace, gpointer user_data, GError ** perr)
{
	gboolean ret;
	Relview * rv = rv_get_instance();
	XddLoader * loader = xdd_loader_new (rv);
	DefaultReplacePolicyHandler * policy_handler = default_replace_policy_handler_new (rv);
	default_replace_policy_handler_set_parent_callback (policy_handler, replace, user_data);

	xdd_loader_set_replace_callback (loader,
			default_replace_policy_handler_get_callback(policy_handler),
			policy_handler);
	ret = xdd_loader_read_from_file (loader, filename, perr);
	xdd_loader_destroy(loader);
	default_replace_policy_handler_destroy (policy_handler);
	return ret;
}

/*!
 * Implements IOHandler::save. See \ref xdd_get_handler.
 */
gboolean _xdd_handler_save (XddHandler * self, const gchar * filename, GError ** perr)
{
	Relview * rv = rv_get_instance();
	XddSaver * saver = xdd_saver_new(rv);
	return xdd_saver_write_to_file (saver, filename, perr);
}

XddHandler * xdd_get_handler ()
{
	IOHandler * ret = g_new0 (IOHandler,1);
	static IOHandlerClass * c = NULL;
	if (!c) {
		c = g_new0 (IOHandlerClass,1);

		c->load = _xdd_handler_load;
		c->save = _xdd_handler_save;

		c->name = g_strdup ("Relview workspace");
		c->extensions = g_strdup ("xdd");
		c->description = g_strdup ("Can store most elements in a Relview "
				"workspace, that is, relations, graphs, functions etc.");
	}

	ret->c = c;
	return ret;
}


/* ---------------------------------------------- Xdd Write - Prototypes --- */

static gboolean _xdd_serialize_function(XddSaver * self, FILE*, Fun * fun, GError ** perr);
static gboolean _xdd_serialize_relation(XddSaver * self, FILE*, Rel * rel, GError ** perr);
static gboolean _xdd_serialize_nodes(XddSaver * self, FILE*, XGraph * gr, GError ** perr);
static gboolean _xdd_serialize_edges(XddSaver * self, FILE*, XGraph * gr, GError ** perr);
static gboolean _xdd_serialize_graph(XddSaver * self, FILE*, XGraph * gr, GError ** perr);
static gboolean _xdd_serialize_domain(XddSaver * self, FILE*, Dom * dom, GError ** perr);
static gboolean _xdd_serialize_functions(XddSaver * self, FILE*, GError ** perr);
static gboolean _xdd_serialize_domains(XddSaver * self, FILE*, GError ** perr);
static gboolean _xdd_serialize_relations(XddSaver * self, FILE*, GError ** perr);
static gboolean _xdd_serialize_graphs(XddSaver * self, FILE*, GError ** perr);

/* ----------------------------------------------- Xdd Write - Functions --- */

/* if pstr is non-NULL, the serialized function string is
 * appended to the existing string. */
gboolean _xdd_serialize_function(XddSaver * self, FILE * fp, Fun * fun, GError ** perr) {
	if (!fun)
		return FALSE;
	else {
		xmlChar * s = xmlEncodeEntitiesReentrant (NULL, (xmlChar*) fun_get_term(fun));
		fprintf (fp, "<function>%s</function>\n", s);
		xmlFree(s);
		return TRUE;
	}
}

/* width and height may be big numbers. */
gboolean _xdd_serialize_relation(XddSaver * self, FILE * fp, Rel * rel, GError ** perr) {
	if (!rel)
		return FALSE;
	else {
		gboolean ret = TRUE;
		mpz_t rows, cols;
		xmlChar * xname = xmlEncodeEntitiesReentrant (NULL, (xmlChar*) rel_get_name(rel));
		KureRel * impl = rel_get_impl(rel);

		mpz_init (rows); mpz_init (cols);
		kure_rel_get_cols(impl, cols);
		kure_rel_get_rows(impl, rows);

		gmp_fprintf (fp, "<relation name=\"%s\" width=\"%Zd\""
				" height=\"%Zd\">\n<bdd><![CDATA[", xname, cols, rows);

		mpz_clear (rows); mpz_clear (cols);
		xmlFree (xname);

		/* The Dddmp part must be base64 encoded, because libxml can't handle
		 * \0 characters in the input. This is somewhat tedious in our case,
		 * because Dddmp can't write to a string buffer. */
		{
			gchar * tmpfile = NULL;
			FILE * auxfp = _tmpfile2 (&tmpfile);
			if ( !auxfp) {
				fprintf (stderr, "Xdd: Unable to create a temporary file to "
						"write relation \"%s\". Reason: %s. Skipped.\n",
						(gchar*) xname, strerror(errno));
				return IGNORE;
			}
			else {
				/* See the GLib manual for the formula. */
				guchar buf[BUFSIZ];
				gchar enc[(BUFSIZ / 3 + 1) * 4 + 4
				          + ((BUFSIZ / 3 + 1) * 4 + 4) / 72 + 1];
				size_t n, m;
				gint state = 0, save = 0;

				kure_rel_write_to_dddmp_stream (impl, auxfp);
				fseek (auxfp, 0, SEEK_SET);

				while ( !feof(auxfp)) {
					n = fread (buf, 1, BUFSIZ, auxfp);
					if (n > 0) {
						m = g_base64_encode_step(buf, n, TRUE, enc, &state, &save);
						if (m > 0) fwrite (enc, 1, m, fp);
					}
				}

				m = g_base64_encode_close(TRUE, enc, &state, &save);
				if (m > 0) fwrite (enc, 1, m, fp);

				fclose (auxfp);
				unlink (tmpfile);
				g_free (tmpfile);
			}
		}

		fprintf (fp, "]]></bdd></relation>\n");
		return ret;
	}
}

gboolean _xdd_serialize_nodes(XddSaver * self, FILE * fp, XGraph * gr, GError ** perr) {
	if (!gr)
		return FALSE;
	else {
		XGRAPH_FOREACH_NODE(gr,cur,iter,{
			XGraphNodeLayout * layout = xgraph_node_get_layout (cur);
			LayoutPoint pt = xgraph_node_layout_get_pos (layout);

			/* NOTE: We don't store the internal IDs. See the serialization code
			 *       for the edges to know why. */
			fprintf(fp,	"<node name=\"%s\" x=\"%d\" y=\"%d\""
						" dim=\"%d\"/>\n", xgraph_node_get_name (cur),
					(gint)pt.x, (gint)pt.y, xgraph_node_layout_get_radius(layout));
		});

		return TRUE;
	}
}

gboolean _xdd_serialize_edges(XddSaver * self, FILE * fp, XGraph * gr, GError ** perr) {
	if (!gr)
		return FALSE;
	else {
		XGRAPH_FOREACH_EDGE(gr,cur,iter,{
			XGraphNode * from = xgraph_edge_get_from_node(cur);
			XGraphNode * to   = xgraph_edge_get_to_node(cur);
			XGraphEdgeLayout * layout = xgraph_edge_get_layout(cur);

			g_assert (from && to);
			fprintf(fp, "<edge from=\"%s\" to=\"%s\">\n",
					xgraph_node_get_name(from), xgraph_node_get_name(to));
			if (! xgraph_edge_layout_is_simple(layout)
					&& from != to)
			{
				XGraphEdgePath * path = xgraph_edge_layout_get_path(layout);
				GQueue/*<LayoutPoint*>*/ * pts = xgraph_edge_path_get_points (path);
				GList * iter = pts->head;

				fprintf(fp, "<path>\n");
				for ( ; iter ; iter = iter->next) {
					LayoutPoint * pt = (LayoutPoint*) iter->data;
					fprintf(fp, "<point>%d, %d</point>", (gint)pt->x, (gint)pt->y);
				}
				fprintf(fp, "</path>\n");
			}
			fprintf(fp, "</edge>\n");
		});

		return TRUE;
	}
}

gboolean _xdd_serialize_graph(XddSaver * self, FILE * fp, XGraph * gr, GError ** perr) {

	if (!gr)
		return FALSE;
	else {
		gboolean ret = TRUE;
		xmlChar * xname = xmlEncodeSpecialChars(NULL, (xmlChar*)xgraph_get_name (gr));

		fprintf(fp, "<graph name=\"%s\"", xname);
		if (xgraph_is_correspondence(gr)) fprintf (fp, " correspondence=\"yes\"");
		if (xgraph_is_hidden (gr)) fprintf (fp, " hidden=\"yes\"");
		fprintf (fp, ">\n");

		/* Serialize the nodes and the edges. Could be slow! */
		ret = _xdd_serialize_nodes(self, fp, gr, perr);
		if (ret) {
			ret = _xdd_serialize_edges(self, fp, gr, perr);
			fprintf (fp, "</graph>\n");
		}

		return ret;
	}
}

gboolean _xdd_serialize_domain(XddSaver * self, FILE * fp, Dom * dom, GError ** perr) {
	if (!dom)
		return FALSE;
	else {
		xmlChar * xname = xmlEncodeSpecialChars(NULL, (xmlChar*)dom_get_name(dom));
		DomainType type = dom_get_type (dom);

		/* We don't need to save 'state' because the only domains we
		 * export are the ones with state equal to 'normal'.  */
		fprintf(fp, "<domain name=\"%s\" "
				"type=\"%s\">\n", xname, (type == DIRECT_PRODUCT) ? "DIRECT_PRODUCT"
						: (type == DIRECT_SUM) ? "DIRECT_SUM" : "UNKNOWN");
		xmlFree (xname);

		/* Currently only domains with exactly two components are
		 * supported. */
		{
			xmlChar * xcomp1 = xmlEncodeSpecialChars(NULL, (xmlChar*)dom_get_first_comp(dom));
			xmlChar * xcomp2 = xmlEncodeSpecialChars(NULL, (xmlChar*)dom_get_second_comp(dom));
			fprintf(fp,	"<comp>%s</comp><comp>%s</comp></domain>\n", xcomp1, xcomp2);
			xmlFree(xcomp1);
			xmlFree(xcomp2);
		}

		return TRUE;
	}
}

/* Don't store the default relation/graph with name "$". */
static gboolean _rel_filter(Rel * rel) {
	return !g_str_equal(rel_get_name(rel), "$") && !rel_is_hidden(rel);
}
static gboolean _graph_filter(XGraph * gr) {
	return !g_str_equal(xgraph_get_name(gr), "$") && !xgraph_is_hidden(gr);
}
static gboolean _fun_filter(Fun * obj) { return !fun_is_hidden(obj); }

static gboolean _xdd_serialize_relations (XddSaver * self, FILE * fp, GError ** perr)
{
	gboolean ret = TRUE;

	FOREACH_REL(rv_get_rel_manager(rv_get_instance()), cur, iter, {
		if (_rel_filter (cur)) {
			ret = _xdd_serialize_relation (self, fp, cur, perr);
			if ( !ret) break;
		}
	});

	return ret;
}

static gboolean _xdd_serialize_functions (XddSaver * self, FILE * fp, GError ** perr)
{
	gboolean ret = TRUE;

	FOREACH_FUN(fun_manager_get_instance(), cur, iter, {
		if (_fun_filter(cur)) {
			ret = _xdd_serialize_function (self, fp, cur, perr);
			if ( !ret) break;
		}
	});

	return ret;
}

static gboolean _xdd_serialize_domains (XddSaver * self, FILE * fp, GError ** perr)
{
	gboolean ret = TRUE;

	FOREACH_DOM(rv_get_dom_manager(self->rv), cur, iter, {
		ret = _xdd_serialize_domain (self, fp, cur, perr);
		if ( !ret) break;
	});

	return ret;
}

static gboolean _xdd_serialize_graphs (XddSaver * self, FILE * fp, GError ** perr)
{
	gboolean ret = TRUE;

	XGRAPH_MANAGER_FOREACH_GRAPH(xgraph_manager_get_instance(), gr, iter, {
		if (_graph_filter (gr)) {
			ret = _xdd_serialize_graph (self, fp, gr, perr);
			if (! ret) break;
		}
	});

	return ret;
}


gboolean xdd_serialize_all (XddSaver * self, FILE * fp, GError ** perr)
{
	gboolean success = TRUE;

	fprintf (fp, "<!DOCTYPE relview SYSTEM \""_XDD_DTD_URL"\" >\n");
	fprintf (fp, "<relview version=\"%d.%d.%d\">\n",
			RELVIEW_MAJOR_VERSION, RELVIEW_MINOR_VERSION, RELVIEW_MICRO_VERSION);

	success = (success && _xdd_serialize_functions(self, fp, perr));
	success = (success && _xdd_serialize_domains(self, fp, perr));
	success = (success && _xdd_serialize_relations(self, fp, perr));
	success = (success && _xdd_serialize_graphs(self, fp, perr));

	if ( !success)
		g_set_error (perr, rv_error_domain(), 0,
				"xdd_serialize_all: Unable to serialize RelView's state.");
	else
		fprintf (fp, "</relview>\n");

	return success;
}


XddSaver * xdd_saver_new (Relview * rv)
{
	XddSaver * self = g_new0 (XddSaver,1);
	self->rv = rv;
	return self;
}

void xdd_saver_destroy (XddSaver * self)
{
	g_free (self);
}

gboolean xdd_saver_write_to_stream (XddSaver * self, FILE * fp, GError ** perr)
{
	return xdd_serialize_all(self, fp, perr);
}

gboolean xdd_saver_write_to_file (XddSaver * self, const gchar * filename, GError ** perr)
{
	FILE * fp = fopen (filename, "wb");
	if ( !fp) {
		g_set_error (perr, rv_error_domain(), 0, "Unable to create file "
				"\"%s\". Reason: %s", filename, g_strerror(errno));
		return FALSE;
	}
	else {
		xdd_saver_write_to_stream(self, fp, perr);
		fclose (fp);
		return TRUE;
	}
}

/* --------------------------------------------- XddContent - Prototypes --- */

typedef struct _XddContent XddContent;

static XddContent * _xdd_content_new();
static void _xdd_content_destroy(XddContent * self);
static void _xdd_content_create_relations(XddContent * self, XddLoader * loader);
static void _xdd_content_merge_relations(XddContent * self, XddLoader * loader);
static void _xdd_content_merge_functions(XddContent * self, XddLoader * loader);
static void _xdd_content_merge_domains(XddContent * self, XddLoader * loader);
static void _xdd_content_merge(XddContent * self, XddLoader * loader);
static void _xdd_content_dump(XddContent * self);

/* ------------------------------- XddContent - Functions and Structures --- */

/* Represents all stuff read from a Xdd file (e.g. relations and graphs). */
struct _XddContent {
	RelManager * rel_root;
	XGraphManager * gm;
	FunManager * fun_root;
	DomManager * dom_root;
};

/*!
 * Creates a compartment to store loaded objects temporarily. For relations,
 * the global \ref KureContext (rv_get_context(rv_get_instance())) is used.
 */
XddContent * _xdd_content_new() {
	XddContent * self = g_new0 (XddContent, 1);
	self->rel_root = rel_manager_new (rv_get_context(rv_get_instance()));
	self->gm = xgraph_manager_new ();
	self->fun_root = fun_manager_new ();
	self->dom_root = dom_manager_new ();
	return self;
}

void _xdd_content_destroy(XddContent * self) {
	/* The lists are all needed. */
	rel_manager_destroy (self->rel_root);
	xgraph_manager_destroy (self->gm);
	fun_manager_destroy (self->fun_root);
	dom_manager_destroy (self->dom_root);
	g_free(self);
}

/* For any graph without a relation and no relation inside the global
 * state, create a corresponding relation. */
void _xdd_content_create_relations (XddContent * self, XddLoader * loader)
{
	GList/*<XGraph*>*/ * useless = NULL;
	RelManager * rm = rv_get_rel_manager(rv_get_instance());

	XGRAPH_MANAGER_FOREACH_GRAPH(self->gm, cur, iter, {
		const gchar * name = xgraph_get_name (cur);

		if ( !rel_manager_exists(self->rel_root, name)
				&& !rel_manager_exists(rm, name)) {
			Rel * rel = rel_new_from_xgraph(rel_manager_get_context(rm), cur);
			if ( !rel) {
				fprintf(stderr, "Xdd: Unable to create relation for graph "
					"\"%s\".\n", name);

				/* The graph cannot be merged into the global state, so
				 * it is useless now. However, we cannot destroy it,
				 * because this could invalidate the iterator. */
				useless = g_list_prepend (useless, cur);
			}
			else {
#ifdef _XDD_VERBOSE
				printf("Xdd: Graph \"%s\" does not have an associated "
							"relation. Creating one.\n", name);
#endif
				rel_manager_insert (self->rel_root, rel);
			}
		}
	});

	{
		GList * iter = useless;
		for ( ; iter ; iter = iter->next) xgraph_destroy ((XGraph*)iter->data);
		g_list_free(useless);
	}
}

/* Merge relations and graphs into the global state. If we encounter an
 * graph without an relation, create a relation for it. */
/* precond: No two object with the same name in self->* */
void _xdd_content_merge_relations(XddContent * self, XddLoader * loader)
{
	Relview * rv = rv_get_instance();
	XGraphManager * gm = xgraph_manager_get_instance ();
	RelManager * rm = rv_get_rel_manager(rv);

	_xdd_content_create_relations(self, loader);

	/* Ask, what to do with relations/graphs that already exist. */
	FOREACH_REL(self->rel_root, cur, iter, {
		const gchar * name = rel_get_name(cur);

		if (rel_manager_exists(rm, name)) {
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.name = name;
			info.type = RV_OBJECT_TYPE_RELATION;

			action = loader->replace_clbk (loader->replace_clbk_user_data, &info);
			g_assert (action != RV_REPLACE_POLICY_ASK_USER);

			switch (action) {
				case RV_REPLACE_ACTION_REPLACE:
					rel_manager_delete_by_name(rm, name);

					/* Remove an existing graph from the global manager if
					 * necessary. */
					xgraph_manager_delete_by_name (gm, name);
					break;
				case RV_REPLACE_ACTION_KEEP: break;
				default: g_warning ("_xdd_content_merge_relations: "
						"Unknown action: %d\n", action);
			}
		}
	});

	XGRAPH_MANAGER_FOREACH_GRAPH(self->gm, cur, iter, {
		const gchar * name = xgraph_get_name (cur);

		/* Only ask for those graph without a relation in the Xdd state */
		if (xgraph_manager_exists(gm, name)
				&& !rel_manager_exists(self->rel_root, name))
		{
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.name = name;
			info.type = RV_OBJECT_TYPE_GRAPH;

			action = loader->replace_clbk (loader->replace_clbk_user_data, &info);
			g_assert (action != RV_REPLACE_POLICY_ASK_USER);

			switch (action) {
				case RV_REPLACE_ACTION_REPLACE:
					xgraph_manager_delete_by_name(gm, name); break;
				case RV_REPLACE_ACTION_KEEP: break;
				default: g_warning ("_xdd_content_merge_relations: "
						"Unknown action: %d\n", action);
			}

		}
	});

	rel_manager_steal_all (rm, self->rel_root);
	xgraph_manager_steal_all_from (gm, self->gm);
}

/* Merge functions into the global state. */
void _xdd_content_merge_functions(XddContent * self, XddLoader * loader)
{
	FunManager * manager = fun_manager_get_instance ();

	FOREACH_FUN(self->fun_root, cur, iter, {
		const gchar * name = fun_get_name(cur);

		if (fun_manager_exists(manager, name)) {
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.type = RV_OBJECT_TYPE_FUNCTION;
			info.name = name;

			action = loader->replace_clbk (loader->replace_clbk_user_data, &info);
			g_assert (action != RV_REPLACE_ACTION_ASK_USER);

			switch (action) {
			case RV_REPLACE_ACTION_REPLACE:
				fun_manager_delete_by_name (manager, name);
				break;
			case RV_REPLACE_ACTION_KEEP: break;
			default: ;
			}
		} /* exist? */
	});

	fun_manager_steal_all(manager, self->fun_root);
}

/* Merge domains into the global state. */
void _xdd_content_merge_domains(XddContent * self, XddLoader * loader)
{
	DomManager * manager = rv_get_dom_manager(loader->rv);

	FOREACH_DOM(self->dom_root, cur, iter, {
		const gchar * name = dom_get_name(cur);

		if (dom_manager_exists(manager, name)) {
			RvReplaceAction action;
			IOHandlerReplaceInfo info = {0};
			info.type = RV_OBJECT_TYPE_DOMAIN;
			info.name = name;

			action = loader->replace_clbk (loader->replace_clbk_user_data, &info);
			g_assert (action != RV_REPLACE_ACTION_ASK_USER);

			switch (action) {
			case RV_REPLACE_ACTION_REPLACE:
				dom_manager_delete_by_name (manager, name);
				//prog_root = del_prog(prog_root, name);
				break;
			case RV_REPLACE_ACTION_KEEP: break;
			default: ;
			}
		} /* exist? */
	});

	dom_manager_steal_all(manager, self->dom_root);
}

/*!
 * Merge the state (e.g. Relation, Graphs) from a Xdd file into the current
 * RelView state. If some objectes already exist, ask the user what to do.
 *
 * \author stb
 * \date 07.11.2008
 *
 * \param self The state to merge into the current RelView state.
 */
void _xdd_content_merge(XddContent * self, XddLoader * loader) {
	_xdd_content_merge_relations(self, loader); /* and graphs */
	_xdd_content_merge_functions(self, loader);
	_xdd_content_merge_domains(self, loader);
}

void _xdd_content_dump(XddContent * self) {
#define COUNT(t,l,c) { t i; for (i = l, c = 0 ; i ; i = i->next) { c ++; } }
	int relCount, graphCount, funCount, domCount;

	//COUNT(rellistptr, self->rel_root, relCount);
	relCount = rel_manager_size(self->rel_root);
	graphCount = xgraph_manager_get_graph_count (self->gm);
	funCount = fun_manager_size(self->fun_root);
	domCount = dom_manager_size(self->dom_root);

	printf("_XddState:___\n"
		"   %d rels, %d graph, %d funs, %d doms\n"
		"\n", relCount, graphCount, funCount, domCount);
}

/* ------------------------------------------------ Xdd Read - Prototypes -- */

typedef struct _XddGraphNode XddGraphNode;
typedef struct _XddPoint XddPoint;
typedef struct _XddGraphEdgePath XddGraphEdgePath;
typedef struct _XddGraphEdge XddGraphEdge;

static gboolean 	_xdd_check_version(const gchar * version);
static XddRetCode 	_xdd_read_relation(xmlNodePtr el, XddContent * content, GError ** perr);
static GSList/*<XddGraphNode>*/* _xdd_read_nodes(xmlNodePtr el,
		GHashTable * /*inout*/h, GError ** perr);
static GSList/*<XddGraphEdge*>*/* _xdd_read_edges(xmlNodePtr elem,
		GHashTable * h, GError ** perr);
static XddRetCode 	_xdd_read_graph(xmlNodePtr elem, XddContent * content, GError ** perr);
static XddRetCode 	_xdd_read_function(xmlNodePtr el, XddContent * content, GError ** perr);
static GSList/*<xmlChar*>*/* _xdd_read_domain_comps(xmlNodePtr el);
static XddRetCode 	_xdd_read_domain(xmlNodePtr el, XddContent * content, GError ** perr);
static XddRetCode 	_xdd_read_relview(xmlNodePtr el, XddContent * content, GError ** perr);
static gboolean 	_xdd_load(XddLoader * self, xmlDocPtr doc, GError ** perr);

/* ------------------------------------------------ Xdd Read - Structures -- */

struct _XddGraphNode {
	int x, y, dim;
	int state;
	xmlChar * name;

	unsigned int id; /* auto-increment, beginning with 1 */
};

struct _XddPoint {
	float x, y;
};

struct _XddGraphEdgePath {
	GSList/*<XddPoint>*/* points;
};

struct _XddGraphEdge {
	xmlChar *name;
	xmlChar *from, *to; /* node names (not IDs!) */
	int fromId, toId;
	int state;
	XddGraphEdgePath path;
	XGraphEdgeLayout * layout;
};

/* ----------------------------------------- Xdd Read - Private Functions -- */

/*!
 * Validates that the file version can be read by the current relview system.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param version Version string of the form <major>.<minor>.<micro>.
 * \return TRUE if the relview version if compatible, FALSE otherwise.
 */
gboolean _xdd_check_version(const gchar * version) {
	/* The version should be used in later versions to be
	 * backward-compatible. */
	return TRUE;
}

/*!
 * Reads a <RELATION> elements from a XDD file.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <RELVIEW> element.
 * \param h HashTable to store the name<->id relationship for the nodes in.
 * \return TRUE on success, FALSE on failure.
 */
XddRetCode _xdd_read_relation(xmlNodePtr el, XddContent * content, GError ** perr)
{
	xmlChar * name = xmlGetProp(el, (xmlChar*)"name");
	int name_len = xmlStrlen(name);
	XddRetCode ret = SUCCESS;
	xmlNodePtr bddEl = _xmlFindChild(el, (xmlChar*)"bdd");
	RelManager * manager = content->rel_root;

	if (xmlStrEqual(name, (xmlChar*)"$")) {
		verbose_printf(VERBOSE_DEBUG, "Xdd: Found graph/relation with name "
				"\"$\". Renamed to \"Dollar\".\n");
		xmlFree (name);
		name = xmlCharStrdup((char*)"Dollar");
	}

	if (name_len < 1 || name_len > MAXNAMELENGTH) {
		fprintf(stderr, "Xdd: Relation name \"%s\" is too long/short.\n", (char*)name);
		ret = IGNORE;
	} else if (rel_manager_exists(manager, (char*)name)) {
		fprintf(stderr, "Xdd: Multiple relations with name \"%s\" found.\n", name);
		ret = IGNORE;
	} else if (NULL == bddEl) {
		fprintf(stderr, "Xdd: No BDD found for relation \"%s\".\n", (char*)name);
		ret = IGNORE;
	} else {
		xmlChar *swidth, *sheight;
		mpz_t rows, cols;

		swidth = xmlGetProp(el, (xmlChar*)"width");
		sheight = xmlGetProp(el, (xmlChar*)"height");

		mpz_init(rows);
		mpz_init(cols);
		mpz_set_str(rows, (char*)sheight, 10);
		mpz_set_str(cols, (char*)swidth, 10);

#ifdef _XDD_VERBOSE
		gmp_fprintf(stderr,
				"Xdd: found relation with name '%s' (%Zd x %Zd = rows x cols)\n",
				name, rows, cols);
#endif

		xmlFree(swidth);
		xmlFree(sheight);

		/* Now decode the String (base64), write it to a temporary file and
		 * read this file using \ref kure_rel_read_from_dddmp_stream. */
		{
			gsize rawlen = 0;
			gchar * raw = (gchar*) g_base64_decode((char*)bddEl->children->content, &rawlen);
			char * tmpname = NULL;
			FILE * fp = _tmpfile2(&tmpname);

			if ( !fp) {
				fprintf (stderr, "Xdd: Unable to open a temporary file "
						"using tmpfile(). Skipped. Reason: %s\n", strerror(errno));
				ret = IGNORE;
			}
			else {
				KureContext * context = rel_manager_get_context (content->rel_root);
				KureRel * impl = kure_rel_new (context);
				Kure_success success;

				fwrite (raw, rawlen, 1, fp);
				fseek (fp, 0, SEEK_SET);

				success = kure_rel_read_from_dddmp_stream (impl, fp, rows, cols);
				if ( !success) {
					fprintf (stderr, "Xdd: Unable to read the BDD of relation "
							"\"%s\". Skipped.\n", (char*)name);
					ret = IGNORE;

					kure_rel_destroy (impl);
				}
				else {
					Rel * rel = rel_new_from_impl((char*) name, impl);
					rel_manager_insert(content->rel_root, rel);
				}

				fclose (fp);
				unlink(tmpname);
				g_free (tmpname);
			}

			g_free (raw);
		}

		mpz_clear(rows);
		mpz_clear(cols);

	}

	return ret;
}

/*!
 * Reads all <NODE> elements for a given <GRAPH> element in a XDD file. If no
 * error occurs, the resulting list contains at least one node, due to the DTD
 * constraints.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <RELVIEW> element.
 * \param h HashTable to store the name<->id relationship for the nodes in.
 * \return A list of nodes.
 */
GSList/*<XddGraphNode>*/* _xdd_read_nodes(xmlNodePtr el,
		GHashTable/*<name,id>*/ * /*inout*/h, GError ** perr)
{
	unsigned int nextNodeId = 1;

	GSList * nl = NULL;
	xmlNodePtr cur = el->children;
	for (; cur; cur = cur->next) {
		if (XML_ELEMENT_NODE == cur->type && 0 == xmlStrcasecmp(cur->name, (xmlChar*)"NODE")) {
			xmlChar *name = xmlGetProp(cur, (xmlChar*)"name");

			/* If another node with the node's name exists, ignore it. */
			if (NULL != g_hash_table_lookup(h, name)) {
				g_warning("Xdd: Node with name \"%s\" already exists. Skipped.\n", name);
				xmlFree(name);
			} else {
				xmlChar *sx = xmlGetProp(cur, (xmlChar*)"x"), *sy = xmlGetProp(cur, (xmlChar*)"y"),
						*sdim = xmlGetProp(cur, (xmlChar*)"dim"), *sstate = xmlGetProp(
								cur, (xmlChar*)"state");
				XddGraphNode * node = g_new0(XddGraphNode, 1);

				node->name = name;
				node->x = MAX(0, atoi((char*)sx));
				node->y = MAX(0, atoi((char*)sy));
				node->dim = atoi((char*)sdim);

				xmlFree(sx);
				xmlFree(sy);
				xmlFree(sdim);
				xmlFree(sstate);

				node->id = nextNodeId++;

#ifdef _XDD_VERBOSE
				printf ("Xdd: Created node with name \"%s\" (id: %d) at (%d,%d), dim=%d\n",
						node->name, node->id, node->x, node->y, node->dim);
#endif

				g_hash_table_insert(h, name, GINT_TO_POINTER(node->id));

				/* Reverse Order! */
				nl = g_slist_prepend(nl, node);
			}
		}
	}

	/* We do not reverse the list, because we use a prepend function later on
	 * to create the nodes in the real graph object. Thus, the nodes will be
	 * reversed twice and everything is in the right place. */

	return nl;
}

/*!
 * Reads a edge path (<PATH> element) for a given <EDGE> element in an XDD
 * file. Because paths are optional, there may be no path, in which case
 * NOPATH will be returned.
 *
 * The path will be stored in edge->path. It's up to the caller to create
 * an layout object for the edge!
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <EDGE> element.
 * \param edge The owning edge.
 * \return SUCESS, NOPATH or ERROR.
 */
/* returns 0 on success, -1 on error an 1 is no path exists.  */
XddRetCode _xdd_read_edge_path(xmlNodePtr el, XddGraphEdge * edge, GError ** perr) {
	XddGraphEdgePath * path = &edge->path;
	xmlNodePtr elPath = _xmlFindChild(el, (xmlChar*)"PATH");
	if (!elPath)
		return IGNORE;
	else {
		xmlNodePtr cur = elPath->children;
		path->points = NULL;

#ifdef _XDD_VERBOSE
		printf( "Xdd: Found an edge with a path.\n");
#endif

		for (; cur; cur = cur->next) {
			if (XML_ELEMENT_NODE == cur->type && 0 == xmlStrcasecmp(cur->name,
					(xmlChar*)"POINT")) {

				/* seek to the text-node */
				xmlNodePtr cur2 = cur->children;
				for (; cur2 && cur2->type != XML_TEXT_NODE; cur2 = cur2->next)
					;
				if (!cur2) {
					g_warning("Xdd: Point without coordinates found. Ignored!\n");
				} else {
					XddPoint * pt = g_new (XddPoint,1);

					int ret2 = sscanf((char*)cur2->content, "%f, %f", &pt->x, &pt->y);
					if (2 != ret2) {
						g_warning("Xdd: Coordinates \"%s\" for a point in an edge path "
									"are invalid!\n", cur2->content);
						g_free(pt);
					} else /* all fine */{
#ifdef _XDD_VERBOSE
						printf("Xdd: Found point %f, %f\n", pt->x, pt->y);
#endif

						/* Reverse order! */
						path->points = g_slist_prepend(path->points, pt);
					}
				}
			}
		}
	}

	path->points = g_slist_reverse(path->points);
	return SUCCESS;
}

/*!
 * Reads all <EDGE> elements in a XDD file belonging to a single <GRAPH>
 * element. Edges are returned as a list. In the XddGraphEdge object the
 * 'path' element is for internal use only. Layout information are (if avail.)
 * stored in the GraphEdgeLayout object. Incident nodes are resolved and
 * checked for existence while reading the edges from the XDD file. Invalid
 * edges (eg. nodes don't exist) are ignored and are not in the result list.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <GRAPH> element.
 * \param h A Hashtable which associates node's names with node's IDs.
 * \return TRUE on success, FALSE on failure.
 */
GSList/*<XddGraphEdge*>*/* _xdd_read_edges(xmlNodePtr elem, GHashTable * h, GError ** perr) {
	GSList * l = NULL;
	xmlNodePtr cur = elem->children;

	if (!h) {
		g_warning("Xdd: Hash table for node name<->id resolving not absent!\n");
		return NULL;
	}

	for (; cur; cur = cur->next) {
		if (XML_ELEMENT_NODE == cur->type && 0 == xmlStrcasecmp(cur->name,
				(xmlChar*)"EDGE")) {
			xmlChar *from = xmlGetProp(cur, (xmlChar*)"from"), *to =
					xmlGetProp(cur, (xmlChar*)"to");
			int fromId = GPOINTER_TO_INT(g_hash_table_lookup(h, from));
			int toId = GPOINTER_TO_INT(g_hash_table_lookup(h, to));

			if (0 == fromId || 0 == toId) {
				g_warning("Xdd: Edge from \"%s\" to \"%s\" cannot be created. "
					"At least one node is missing. Skipped.\n",	from, to);
				xmlFree(from);
				xmlFree(to);
			} else {
				xmlChar *name = xmlGetProp(cur, (xmlChar*)"name"), *sstate = xmlGetProp(
						cur, (xmlChar*)"state");

				XddGraphEdge * edge = g_new0(XddGraphEdge, 1);
				int ret2;

				edge->name = name;
				edge->from = from;
				edge->to = to;
				edge->fromId = fromId;
				edge->toId = toId;
				//edge->state = _edge_state_from_string((gchar*) sstate); /* handles NULL */

#ifdef _XDD_VERBOSE
			printf("Xdd: Found edge from \"%s\" (id %d) to \"%s\" (id %d).\n",
					from, fromId, to, toId);
#endif

				xmlFree(sstate);

#ifdef _XDD_VERBOSE
				printf("Xdd: Does the edge have a path? ... ");
#endif
				/* Read the Path, if there is one. If we get errors or there is no
				 * path, don't use a/the path. */
				ret2 = _xdd_read_edge_path(cur, edge, perr);
				if (SUCCESS == ret2) {
#ifdef _XDD_VERBOSE
					printf("yes!\n");
#endif
					/* Create an edge layout object for the path. */
					edge->layout = xgraph_indep_edge_layout_new ();
					{
						XGraphEdgePath * path = xgraph_edge_path_new ();
						GQueue * pts = xgraph_edge_path_get_points(path);
						GSList/*<XddPoint*>*/ * iter;

						for ( iter = edge->path.points ; iter ; iter = iter->next) {
							XddPoint * pt = (XddPoint*) iter->data;
							g_queue_push_tail (pts, layout_point_new (pt->x, pt->y));
						}

						xgraph_edge_layout_set_path (edge->layout, path);
						xgraph_edge_path_destroy (path);
					}

					/* The path itself is no longer needed */
					g_slist_foreach(edge->path.points, (GFunc) g_free, NULL);
					g_slist_free(edge->path.points);
					edge->path.points = NULL;
				} else {
#ifdef _XDD_VERBOSE
					printf("no.\n");
#endif
					edge->layout = NULL;
				}

				l = g_slist_prepend(l, edge);
			}
		}
	}

	return l;
}

/*!
 * Reads the initial <GRAPH> element in a XDD file and stores it in the XDD
 * content object.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <GRAPH> element.
 * \param content The XDD content object to store the objects in the xdd in.
 * \return TRUE on success, FALSE on failure.
 */
XddRetCode _xdd_read_graph(xmlNodePtr elem, XddContent * content, GError ** perr)
{
	xmlChar * name = xmlGetProp(elem, (xmlChar*)"name");
	int name_len = xmlStrlen(name);
	//xmlChar * shidden = xmlGetProp(elem, (xmlChar*)"hidden");
	//xmlChar * scorr = xmlGetProp(elem, (xmlChar*)"correspondence");
	XddRetCode ret = SUCCESS;

#ifdef _XDD_VERBOSE
	printf("Xdd: Found graph with name \"%s\".\n",	(char*)name);
#endif

	if (name_len < 1 || name_len > MAXNAMELENGTH) {
		g_warning("Xdd: Graph name \"%s\" too long, or too short (<1).", (char*)name);
		ret = IGNORE;
	} else if (!namespace_is_valid_name(rv_get_namespace(rv_get_instance()), (gchar*)name)) {
		g_warning("Xdd: Graph name \"%s\" invalid. Ignored.\n", (char*)name);
		ret = IGNORE;
	} else if (xgraph_manager_exists(content->gm, (char*)name)) {
		g_warning("Xdd: More than one graph of name \"%s\" found. Ignoring.\n", (char*)name);
		ret = IGNORE;
	} else /* name is valid */
	{
		/* Hash the node names to the nodes; also used to identify
		 * nodes with same name and edges with missing nodes. */
		GHashTable/*<name,id>*/ * h = g_hash_table_new(g_str_hash, g_str_equal);

		GSList/*<XddGraphNode*>*/*nl = _xdd_read_nodes(elem, h /*out*/, perr);
		GSList/*<XddGraphEdge*>*/*el = _xdd_read_edges(elem, h /*in*/, perr);

		/* Create the graph for the XddContent ... nodes first ... */
		XGraph * gr = xgraph_manager_create_graph (content->gm, (char*)name);
		GHashTable/*<id,XGraphNode*>*/ * map
			= g_hash_table_new (g_direct_hash, g_direct_equal);
		GSList * iter = nl;

		g_hash_table_destroy (h);

		xgraph_block_notify (gr);

		for (; iter; iter = iter->next) {
			XddGraphNode * node = (XddGraphNode*) iter->data;
			XGraphNode * cur = xgraph_create_node (gr);
			XGraphNodeLayout * layout = xgraph_node_get_layout(cur);

			xgraph_node_set_name (cur, node->name);

			g_hash_table_insert (map, (gpointer)(long)node->id, cur);

			xgraph_node_layout_set_pos (layout, node->x, node->y);
			xgraph_node_layout_set_radius (layout, node->dim);

			/* Destroy the node on the fly. */
			xmlFree(node->name);
			g_free(node);
		}
		g_slist_free(nl);

		/* ... than the edges.*/
		for (iter = el; iter; iter = iter->next) {
			XddGraphEdge * edge = (XddGraphEdge*) iter->data;
			XGraphNode * from = g_hash_table_lookup (map, GINT_TO_POINTER(edge->fromId));
			XGraphNode * to   = g_hash_table_lookup (map, GINT_TO_POINTER(edge->toId));
			XGraphEdge * cur = xgraph_create_edge(gr, from, to);

			if (edge->layout) {
				xgraph_edge_apply_layout (cur, edge->layout);
				xgraph_edge_layout_destroy (edge->layout);
			}

			/* Destroy the edge on the fly. */
			xmlFree(edge->from);
			xmlFree(edge->to);
			xmlFree(edge->name);
		}
		g_slist_free(el);

		g_hash_table_destroy (map);

		xgraph_unblock_notify (gr);
	}

	xmlFree(name);

	return ret;
}

/*!
 * Reads the initial <FUNCTION> element in a XDD file and stores it in the XDD
 * content object.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <FUNCTION> element.
 * \param content The XDD content object to store the objects in the xdd in.
 * \return TRUE on success, FALSE on failure.
 */
XddRetCode _xdd_read_function(xmlNodePtr el, XddContent * content, GError ** perr) {

	/* find the term (text-node) */
	xmlNodePtr cur = el->children;
	for ( ; cur && cur->type != XML_TEXT_NODE ; cur = cur->next);
	if (! cur) {
		g_warning("Xdd: Function found without a term. Ignored.\n");
		return IGNORE;
	}
	else {
		XddRetCode ret = SUCCESS;
		xmlChar * term = cur->content, *fun_name = NULL;
		const xmlChar * p = xmlStrchr (term, '(');
		if (NULL == p) {
			g_warning("Xdd: Invalid function term found: \"%s\".\n", term);
			return IGNORE;
		}

		/* The '(' does NOT belong the function's name anymore. */
		fun_name = xmlStrsub (term, 0, (int)(p - term));

		/* first function of that name? */
		if (fun_manager_exists(content->fun_root, (gchar*) fun_name)) {
			g_warning("Xdd: Function of name \"%s\" already known. Ignoring.\n",
					(char*) fun_name);
			ret = IGNORE;
		}
		else {
			GError * err = NULL;
			Fun * obj = NULL;
			xmlChar * term = cur->content;

#ifdef _XDD_VERBOSE
			printf("Xdd: Function \"%s\" found: \"%s\".\n", fun_name, term);
#endif

			obj = fun_new_from_def ((char*)term, &err);
			if (obj) {
				fun_manager_insert (content->fun_root, obj);
			} else {
				g_warning ("Xdd: Unable to create function "
						"\"%s\". Skipped. Reason: %s", (char*)fun_name, err->message);
				g_error_free(err);
			}
		}

		xmlFree (fun_name);

		return ret;
	}
}

/*!
 * Reads all <COMP> elements from a XDD file for the specified domain and
 * returns them in a list.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <DOMAIN> element.
 * \return NULL, if there are no components or on failure.
 */
GSList/*<xmlChar*>*/* _xdd_read_domain_comps(xmlNodePtr el)
{
	GSList * cl = NULL;
	xmlNodePtr cur = el->children;

	for (; cur; cur = cur->next) {
		if (XML_ELEMENT_NODE == cur->type && 0 == xmlStrcasecmp(cur->name,
				(xmlChar*)"COMP")) {
			/* A <COMP> element. Look for the text-node */
			xmlNodePtr cur2 = cur->children;
			for (; cur2 && cur2->type != XML_TEXT_NODE; cur2 = cur2->next)
				;
			if (!cur2)
				g_warning("Xdd: Empty <COMP> element found in domain. Ignored!\n");
			else {
#ifdef _XDD_VERBOSE
				printf("Xdd: Found component for domain: \"%s\".\n", cur2->content);
#endif

				/* Reverse order! */
				cl = g_slist_prepend(cl, xmlStrdup(cur2->content));
			}
		}
	}

	/* All comps. were ignored. */
	if (NULL == cl) {
		return NULL;
	} else {
		/* Re-reverse the list. */
		return g_slist_reverse(cl);
	}
}


/*!
 * Reads a <DOMAIN> element from an XDD file an stores it into the XddContent
 * object.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <DOMAIN> element.
 * \param content The XDD content object to store the objects in the xdd in.
 * \return TRUE on success, FALSE on failure.
 */
XddRetCode _xdd_read_domain(xmlNodePtr el, XddContent * content, GError ** perr) {
	XddRetCode ret = SUCCESS;

	/* <!ELEMENT domain ((comp,comp),comp*)>
	 <!ATTLIST domain name CDATA #REQUIRED>
	 <!ATTLIST domain type (DIRECT_SUM|DIRECT_PRODUCT) #REQUIRED>
	 <!ELEMENT comp (#PCDATA)>*/
	/* A domain has two, or more <comp>  elements. */
	xmlChar * name = xmlGetProp(el, (xmlChar*)"name");
	int name_len = xmlStrlen(name);

	if (name_len < 1 || name_len > MAXNAMELENGTH) {
		g_warning("Xdd: Domain name \"%s\" is too long. Max. %d characters "
					"are allowed.\n", name, MAXNAMELENGTH);
		ret = IGNORE;
	} else {
		if (dom_manager_exists(content->dom_root, (char*)name)) {
			g_warning("Xdd: Domain of name \"%s\" already known. Ignoring.\n", name);
			ret = IGNORE;
		} else {
			xmlChar * stype = xmlGetProp(el, (xmlChar*)"type");
			DomainType type;

#ifdef _XDD_VERBOSE
			printf("Xdd: Found domain with name \"%s\" of type %s.\n", name, stype);
#endif

			if (0 == xmlStrcasecmp(stype, (xmlChar*)"DIRECT_SUM")) {
				type = DIRECT_SUM;
			} else if (0 == xmlStrcasecmp(stype, (xmlChar*)"DIRECT_PRODUCT")) {
				type = DIRECT_PRODUCT;
			} else {
				g_warning("Xdd: Unknown domain type \"%s\".\n", stype);
				ret = IGNORE;
			}

			if (TRUE == ret) {
				GSList * cl = _xdd_read_domain_comps(el);
				if (NULL == cl || NULL == g_slist_next (cl)) {
					g_warning("Xdd: No (or only one) (valid) component(s) "
								"for domain \"%s\".\n", name);
					ret = IGNORE;
				} else if (2 < g_slist_length(cl)) {
					g_warning("Xdd: Domain \"%s\" has more than 2 components."
								" Ignored.\n", name);
					ret = IGNORE;
				} else {
					GError * err = NULL;
					Dom * dom = dom_new ((gchar*)name, type, (gchar*)cl->data,
							(gchar*)cl->next->data, &err);

	#ifdef _XDD_VERBOSE
					printf("Xdd: Domain \"%s\" has components \"%s\" and \"%s\"\n",
							(gchar*)name, (gchar*)cl->data, (gchar*)cl->next->data);
	#endif
					if (dom)
						dom_manager_insert (content->dom_root, dom);
					else {
						fprintf (stderr, "Xdd: Unable to create domain \"%s\" "
								"from components \"%s\" and \"%s\". Skipped. "
								"Reason: %s\n", (gchar*)name, (gchar*)cl->data,
								(gchar*)cl->next->data, err->message);
						g_error_free(err);
						ret = IGNORE;
					}
				}

				if (cl != NULL) {
					/* free the components list ... */
					g_slist_foreach(cl, (GFunc) xmlFree, NULL);
					g_slist_free(cl);
				}
			}

			xmlFree(stype);
		} /* name unique? */
	}

	xmlFree(name);

	return ret;
}


/*!
 * Reads the initial <RELVIEW> element in a XDD file. Also validates the files
 * version.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param el The XML node of the <RELVIEW> element.
 * \param content The XDD content object to store the objects in the xdd in.
 * \return TRUE on success, FALSE on failure.
 */
XddRetCode _xdd_read_relview(xmlNodePtr el, XddContent * content, GError ** perr)
{
	xmlChar * versionStr = xmlGetProp(el, (xmlChar*)"version");
	XddRetCode ret = ERROR;

	if (!_xdd_check_version((char*)versionStr)) {
		g_set_error (perr, 0,0, "XddFile Version mismatch. Version associated "
				"with the Xdd file is %s.\n", versionStr);
	} else {
		xmlNodePtr curEl = el->children;
		ret = SUCCESS;

		for (; ret != ERROR && curEl; curEl = curEl->next) {
			if (curEl->type == XML_ELEMENT_NODE) {
				const xmlChar * elName = curEl->name;

				if (0 == xmlStrcasecmp(elName, (xmlChar*)"relation"))
					ret = _xdd_read_relation(curEl, content, perr);
				else if (0 == xmlStrcasecmp(elName, (xmlChar*)"graph"))
					ret = _xdd_read_graph(curEl, content, perr);
				else if (0 == xmlStrcasecmp(elName, (xmlChar*)"function"))
					ret = _xdd_read_function(curEl, content, perr);
				else if (0 == xmlStrcasecmp(elName, (xmlChar*)"domain"))
					ret = _xdd_read_domain(curEl, content, perr);
				else {
					g_warning ("Xdd: Unknown Element \"%s\" in <relview> "
							"element.\n", elName);
					ret = IGNORE;
				}
			}
		} /* for each child */
	} /* version good? */

	xmlFree(versionStr);

	return ret;
}


/*!
 * Loads a XDD file from a given XML document tree, generates by libxml2.
 * Updates also the GUI, but this could and will change in the future.
 *
 * \author stb
 * \date 03.06.2009
 *
 * \param doc The XML document tree.
 * \return Returns TRUE on success, FALSE otherwise.
 */
gboolean _xdd_load (XddLoader * self, xmlDocPtr doc, GError ** perr)
{
	if (NULL == doc)
		return FALSE;
	else {
		XddContent * state = _xdd_content_new();
		xmlNode * root_element;
		XddRetCode ret = SUCCESS;

		root_element = xmlDocGetRootElement(doc);
		ret = _xdd_read_relview(root_element, state, perr);
		if (ret != ERROR) {
			VERBOSE(VERBOSE_DEBUG, _xdd_content_dump(state);)
			_xdd_content_merge(state, self);
		}

		_xdd_content_destroy(state);
		return (ret != ERROR);
	}
}

/* ------------------------------------------ Xdd Read - Public Functions -- */

#include <libxml/xmlIO.h>

static xmlExternalEntityLoader _old_entity_loader = NULL;

static xmlParserInputPtr
_xdd_external_entity_loader(const char * url, const char * id,
        xmlParserCtxtPtr ctxt);

/* Utilities to load the external DTD. */
xmlParserInputPtr
_xdd_external_entity_loader(const char * url, const char * id,
        xmlParserCtxtPtr ctxt)
{
    if (0 == g_strcasecmp(url, _XDD_DTD_URL))
    {
        gchar * path = rv_find_data_file("xdd.dtd");
        if (!path)
        {
            printf("Xdd: Unable to locate DTD \"xdd.dtd\".\n");
            return NULL;
        }
        else
        {
            xmlParserInputPtr stream;

#ifdef _XDD_VERBOSE
            printf("path to xdd.dtd: %s\n", path);
#endif

            stream = xmlNewIOInputStream(ctxt,
                    xmlParserInputBufferCreateFilename(path,
                            XML_CHAR_ENCODING_NONE), XML_CHAR_ENCODING_NONE);
            g_free(path);
            return stream;
        }
    }
    else return _old_entity_loader(url, id, ctxt);
}

XddLoader * xdd_loader_new (Relview * rv)
{
	XddLoader * self = g_new0 (XddLoader, 1);
	self->rv = rv;
	return self;
}

void xdd_loader_destroy (XddLoader * self)
{
	g_free (self);
}

void xdd_loader_set_replace_callback (XddLoader * self,
		IOHandler_ReplaceCallback replace, gpointer user_data)
{
	self->replace_clbk = replace;
	self->replace_clbk_user_data = user_data;
}

gboolean xdd_loader_read_from_file (XddLoader * self, const gchar * filename, GError ** perr)
{
	xmlParserCtxtPtr ctxt; /* the parser context */
	xmlDocPtr doc; /* the resulting document tree */
	int ret = FALSE;

	/* create a parser context */
	ctxt = xmlNewParserCtxt();
	if (ctxt == NULL) {
		g_set_error (perr, 0,0, "Xdd: Failed to allocate parser context.");
		return FALSE;
	}

	/* used to load the DTD. */
	_old_entity_loader = xmlGetExternalEntityLoader();
	xmlSetExternalEntityLoader(_xdd_external_entity_loader);

	/* parse the file, activating the DTD validation option */
	doc = xmlCtxtReadFile(ctxt, filename, NULL, XML_PARSE_DTDVALID);
	if (doc == NULL) {
		g_set_error (perr, 0,0, "Xdd: Unable to load file \"%s\".", filename);
		xmlSetExternalEntityLoader(_old_entity_loader);
		return FALSE;
	} else {
		/* Validation successfull? */
		if (ctxt->valid == 0) {
			g_set_error (perr, 0,0, "Xdd: Failed to validate file \"%s\".", filename);
		} else
			ret = _xdd_load(self, doc, perr);

		xmlFreeDoc(doc);
	}

	xmlFreeParserCtxt(ctxt);
	xmlSetExternalEntityLoader(_old_entity_loader);

	return ret;
}

/* ------------------------------------------------ Xdd Auxiliary Functions --- */

xmlNodePtr _xmlFindChild(xmlNodePtr el, const xmlChar * childName) {
	xmlNodePtr cur = el->children;
	for (; cur; cur = cur->next) {
		if (0 == xmlStrcasecmp(cur->name, childName))
			return cur;
	}
	return NULL;
}


/* *pfname should be NULL. In case of an error, *pfname will be NULL.
 * On success, *pfname must be freed using g_free/ */
FILE * _tmpfile2(gchar ** pfname) {
	GError * err = NULL;
	int fd = g_file_open_tmp(NULL, pfname, &err);
	if (-1 == fd) {
		printf("Unable to open temporary file: %s\n", err->message);
		g_error_free(err);
		return NULL;
	} else {
		FILE * fp = fdopen(fd, "w+b");
		if (!fp) {
			g_free(*pfname);
			*pfname = NULL;
		}
		return fp;
	}
}
