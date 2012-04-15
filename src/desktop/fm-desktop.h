#ifndef _FM_DESKTOP_H_
#define _FM_DESKTOP_H_

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

int fm_desktop_init();
void fm_desktop_cleanup();

GtkWidget* fm_desktop_new( GdkScreen* screen );
void fm_desktop_update_wallpaper();
void fm_desktop_update_colors();
void fm_desktop_update_thumbnails();
void fm_desktop_update_view();

G_END_DECLS

#endif
