/*
 * FileUtils.c
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

#include "FileUtils.h"
#include <string.h>

gchar * file_utils_get_extension (const gchar * filename)
{
	const gchar * last_dot = strrchr (filename, '.');
	if (! last_dot) return NULL;
	else return g_strdup (last_dot+1);
}


gchar * file_utils_strip_extension (const gchar * filename)
{
	const gchar * last_dot = strrchr (filename, '.');
	if (! last_dot) return g_strdup (filename);
	else {
		gint len = last_dot - filename;
		return g_strndup (filename, len);
	}
}
