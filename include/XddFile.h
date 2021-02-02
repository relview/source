/*
 * XddFile.h
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

#ifndef XDDFILE_H_
#define XDDFILE_H_

#include <stdio.h>
#include <glib.h>
#include "IOHandler.h"

typedef IOHandler XddHandler;

typedef struct _XddSaver XddSaver;
typedef struct _XddLoader XddLoader;

XddSaver * 		xdd_saver_new (Relview * rv);
void 			xdd_saver_destroy (XddSaver * self);
gboolean 		xdd_saver_write_to_stream (XddSaver * self,
					FILE * stream, GError ** perr);
gboolean 		xdd_saver_write_to_file (XddSaver * self,
					const gchar * filename, GError ** perr);


XddLoader * 	xdd_loader_new (Relview * rv);
void 			xdd_loader_destroy (XddLoader * self);
void 			xdd_loader_set_replace_callback (XddLoader * self,
					IOHandler_ReplaceCallback, gpointer user_data);
gboolean 		xdd_loader_read_from_file (XddLoader * self, const gchar * filename,
					GError ** perr);
gboolean 		xdd_loader_read_from_string (XddLoader * self, const gchar * s,
					gint len, GError ** perr);



/*!
 *  Returns a newly created \ref IOHandler, which can be used to save and
 *  load XDD files in a common way.
 */
XddHandler * 	xdd_get_handler ();


/* ----------------------------------------------------------- Xdd Output --- */

/*!
 * Write the current RelView state to the given stream. The state consists
 * (currently) of all relations and graph (except for $), all non-hidden
 * functions and domains. Any hidden elements won't be written.
 * 
 * \author stb 
 * \date 07.11.2008
 * 
 * \parem fp The stream to write to. The stream don't have to be seekable.
 * \return Returns TRUE on success, FALSE on error.
 */
gboolean 		xdd_write_to_stream (FILE * fp);


/*!
 * A wrapper for \ref xdd_write_to_stream. The difference if, that here
 * a filename is given and the Xdd data are stored in the file. If a name
 * with the given file already exists, it will be overwritten.
 * 
 * \author stb 
 * \date 07.11.2008
 * 
 * \parem file The file to write the Xdd data into.
 * \return Returns TRUE on success, FALSE on error.
 */
gboolean 		xdd_write_to_file (const gchar * file);


/* ------------------------------------------------------------ Xdd Input --- */

/*!
 * Read the given Xdd file into the current state of the System. If some
 * object (e.g Relations) already exist, the user is asked what to do. The
 * suffix of the given file doesn't care. If an error occures during reading
 * the state of the systems won't be changed.
 * 
 * \author stb 
 * \date 07.11.2008
 * 
 * \param file The file to read as an Xdd file.
 * \return Returns TRUE on success, FALSE on error.
 */
gboolean 		xdd_load_from_file (const gchar * file);


/*!
 * Parse the given string as the contents of a Xdd file. If some
 * object (e.g Relations) already exist, the user is asked what to do. The
 * suffix of the given file doesn't care. If an error occures during reading
 * the state of the systems won't be changed.
 * 
 * \author stb 
 * \date 07.11.2008
 * 
 * \param s The string to parse. Have not to be nul-terminated.
 * \param len The exact len of the given string in bytes, without
 *            any trailing nul-terminating byte(s). If len is 0, 
 *            parsing will fail.
 * \return Returns TRUE on success, FALSE on error.
 */
gboolean 		xdd_load_from_string (const gchar * s, gsize len);

#endif /*XDDFILE_H_*/
