/*
 * FileUtils.h
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

#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include <glib.h>

/*!
 * Returns the part between the rightmost dot in filename and the end. If there
 * is no dot in filename, NULL is returned. If there is a dot, but the
 * specified part is empty, "" is returned. The returned string must be freed
 * with g_free after use.
 */
gchar * file_utils_get_extension (const gchar * filename);


/*!
 * If filename contains no dot, a copy of filename is returns. Otherwise
 * the part between the beginning and the rightmost dot (excl.) is
 * returned. Never returns NULL. The returned string must be freed with g_free
 * after use.
 */
gchar * file_utils_strip_extension (const gchar * filename);


#endif /* FILEUTILS_H_ */
