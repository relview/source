# Has to be of the form $(PACKAGE_NAME)-$(PACKAGE_VERSION)
# Also used in src/Relview.c.
common_defines = \
        -DRELVIEW_DATADIR="\"$(datadir)/relview-8.2\""

EXTRA_DIST =

gui_sources = gui/FileWindow.c\
		gui/input_string2_gtk.c \
        gui/DebugWindow.c \
        gui/DirWindow.c \
        gui/EvalWindow.c \
        gui/Edge.c \
        gui/Edge.h \
        gui/GraphWindow.c \
        gui/Menu.c \
        gui/Node.c \
        gui/Node.h \
        gui/SelectionManager.c \
        gui/SelectionManager.h \
        gui/Vec2d.c \
        gui/Vec2d.h \
        gui/graphwindow_gtk.c \
        gui/input_string_gtk.c \
        gui/IterWindow.c \
        gui/LabelChooserDialog.c \
        gui/LabelWindow.c \
        gui/mark_edgesnodes_gtk.c \
        gui/MessageBoxes.c \
        gui/multiop.c \
        gui/multiopwindow_gtk.c \
        gui/multiopwindow_intf.c \
        gui/OptionsDialog.c \
        gui/RelationViewport.c \
        gui/RelationWindow.c \
        gui/RelviewWindow.c \
        gui/rvops_gtk.c \
        gui/rvops_intf.c \
        gui/testwindow_gtk.c \
        gui/testwindow_intf.c \
        gui/PluginWindow.c \
	gui/GraphWindowDisplay.c

label_sources = label/labelparser.y \
        label/labellexer.l \
		label/Label.cpp

other_sources = Prefs.c \
		Eps.c \
		FileUtils.c\
		IOHandler.c \
		Namespace.c \
		Workspace.c \
        Domain.c \
        file_ops.c \
        Function.c \
        Graph.c \
        Program.c \
        util.c \
        version.c \
        Semaphore.c \
        compute.c \
        history.c \
        Relview.c \
        Relation.c \
        RelationProxyAdapter.c \
        main.c \
		XddFile.c \
		FileLoader.c \
		PlugInManager.c \
		plugin.c \
		GraphUtils.c


bin_PROGRAMS = relview-bin
relview_bin_SOURCES = $(label_sources) $(gui_sources) $(bdd_sources) \
	$(relation_sources) $(other_sources)
relview_bin_LDADD = $(GLIB_LIBS) $(GTK_LIBS) $(CAIRO_LIBS) \
	$(GDK_LIBS) $(KURE_LIBS) $(XML_LIBS)

BUILT_SOURCES = label/labelparser.h

# tell bison (and automake) that we also need a header file for our parsers.
AM_YFLAGS = -d

# tell the compiler where it can find config.h and the header files for the
# label- and the relview-language-parser.
AM_CPPFLAGS = -I$(top_srcdir) -Ilabel \
	-Iexport_import -Iinclude \
	-I$(top_srcdir)/include $(GLIB_CFLAGS) $(GTK_CFLAGS) $(CAIRO_CFLAGS) \
	$(GDK_CFLAGS) $(common_defines)	$(KURE_CFLAGS) $(XML_CPPFLAGS)

# build distribute the relview.in script. It's not easily possible to let
# configure do that due to relative paths, e.g. @bindir@ will become
# $prefix/bin, not /usr/local/bin .
bin_SCRIPTS = relview
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += relview.in

relview_binary = $(shell echo "relview-bin" | sed '$(transform)')
edit = sed \
	  -e 's|@bindir[@]|$(bindir)|g' \
	  -e 's|@relview_binary[@]|$(relview_binary)|g' \
	  -e 's|@AWK[@]|$(AWK)|g'

relview: relview.in Makefile
	rm -f $@ $@.tmp
	$(edit) '$(srcdir)/$@.in' >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@
