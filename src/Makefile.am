## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/src/xdgmime \
	-I$(top_srcdir)/src/ptk \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = pcmanfm

XDGMIME_SOURCES= \
	xdgmime/xdgmimealias.c xdgmime/xdgmimealias.h \
	xdgmime/xdgmime.c xdgmime/xdgmime.h \
	xdgmime/xdgmimecache.c xdgmime/xdgmimecache.h \
	xdgmime/xdgmimeglob.c xdgmime/xdgmimeglob.h \
	xdgmime/xdgmimeint.c xdgmime/xdgmimeint.h \
	xdgmime/xdgmimemagic.c xdgmime/xdgmimemagic.h \
	xdgmime/xdgmimeparent.c xdgmime/xdgmimeparent.h

PTK_SOURCES= \
	ptk/ptk-file-browser.c ptk/ptk-file-browser.h \
	ptk/ptk-icon-view.c ptk/ptk-icon-view.h \
	ptk/exo-marshal.c ptk/exo-marshal.h \
	ptk/exo-string.c ptk/exo-string.h \
	ptk/ptk-tree-model-sort.c ptk/ptk-tree-model-sort.h \
	ptk/ptk-file-icon-renderer.c ptk/ptk-file-icon-renderer.h \
	ptk/ptk-utils.c ptk/ptk-utils.h \
	ptk/ptk-dir-tree-view.c ptk/ptk-dir-tree-view.h \
	ptk/ptk-location-view.c ptk/ptk-location-view.h \
	ptk/ptk-bookmarks.c ptk/ptk-bookmarks.h
pcmanfm_SOURCES = \
	$(XDGMIME_SOURCES) \
	$(PTK_SOURCES) \
	main.c \
	glade-support.c glade-support.h \
	main-window.c main-window.h \
	file-icon.c file-icon.h \
	mime-action.c mime-action.h \
	input-dialog.c input-dialog.h \
	folder-content.c folder-content.h \
	file-operation.c file-operation.h \
	file-operation-ui.c file-operation-ui.h \
	file-properties.c file-properties.h \
	file-properties-ui.c file-properties-ui.h \
	settings.c settings.h \
	pref-dialog.c pref-dialog.h \
	pref-dialog-ui.c pref-dialog-ui.h \
	edit-bookmarks.c edit-bookmarks.h \
	mime-description.c mime-description.h \
	app-chooser-dialog.c app-chooser-dialog.h \
	app-chooser-dialog-ui.c app-chooser-dialog-ui.h

pcmanfm_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) @FAM_LIBS@

