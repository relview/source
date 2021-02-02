/*
 * Misc.h
 *
 *  Created on: Jun 19, 2009
 *      Author: stb
 */

#ifndef MISC_H_

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

gboolean random_atom (Rel * dst, const Rel * src);
gboolean dump_bdd_as_ddcal(Rel * rel);
GdkPixbuf * relation_bdd_to_pixbuf (Rel * rel);

#define MISC_H_

#endif /* MISC_H_ */
