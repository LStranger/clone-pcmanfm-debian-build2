#include "file-icon.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>

#include "xdgmime.h"

static GHashTable *icon_table = NULL;
static GdkPixbuf* folder_icon32 = NULL;
static GdkPixbuf* folder_icon20 = NULL;
static GdkPixbuf* regular_file_icon32 = NULL;

void file_icon_init( const char*theme_name )
{
  GtkSettings* settings = gtk_settings_get_default();
  GtkIconTheme* icon_theme;

  /* Dirty hack to set icon theme */
  if( settings ) {
    g_object_set ( settings, "gtk-icon-theme-name",
                   theme_name ? theme_name : "gnome", NULL );
  }

  icon_theme = gtk_icon_theme_get_default();

  folder_icon20 = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-directory", 20, 0, NULL );

  folder_icon32 = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-directory", 32, 0, NULL );

  regular_file_icon32 = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-regular",  32, 0, NULL );

  icon_table = g_hash_table_new_full( g_direct_hash,
                                               NULL,
                                               NULL,
                                               (GDestroyNotify)gdk_pixbuf_unref );
}

void file_icon_clean()
{
  g_hash_table_destroy( icon_table );
}

GdkPixbuf* get_mime_icon( const char *mime )
{
  const char* sep = strchr( mime, '/' );
  char icon_name[100] = "gnome-mime-";
  GtkIconTheme *icon_theme;
  GdkPixbuf* icon = NULL;
  GError* err = NULL;

  if( G_UNLIKELY( mime == XDG_MIME_TYPE_DIRECTORY ) )
    return get_folder_icon32();

  icon = g_hash_table_lookup( icon_table, mime );

  if( icon ){
/*    g_print( "icon %s found!\n", mime );  */
    return g_object_ref( icon );
  }

  if( !sep )
    return NULL;

  strncat( icon_name, mime, (sep - mime) );
  strcat( icon_name, "-" );
  strcat( icon_name, sep + 1 );

  gdk_screen_get_default ();

  icon_theme = gtk_icon_theme_get_default ();
  icon = gtk_icon_theme_load_icon (icon_theme, icon_name,  32, GTK_ICON_LOOKUP_USE_BUILTIN, &err);

  if( ! icon ) {
    icon_name[11] = 0;
    strncat( icon_name, mime, (sep - mime) );
    icon = gtk_icon_theme_load_icon ( icon_theme, icon_name,  32, 0, NULL);
  }

  g_hash_table_insert( icon_table, (gpointer)mime, (gpointer)icon );

  return (icon ? g_object_ref( icon ) : NULL);
}

GdkPixbuf* get_folder_icon32()
{
  return folder_icon32;
}

GdkPixbuf* get_folder_icon20()
{
  return folder_icon20;
}

GdkPixbuf* get_regular_file_icon32()
{
  return regular_file_icon32;
}

GdkPixbuf* get_home_dir_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-home", size, 0, NULL );
}

GdkPixbuf* get_desktop_dir_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-desktop", size, 0, NULL );
}

GdkPixbuf* get_trash_empty_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-trash-empty", size, 0, NULL );
}

GdkPixbuf* get_trash_full_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-trash-full", size, 0, NULL );
}

GdkPixbuf* get_harddisk_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-dev-harddisk", size, 0, NULL );
}

GdkPixbuf* get_blockdev_icon(int size)
{
  GdkPixbuf* icon;
  GtkIconTheme* icon_theme;
  icon_theme = gtk_icon_theme_get_default();

  icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-blockdev", size, 0, NULL );
}


