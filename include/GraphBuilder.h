#ifndef GRAPHBUILDER_H_
#define GRAPHBUILDER_H_

typedef struct _GraphBuilder GraphBuilder;

typedef gboolean (*GraphBuilder_buildNodeFunc) (gpointer, gint);
typedef gboolean (*GraphBuilder_buildEdgeFunc) (gpointer, gint, gint);
typedef void (*GraphBuilder_destroyFunc) (gpointer);

/*!
 * An edge is built, after both incident nodes were built.
 * */
/*interface*/struct _GraphBuilder {
	// FALSE is returned, e.g. on name clashes. First argument is the owner,
	// NOT the GraphBuilder.
	gboolean (*buildNode)(gpointer owner, gint);

	// FALSE, if (at least) one node doesn't exist. First argument is the
	// owner, NOT the GraphBuilder.
	gboolean (*buildEdge)(gpointer owner, gint, gint);

	void (*destroy)(gpointer);

	/* Doesn't make much sense, due to no unified graph interface. */
	//void* (*getGraph) (GraphBuilder*);

	gpointer owner;
};

#endif /*GRAPHBUILDER_H_*/

