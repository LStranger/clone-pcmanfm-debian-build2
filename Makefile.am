## Process this file with automake to produce Makefile.in

SUBDIRS = src po

desktopdir = $(datadir)/applications
desktop_DATA = pcmanfm.desktop

mimedir = $(datadir)/mime/packages
mime_DATA = src/mime-type/libmimetype.xml

icondir = $(datadir)/pixmaps
icon_DATA = pcmanfm.png

filedir = $(datadir)/pcmanfm
fallbackicondir=$(filedir)/icons
uidir=$(filedir)/ui

file_DATA = \
	data/mount.rules
fallbackicon_DATA= \
	data/icons/file.png \
	data/icons/folder.png
ui_DATA= \
	data/ui/prefdlg.glade \
	data/ui/appchooserdlg.glade \
	data/ui/file_properties.glade \
	data/ui/about-dlg.glade

EXTRA_DIST = \
	autogen.sh \
	pcmanfm.desktop.in \
	$(icon_DATA) \
	$(mime_DATA) \
	$(file_DATA) \
	$(fallbackicon_DATA) \
	$(ui_DATA)

##install-data-hook:
##	update-mime-database $(datadir)/mime
##	update-desktop-database $(desktopdir)

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
	for pixmap in $(srcdir)/pixmaps/*; do \
        	if test -f $$pixmap; then \
			$(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
		fi \
	done \
	fi

dist-hook:
	if test -d pixmaps; then \
		mkdir $(distdir)/pixmaps; \
		for pixmap in pixmaps/*; do \
			if test -f $$pixmap; then \
				cp -p $$pixmap $(distdir)/pixmaps; \
			fi \
		done \
	fi

