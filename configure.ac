#
# configure.ac
#
#  Copyright (C) 2008,2009,2010,2011,2012 Stefan Bolus, University of Kiel,
#  Germany
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

AC_PREREQ([2.63])
AC_INIT([relview], [8.1], [stb@informatik.uni-kiel.de])

# Used in relview-8.0.pc.in
AC_SUBST([PACKAGE_VERSION])

# "subdir-objects": store object (.o) files in the corresponding subdirectories
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects dist-bzip2])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_YACC
AC_PROG_LEX
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_AWK

# Used in src/Makefile.am
AC_SUBST([AWK])

# Checks for library functions.
AC_FUNC_FORK
# later ... currently there are no replacements for them
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_CHECK_FUNCS([getcwd gettimeofday memset pow regcomp select sqrt strchr strdup strstr])

# glib-2.0 und gtk+-2.0
AM_PATH_GLIB_2_0([2.18.0],,,gthread gmodule )
AM_PATH_GTK_2_0([2.16.0])

# gdk-pixbuf-2.0
PKG_CHECK_MODULES(GDK, gdk-pixbuf-2.0,,, AC_MSG_ERROR([Need gdk-2.0. Download from http://www.gtk.org .]))
AC_SUBST(GDK_CFLAGS)
AC_SUBST(GDK_LIBS)

# cairo and cairo-ps (PostScript output)
PKG_CHECK_MODULES(CAIRO, [cairo cairo-ps],,, AC_MSG_ERROR([Need cairo. Download from http://www.cairographics.org .]))
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

AM_PATH_XML2()
AC_SUBST([XML_CPPFLAGS])
AC_SUBST([XML_LIBS])

# kure2
PKG_CHECK_MODULES(KURE, kure2,,, AC_MSG_ERROR([Need Kure2 library.]))
AC_SUBST([KURE_CFLAGS])
AC_SUBST([KURE_LIBS])

AC_CONFIG_FILES([Makefile
                 src/Makefile
		         relview-8.2.pc])

AC_OUTPUT

echo
echo "Building RelView with prefix=$prefix, datarootdir=$datarootdir"
echo
echo "Packages"
echo "     Kure2:           $KURE_CFLAGS, $KURE_LIBS"
echo "  GLib-2.0:           $GLIB_CFLAGS, $GLIB_LIBS"
echo "    CFLAGS:	    $CFLAGS"
echo
echo "Build with 'make' and install with 'make install'. Enjoy!"
echo
