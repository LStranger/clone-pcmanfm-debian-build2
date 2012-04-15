/*
*  C Implementation: PtkLocationView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <glib.h>
#include <glib/gi18n.h>

#include "ptk-location-view.h"
#include "file-icon.h"
#include <stdio.h>

#include "ptk-bookmarks.h"

#if 0
/* Linux specific */
#if defined (__linux__)
#include <mntent.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libhal.h>
#endif

#endif


static int n_ref = 0;
static GtkTreeModel* model = NULL;

static void ptk_location_view_init_model( GtkListStore* list );

/*
static LibHalContext* hal_context = NULL;
*/

enum {
  COL_ICON = 0,
  COL_NAME,
  COL_PATH,
  N_COLS
};

#if 0
static void on_hal_device_added( LibHalContext *ctx, const char *udi )
{
  char* prop;
  g_print("Add %s\n", udi);
  prop = libhal_device_get_property_string (hal_context,
                                            udi, "volume.label", NULL);
  g_print( "%s\n", prop );
  libhal_free_string (prop);

  prop = libhal_device_get_property_string (hal_context,
                                            udi, "volume.fstype", NULL);
  g_print( "%s\n", prop );
  libhal_free_string (prop);

  dbus_uint64_t size = libhal_device_get_property_uint64 (hal_context,
                                            udi, "volume.size", NULL);
  g_print( "size: %llu\n", size );

  prop = libhal_device_get_property_string (hal_context,
                                            udi, "storage.vendor", NULL);
  g_print( "vendor: %s\n", prop );
  libhal_free_string (prop);

  prop = libhal_device_get_property_string (hal_context,
                                            udi, "storage.model", NULL);
  g_print( "model: %s\n", prop );
  libhal_free_string (prop);
}

static void on_hal_device_removed( LibHalContext *ctx, const char *udi )
{
  g_print("Remove %s\n", udi);
}

static void on_hal_property_modified( LibHalContext *ctx,
                                      const char *udi,
                                      const char *key,
                                      dbus_bool_t is_removed,
                                      dbus_bool_t is_added )
{
  g_print("Modified %s, key = %s\n", udi, key );
}

/* This function is taken from gnome volumen manager */
static dbus_bool_t
hal_mainloop_integration (LibHalContext *ctx, DBusError *error)
{
  DBusConnection *dbus_connection;
  dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, error);

  if (dbus_error_is_set (error))
    return FALSE;

  dbus_connection_setup_with_g_main (dbus_connection, NULL);
  libhal_ctx_set_dbus_connection (ctx, dbus_connection);
  return TRUE;
}

static gboolean init_hal()
{
  DBusError error;

  char** devices;
  char** device;
  int n;

  if( hal_context )
    return TRUE;

  hal_context = libhal_ctx_new ();

  if( hal_context )
  {
    dbus_error_init (&error);
    if( hal_mainloop_integration ( hal_context, &error ) )
    {
      libhal_ctx_set_device_added (hal_context, on_hal_device_added);
      libhal_ctx_set_device_removed (hal_context, on_hal_device_removed);
      /*
      libhal_ctx_set_device_new_capability (ctx, on_hal_device_new_capability);
      libhal_ctx_set_device_lost_capability (ctx, on_hal_device_lost_capability);
      libhal_ctx_set_device_condition (ctx, on_hal_device_condition);
      */
      libhal_ctx_set_device_property_modified (hal_context, on_hal_property_modified);

      if( libhal_device_property_watch_all (hal_context, &error) )
      {
        if( libhal_ctx_init (hal_context, &error) )  {
          dbus_error_free (&error);
          devices = libhal_find_device_by_capability (hal_context, "volume", &n, &error);
          if( devices ) {
            for( device = devices; *device; ++device ) {
              g_print( "%s\n", *device );
              char* mp = libhal_device_get_property_string (hal_context,
                  *device, "volume.mount_point", NULL);
              /* g_print( "%s\n", mp ); */
              libhal_free_string (mp);
            }
            libhal_free_string_array (devices);
            return TRUE;
          }
          else {
            g_warning ("HAL is not running: %s", error.message);
          }
        }
        else {
          g_warning ("%s\n", error.message);
        }
      }
      else {
        g_warning ("%s\n", error.message);
      }
    }
    libhal_ctx_free (hal_context);
    hal_context = NULL;
    dbus_error_free (&error);
  }
  else {
    g_warning ("%s\n", error.message);
  }
  return FALSE;
}
#endif /* Disable hal currently */


static void on_bookmark_changed( PtkBookmarks* bookmarks, gpointer data )
{
  gtk_list_store_clear( GTK_LIST_STORE(model) );
  ptk_location_view_init_model( GTK_LIST_STORE(model) );
}

static void on_view_destroy( GtkObject* view )
{
  --n_ref;
  if( n_ref <= 0 ) {
    ptk_bookmarks_remove_callback((GSourceFunc)on_bookmark_changed, NULL);
    model = NULL;
  }
}

void ptk_location_view_init_model( GtkListStore* list )
{
  int pos = 0;
  GtkTreeIter it;
  GdkPixbuf* icon;
  gchar* name;
  gchar* uname;
  gchar* real_path;
  PtkBookmarks* bookmarks;
  GList* l;
#if defined (__linux__)
/*
  FILE* fstab;
  struct mntent * mnt_ent;
*/
#endif

  real_path = (gchar*)g_get_home_dir();

  name = g_path_get_basename( real_path );
  icon = get_home_dir_icon(20);
  gtk_list_store_insert_with_values( list, &it, pos,
                                     COL_ICON,
                                     icon,
                                     COL_NAME,
                                     name,
                                     COL_PATH,
                                     real_path, -1);
  gdk_pixbuf_unref(icon);

  icon = get_desktop_dir_icon(20);
  real_path = g_build_filename( real_path, "Desktop", NULL );
  gtk_list_store_insert_with_values( list, &it, ++pos,
                                     COL_ICON,
                                     icon,
                                     COL_NAME,
                                     _("Desktop"),
                                     COL_PATH,
                                     real_path, -1);
  g_free( real_path );
  gdk_pixbuf_unref(icon);

/*  FIXME: I don't like trash can
  icon = get_trash_empty_icon(20);
  real_path = g_build_filename( g_get_home_dir(), ".Trash", NULL );
  gtk_list_store_insert_with_values( list, &it, ++pos,
                                     COL_ICON,
                                     icon,
                                     COL_NAME,
                                     _("Trash"),
                                     COL_PATH,
                                     real_path, -1);
  g_free( real_path );
  gdk_pixbuf_unref(icon);
*/

/*
  icon = get_harddisk_icon(20);
  gtk_list_store_insert_with_values( list, &it, ++pos,
                                     COL_ICON,
                                     icon,
                                     COL_NAME,
                                     _("File System"),
                                     COL_PATH,
                                     "/", -1);
  gdk_pixbuf_unref(icon);
*/
  /* Separator */
  ++pos;
  gtk_list_store_append( list, &it );

  /* Disable HAL support currently */
#if 0 /* defined (__linux__) */
  /* Add mountable devices to the list */
  if( init_hal() )
  {
    icon = get_harddisk_icon(20);
#if 0
    fstab = setmntent("/etc/fstab", "r");
    while( mnt_ent = getmntent( fstab ) )
    {
      if( 0 == strncmp( "proc", mnt_ent->mnt_type, 4 ) )
        continue;
      if( 0 == strncmp( "swap", mnt_ent->mnt_type, 4 ) )
        continue;
      gtk_list_store_insert_with_values( list, &it, ++pos,
                                        COL_ICON,
                                        icon,
                                        COL_NAME,
                                        mnt_ent->mnt_dir,
                                        COL_PATH,
                                        mnt_ent->mnt_dir, -1);
    }
    endmntent( fstab );
#endif
    gdk_pixbuf_unref(icon);
  }

  /* Separator */
  ++pos;
  gtk_list_store_append( list, &it );

#endif /* defined (__linux__) */

  /* Add bookmarks */
  bookmarks = ptk_bookmarks_get();

  icon = get_folder_icon20();
  for( l = bookmarks->list; l; l = l->next ) {
    name = (char*)l->data;
    gtk_list_store_insert_with_values( list, &it, ++pos,
                                       COL_ICON,
                                       icon,
                                       COL_NAME,
                                       name,
                                       COL_PATH,
                                       ptk_bookmarks_item_get_path(name), -1);
  }

  /*
  NOTICE: gdk_pixbuf_unref(icon); is not necessary.
          Folder icon should not be unref, since it's shared by
          everywhere in the program.
  */
  return GTK_TREE_MODEL(list);
}

gboolean view_sep_func( GtkTreeModel* model,
                        GtkTreeIter* it, gpointer data )
{
  GdkPixbuf* icon;
  gtk_tree_model_get( model, it, COL_ICON, &icon, -1 );
  if( G_LIKELY( icon ) ) {
    gdk_pixbuf_unref( icon );
    return FALSE;
  }
  return TRUE;
}

GtkWidget* ptk_location_view_new()
{
  GtkTreeView* view;
  GtkTreeViewColumn* col;
  GtkCellRenderer* renderer;
  GtkListStore* list;

  if( !model ) {
    list = gtk_list_store_new( N_COLS,
                               GDK_TYPE_PIXBUF,
                               G_TYPE_STRING,
                               G_TYPE_STRING );
    ptk_location_view_init_model( list );
    ptk_bookmarks_add_callback( (GSourceFunc)on_bookmark_changed, NULL );
    model = (GtkTreeModel*) list;
  }
  ++n_ref;
  view = gtk_tree_view_new_with_model( model );
  gtk_tree_view_set_headers_visible( view, FALSE );
  gtk_tree_view_set_row_separator_func( view,
                    (GtkTreeViewRowSeparatorFunc)view_sep_func,
                    NULL, NULL);

  col = gtk_tree_view_column_new();
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start( col, renderer, FALSE );
  gtk_tree_view_column_set_attributes(col, renderer,
                                      "pixbuf", COL_ICON, NULL);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start( col, renderer, TRUE );
  gtk_tree_view_column_set_attributes( col, renderer,
                                       "text", COL_NAME, NULL );

  gtk_tree_view_append_column ( view, col );

  g_signal_connect( view, "destroy", on_view_destroy, NULL );
  return GTK_WIDGET( view );
}

gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path )
{
  GtkTreeIter it;
  GtkTreeSelection* tree_sel;
  char* real_path;
  if( !path )
    return FALSE;

  tree_sel = gtk_tree_view_get_selection( location_view );
  if( gtk_tree_model_get_iter_first( model, &it ) ) {
    do {
      gtk_tree_model_get( model, &it, COL_PATH, &real_path, -1 );
      if( real_path ) {
        if( 0 == strcmp( path, real_path ) ) {
          g_free( real_path );
          gtk_tree_selection_select_iter( tree_sel, &it );
          return TRUE;
        }
        g_free( real_path );
      }
    }while( gtk_tree_model_iter_next (model, &it) );
  }
  gtk_tree_selection_unselect_all ( tree_sel );
  return FALSE;
}

char* ptk_location_view_get_selected_dir( GtkTreeView* location_view )
{
  GtkTreeIter it;
  GtkTreeSelection* tree_sel;
  char* real_path = NULL;

  tree_sel = gtk_tree_view_get_selection( location_view );
  if( gtk_tree_selection_get_selected(tree_sel, NULL, &it) ) {
    gtk_tree_model_get( model, &it, COL_PATH, &real_path, -1 );
  }
  return real_path;
}

