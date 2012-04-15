#ifndef	_FILE_ICON_H_
#define	_FILE_ICON_H_

#include <gtk/gtk.h>

void file_icon_init( const char*theme_name );
void file_icon_clean();

GdkPixbuf* get_mime_icon( const char *mime );
GdkPixbuf* get_folder_icon32();
GdkPixbuf* get_folder_icon20();
GdkPixbuf* get_regular_file_icon32();

GdkPixbuf* get_home_dir_icon(int size);
GdkPixbuf* get_desktop_dir_icon(int size);
GdkPixbuf* get_trash_empty_icon(int size);
GdkPixbuf* get_trash_full_icon(int size);
GdkPixbuf* get_harddisk_icon(int size);
GdkPixbuf* get_blockdev_icon(int size);

#endif
