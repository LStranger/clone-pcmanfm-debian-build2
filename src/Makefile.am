## Process this file with automake to produce Makefile.in

INCLUDES = \
    -I$(top_srcdir)/src/mime-type \
    -I$(top_srcdir)/src/compat \
    -I$(top_srcdir)/src/ptk \
    -I$(top_srcdir)/src/ptk-ui-xml \
    -I$(top_srcdir)/src/vfs \
    -I$(top_srcdir)/src/libmd5-rfc \
    -I$(top_srcdir)/src/desktop \
    -I$(top_srcdir)/src/inotify \
    -DPACKAGE_DATA_DIR=\""$(datadir)/pcmanfm"\" \
    -DPACKAGE_UI_DIR=\""$(datadir)/pcmanfm/ui"\" \
    -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
    @PACKAGE_CFLAGS@ @HAL_CFLAGS@

bin_PROGRAMS = pcmanfm

MIMETYE_SOURCES = \
    mime-type/mime-type.c mime-type/mime-type.h \
    mime-type/mime-action.c mime-type/mime-action.h \
    mime-type/mime-cache.c mime-type/mime-cache.h

MD5_SOURCES = \
    libmd5-rfc/md5.c libmd5-rfc/md5.h

VOLUME_HAL_SOURCES= \
    vfs/vfs-volume-hal.c \
    vfs/vfs-volume-hal-options.h \
    vfs/vfs-volume-hal-options.c

VOLUME_NOHAL_SOURCES= \
    vfs/vfs-volume-nohal.c

if USE_HAL
VOLUME_SOURCES= \
    $(VOLUME_HAL_SOURCES)
else
VOLUME_SOURCES= \
    $(VOLUME_NOHAL_SOURCES)
endif

VFS_SOURCES = \
    vfs/vfs-file-monitor.c vfs/vfs-file-monitor.h \
    vfs/vfs-mime-type.c vfs/vfs-mime-type.h \
    vfs/vfs-file-info.c vfs/vfs-file-info.h \
    vfs/vfs-dir.c vfs/vfs-dir.h \
    vfs/vfs-app-desktop.c vfs/vfs-app-desktop.h \
    vfs/vfs-file-task.c vfs/vfs-file-task.h \
    vfs/vfs-volume.h \
    $(VOLUME_SOURCES) \
    vfs/vfs-execute.c vfs/vfs-execute.h \
    vfs/vfs-async-task.c vfs/vfs-async-task.h \
    vfs/vfs-thumbnail-loader.c vfs/vfs-thumbnail-loader.h

PTK_SOURCES = \
    ptk/ptk-file-browser.c ptk/ptk-file-browser.h \
    ptk/ptk-file-list.c ptk/ptk-file-list.h \
    ptk/ptk-icon-view.c ptk/ptk-icon-view.h \
    ptk/ptk-path-entry.c ptk/ptk-path-entry.h \
    ptk/ptk-text-renderer.c ptk/ptk-text-renderer.h \
    ptk/exo-marshal.c ptk/exo-marshal.h \
    ptk/exo-string.c ptk/exo-string.h \
    ptk/ptk-file-icon-renderer.c ptk/ptk-file-icon-renderer.h \
    ptk/ptk-utils.c ptk/ptk-utils.h \
    ptk/ptk-dir-tree-view.c ptk/ptk-dir-tree-view.h \
    ptk/ptk-dir-tree.c ptk/ptk-dir-tree.h \
    ptk/ptk-location-view.c ptk/ptk-location-view.h \
    ptk/ptk-bookmarks.c ptk/ptk-bookmarks.h \
    ptk/ptk-input-dialog.c ptk/ptk-input-dialog.h \
    ptk/ptk-file-task.c ptk/ptk-file-task.h \
    ptk/ptk-file-archiver.c ptk/ptk-file-archiver.h \
    ptk/ptk-console-output.c ptk/ptk-console-output.h \
    ptk/ptk-clipboard.c ptk/ptk-clipboard.h \
    ptk/ptk-file-menu.c ptk/ptk-file-menu.h \
    ptk/ptk-file-misc.c ptk/ptk-file-misc.h

PTK_UI_XML_SOURCES= \
    ptk-ui-xml/ptk-ui-xml.c ptk-ui-xml/ptk-ui-xml.h \
    ptk-ui-xml/ptk-xml-tree.c ptk-ui-xml/ptk-xml-tree.h

DESKTOP_SOURCES = \
    desktop/fm-desktop.c desktop/fm-desktop.h \
    desktop/working-area.c desktop/working-area.h

INOTIFY_SOURCES = \
    inotify/inotify-syscalls.h \
    inotify/linux-inotify.h

COMPAT_SOURCES = \
    compat/glib-mem.h \
    compat/glib-utils.h \
    compat/glib-utils.c

pcmanfm_SOURCES = \
    $(MIMETYE_SOURCES) \
    $(PTK_SOURCES) \
    $(PTK_UI_XML_SOURCES) \
    $(INOTIFY_SOURCES) \
    $(VFS_SOURCES) \
    $(MD5_SOURCES) \
    $(DESKTOP_SOURCES) \
    $(COMPAT_SOURCES) \
    private.h \
    main.c \
    main-window.c main-window.h \
    file-properties.c file-properties.h \
    settings.c settings.h \
    pref-dialog.c pref-dialog.h \
    edit-bookmarks.c edit-bookmarks.h \
    app-chooser-dialog.c app-chooser-dialog.h \
    file-assoc-dlg.c file-assoc-dlg.h \
    file-assoc-dlg-ui.c file-assoc-dlg-ui.h

pcmanfm_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) @FAM_LIBS@ @HAL_LIBS@

