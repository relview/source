#
# Makefile.am
#
#  Copyright (C) 2008,2009,2010,2011 Stefan Bolus, University of Kiel, Germany
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

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src

.PHONY: doc
doc:
	$(MAKE) -C doc

pkgconfigdir = $(libdir)/pkgconfig

# Distribute the pkg-config .pc file.
pkgconfig_DATA = relview-8.2.pc

# IMPORTANT: If you change the directory relview-8.0 to something like
#    like "8.x", then you have to change the function  rv_find_data_file
#    in src/Relview.c as well.

rvincludedir = $(includedir)/relview-8.2/relview
rvdatadir = $(datadir)/relview-8.2
labeldir = $(rvdatadir)/labels
programdir = $(rvdatadir)/programs

# install start-up files, but don't distribute them
dist_rvdata_DATA =  data/xdd.dtd data/relview.glade data/base_functions.pdf

# Headers which shall be installed, e.g. for plugins.
rvinclude_HEADERS = $(top_srcdir)/include/plugin.h \
	$(top_srcdir)/include/RelviewTypes.h \
	$(top_srcdir)/include/Observer.h \
	$(top_srcdir)/include/Namespace.h \
	$(top_srcdir)/include/Relview.h \
	$(top_srcdir)/include/Relation.h \
	$(top_srcdir)/include/Graph.h \
	$(top_srcdir)/include/GraphBuilder.h \
	$(top_srcdir)/include/Domain.h \
	$(top_srcdir)/include/Function.h \
	$(top_srcdir)/include/Program.h \
	$(top_srcdir)/include/Menu.h \
	$(top_srcdir)/include/IOHandler.h \
	$(top_srcdir)/include/FileLoader.h \
	$(top_srcdir)/include/label.h \
	$(top_srcdir)/include/GraphWindow.h \
	$(top_srcdir)/include/RelationWindow.h \
	$(top_srcdir)/include/RelationViewport.h

#
# Use
#    $ ls -1 <dir>/* | awk '{print "\t"$1" \\"}'
# to list the files in a directory for distribution.

EXTRA_DIST =

# Include all the headers in the distribution.
EXTRA_DIST += include/compute.h \
	include/DebugWindow.h \
	include/DirWindow.h \
	include/Domain.h \
	include/Eps.h \
	include/EvalWindow.h \
	include/FileLoader.h \
	include/file_ops.h \
	include/FileUtils.h \
	include/FileWindow.h \
	include/Function.h \
	include/global.h \
	include/GraphBuilder.h \
	include/graph.h \
	include/Graph.h \
	include/GraphImpl.h \
	include/GraphWindow.h \
	include/GraphWindowImpl.h \
	include/history.h \
	include/input_string_gtk.h \
	include/IOHandler.h \
	include/IterWindow.h \
	include/LabelChooserDialog.h \
	include/label.h \
	include/labelparser.h \
	include/LabelWindow.h \
	include/mark_edgesnodes_gtk.h \
	include/Menu.h \
	include/Misc.h \
	include/msg_boxes.h \
	include/multiop.h \
	include/multiopwindow_gtk.h \
	include/multiopwindow_intf.h \
	include/Namespace.h \
	include/Observer.h \
	include/OptionsDialog.h \
	include/plugin.h \
	include/pluginImpl.h \
	include/PluginWindow.h \
	include/prefs.h \
	include/Program.h \
	include/random.h \
	include/Relation.h \
	include/RelationImpl.h \
	include/RelationProxyAdapter.h \
	include/RelationProxyFactory.h \
	include/RelationProxy.h \
	include/RelationViewport.h \
	include/RelationWindow.h \
	include/Relview.h \
	include/RelviewTypes.h \
	include/RelviewWindow.h \
	include/rvops_gtk.h \
	include/Semaphore.h \
	include/testwindow_gtk.h \
	include/utilities.h \
	include/version.h \
	include/Workspace.h \
	include/XddFile.h

# distribute labels and RelViews example programs
dist_label_DATA = data/labels/Example.label
dist_program_DATA = data/programs/ApproxAlg.prog \
	data/programs/Aux.prog \
	data/programs/Cyclefree.prog \
	data/programs/DedekindCuts.prog \
	data/programs/DFS.prog \
	data/programs/Graph.prog \
	data/programs/KernelCyclefree.prog \
	data/programs/KernelEnumerate.prog \
	data/programs/Misc.prog \
	data/programs/OrderLattice.prog \
	data/programs/PetriNets.prog \
	data/programs/RecursiveProg.prog \
	data/programs/ReducedGraph.prog \
	data/programs/SpannTree.prog \
	data/programs/TransClosure.prog \
	data/programs/VertexBase.prog

EXTRA_DIST += CHANGES LICENSE COMPILE FILES README

# doc files for the distribution
EXTRA_DIST += doc/Makefile doc/doxygen/Makefile \
	doc/doxygen/index.html doc/doxygen/index.txt \
	doc/doxygen/Doxyfile
