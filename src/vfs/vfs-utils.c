/*
 *      vfs-utils.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "vfs-utils.h"

GdkPixbuf* vfs_load_icon( GtkIconTheme* theme, const char* icon_name, int size )
{
    GdkPixbuf* icon = NULL;
    const char* file;
    GtkIconInfo* inf = gtk_icon_theme_lookup_icon( theme, icon_name, size,
                                             GTK_ICON_LOOKUP_USE_BUILTIN );
    if( G_UNLIKELY( ! inf ) )
        return NULL;

    file = gtk_icon_info_get_filename( inf );
    if( G_LIKELY( file ) )
        icon = gdk_pixbuf_new_from_file( file, NULL );
    else
        icon = gtk_icon_info_get_builtin_pixbuf( inf );
    gtk_icon_info_free( inf );

    if( G_LIKELY( icon ) )  /* scale down the icon if it's too big */
    {
        int width, height;
        height = gdk_pixbuf_get_height(icon);
        width = gdk_pixbuf_get_width(icon);

        if( G_UNLIKELY( height > size || width > size ) )
        {
            GdkPixbuf* scaled;
            if( height > width )
            {
                width = size * height / width;
                height = size;
            }
            else if( height < width )
            {
                height = size * width / height;
                width = size;
            }
            else
                height = width = size;
            scaled = gdk_pixbuf_scale_simple( icon, width, height, GDK_INTERP_BILINEAR );
            g_object_unref( icon );
            icon = scaled;
        }
    }
    return icon;
}
