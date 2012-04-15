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
#include <stdio.h>
#include <string.h>

#include "ptk-bookmarks.h"
#include "ptk-utils.h"

#include "vfs-volume.h"

static GtkTreeModel* model = NULL;
static int n_vols = 0;
static guint theme_changed = 0; /* GtkIconTheme::"changed" handler */

static void ptk_location_view_init_model( GtkListStore* list );

static void on_volume_event ( VFSVolume* vol, VFSVolumeState state, gpointer user_data );

static void add_volume( VFSVolume* vol, gboolean set_icon );
static void remove_volume( VFSVolume* vol );
static void update_volume( VFSVolume* vol );

static void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                              GtkTreeViewColumn *col, gpointer user_data );
static gboolean on_button_press_event( GtkTreeView* view, GdkEventButton* evt,
                                       gpointer user_data );

enum {
    COL_ICON = 0,
    COL_NAME,
    COL_PATH,
    COL_DATA,
    N_COLS
};

static void on_bookmark_changed( gpointer bookmarks_, gpointer data )
{
    PtkBookmarks* bookmarks = (PtkBookmarks*)bookmarks_;
    gtk_list_store_clear( GTK_LIST_STORE( model ) );
    ptk_location_view_init_model( GTK_LIST_STORE( model ) );
}

static void on_model_destroy( gpointer data, GObject* object )
{
    GtkIconTheme* icon_theme;

    model = NULL;
    ptk_bookmarks_remove_callback( on_bookmark_changed, NULL );
    n_vols = 0;

    icon_theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect( icon_theme, theme_changed );
}

static void update_icons()
{
    GtkIconTheme* icon_theme;
    GtkTreeIter it;
    GdkPixbuf* icon;
    VFSVolume* vol;
    int i;

    GtkListStore* list = GTK_LIST_STORE( model );
    icon_theme = gtk_icon_theme_get_default();

    gtk_tree_model_get_iter_first( model, &it );
    icon = gtk_icon_theme_load_icon ( icon_theme, "gnome-fs-home", 20, 0, NULL );
    gtk_list_store_set( list, &it, COL_ICON, icon, -1 );
    if ( icon )
        gdk_pixbuf_unref( icon );

    gtk_tree_model_iter_next( model, &it );
    icon = gtk_icon_theme_load_icon ( icon_theme, "gnome-fs-desktop", 20, 0, NULL );
    gtk_list_store_set( list, &it, COL_ICON, icon, -1 );
    if ( icon )
        gdk_pixbuf_unref( icon );


    for( i = 0; i < n_vols; ++i )
    {
        if( G_UNLIKELY( ! gtk_tree_model_iter_next( model, &it ) ) )
            break;
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );
        icon = gtk_icon_theme_load_icon ( icon_theme,
                                          vfs_volume_get_icon( vol ),
                                          20, 0, NULL );
        gtk_list_store_set( GTK_LIST_STORE( model ), &it, COL_ICON, icon, -1 );
        if ( icon )
            gdk_pixbuf_unref( icon );
    }

    /* separator */
    gtk_tree_model_iter_next( model, &it );

    /* bookmarks */
    icon = gtk_icon_theme_load_icon ( icon_theme, "gnome-fs-directory", 20, 0, NULL );
    while( gtk_tree_model_iter_next( model, &it ) )
    {
        gtk_list_store_set( list, &it, COL_ICON, icon, -1 );
    }
    if ( icon )
        gdk_pixbuf_unref( icon );
}

void ptk_location_view_init_model( GtkListStore* list )
{
    int pos = 0;
    GtkTreeIter it;
    gchar* name;
    gchar* uname;
    gchar* real_path;
    PtkBookmarks* bookmarks;
    GList* l;
    VFSVolume** volumes;
    int i, n;

    real_path = ( gchar* ) g_get_home_dir();
    name = g_path_get_basename( real_path );
    gtk_list_store_insert_with_values( list, &it, pos,
                                       COL_NAME,
                                       name,
                                       COL_PATH,
                                       real_path, -1 );

    real_path = g_build_filename( real_path, "Desktop", NULL );
    gtk_list_store_insert_with_values( list, &it, ++pos,
                                       COL_NAME,
                                       _( "Desktop" ),
                                       COL_PATH,
                                       real_path, -1 );
    g_free( real_path );

    /*  FIXME: I don't like trash can
      icon = gtk_icon_theme_load_icon (icon_theme, "gnome-fs-trash-empty", 20, 0, NULL )
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

    n_vols = 0;
    volumes = vfs_volume_get_all_volumes( &n );
    vfs_volume_add_callback( on_volume_event, model );
    /* Add mountable devices to the list */
    for ( i = 0; i < n; ++i )
    {
        /* FIXME: Should we only show removable volumes?
        if( ! vfs_volume_is_removable( volumes[i] ) )
            continue; */
        add_volume( volumes[ i ], FALSE );
    }
    pos += n;

    /* Separator */
    gtk_list_store_append( list, &it );
    ++pos;

    /* Add bookmarks */
    bookmarks = ptk_bookmarks_get();

    for ( l = bookmarks->list; l; l = l->next )
    {
        name = ( char* ) l->data;
        gtk_list_store_insert_with_values( list, &it, ++pos,
                                           COL_NAME,
                                           name,
                                           COL_PATH,
                                           ptk_bookmarks_item_get_path( name ), -1 );
    }

    update_icons();
}

static gboolean view_sep_func( GtkTreeModel* model,
                               GtkTreeIter* it, gpointer data )
{
    GtkTreePath * tree_path;
    int i;
    tree_path = gtk_tree_model_get_path( model, it );
    if ( tree_path )
    {
        i = gtk_tree_path_get_indices( tree_path ) [ 0 ];
        gtk_tree_path_free( tree_path );
        return ( i == ( n_vols + 2 ) );
    }
    return TRUE;
}

static void on_bookmark_edited( GtkCellRendererText *cell,
                                gchar *path_string,
                                gchar *new_text,
                                GtkTreeView* view )
{
    char* path;
    GtkTreeIter it;

    gtk_tree_model_get_iter_from_string( model,
                                         &it, path_string );
    if( new_text && *new_text )
    {
        gtk_tree_model_get( model, &it, COL_PATH, &path, -1 );
        if( path )
        {
            ptk_bookmarks_rename( path, new_text );
            g_free( path );
        }
    }
}

GtkWidget* ptk_location_view_new()
{
    GtkTreeView * view;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkListStore* list;
    GtkIconTheme* icon_theme;

    if ( !model )
    {
        list = gtk_list_store_new( N_COLS,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_STRING,
                                   G_TYPE_POINTER );
        g_object_weak_ref( G_OBJECT( list ), on_model_destroy, NULL );
        model = ( GtkTreeModel* ) list;
        ptk_location_view_init_model( list );
        ptk_bookmarks_add_callback( on_bookmark_changed, NULL );
        icon_theme = gtk_icon_theme_get_default();
        theme_changed = g_signal_connect( icon_theme, "changed",
                                          G_CALLBACK( update_icons ), NULL );
    }
    else
    {
        g_object_ref( G_OBJECT( model ) );
    }

    view = GTK_TREE_VIEW(gtk_tree_view_new_with_model( model ));
    g_object_unref( G_OBJECT( model ) );

    gtk_tree_view_set_headers_visible( view, FALSE );
    gtk_tree_view_set_row_separator_func( view,
                                          ( GtkTreeViewRowSeparatorFunc ) view_sep_func,
                                          NULL, NULL );

    col = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "pixbuf", COL_ICON, NULL );

    renderer = gtk_cell_renderer_text_new();
    g_signal_connect( renderer, "edited", G_CALLBACK(on_bookmark_edited), view );
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", COL_NAME, NULL );

    gtk_tree_view_append_column ( view, col );

    g_signal_connect( view, "row-activated", G_CALLBACK( on_row_activated ), NULL );

    g_signal_connect( view, "button-press-event", G_CALLBACK( on_button_press_event ), NULL );

    return GTK_WIDGET( view );
}

gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    char* real_path;
    if ( !path )
        return FALSE;

    tree_sel = gtk_tree_view_get_selection( location_view );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, COL_PATH, &real_path, -1 );
            if ( real_path )
            {
                if ( 0 == strcmp( path, real_path ) )
                {
                    g_free( real_path );
                    gtk_tree_selection_select_iter( tree_sel, &it );
                    return TRUE;
                }
                g_free( real_path );
            }
        }
        while ( gtk_tree_model_iter_next ( model, &it ) );
    }
    gtk_tree_selection_unselect_all ( tree_sel );
    return FALSE;
}

static gboolean try_mount( GtkTreeView* view, VFSVolume* vol )
{
    GtkWidget* parent;
    GtkWidget* dlg;
    gboolean mount_ok = FALSE;

    parent = gtk_widget_get_toplevel( GTK_WIDGET(view) );
    dlg = gtk_message_dialog_new( GTK_WINDOW(parent),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_YES_NO,
                                  _("The volume \"%s\" is not mounted.\nDo you want to mount it?"),
                                  vfs_volume_get_disp_name( vol ) );
    mount_ok = (gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_YES );
    gtk_widget_destroy( dlg );

    if( mount_ok )
    {
        mount_ok = vfs_volume_mount( vol );
        if( ! mount_ok )
            ptk_show_error( GTK_WINDOW(parent), _("Unable to mount device") );
    }

    return mount_ok;
}

char* ptk_location_view_get_selected_dir( GtkTreeView* location_view )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    char* real_path = NULL;
    int i;
    VFSVolume* vol;

    tree_sel = gtk_tree_view_get_selection( location_view );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        gtk_tree_model_get( model, &it, COL_PATH, &real_path, -1 );

        if( ! real_path )
        {
            tree_path = gtk_tree_model_get_path( model, &it );
            i = gtk_tree_path_get_indices( tree_path )[0];
            gtk_tree_path_free( tree_path );
            if( i >= 2 && i < (2 + n_vols) ) /* volume */
            {
                gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );
                if( ! vfs_volume_is_mounted( vol ) )
                    try_mount( location_view, vol );

                real_path = (char*)vfs_volume_get_mount_point( vol );
                gtk_list_store_set( GTK_LIST_STORE(model), &it, COL_PATH, real_path, -1 );
                return g_strdup(real_path);
            }
        }
    }
    return real_path;
}

void on_volume_event ( VFSVolume* vol, VFSVolumeState state, gpointer user_data )
{
    switch ( state )
    {
    case VFS_VOLUME_ADDED:
        add_volume( vol, TRUE );
        vfs_volume_mount( vol );
        break;
    case VFS_VOLUME_REMOVED:
        remove_volume( vol );
        break;
    case VFS_VOLUME_CHANGED:
        update_volume( vol );
        break;
    /* FIXME:
        VFS_VOLUME_MOUNTED VFS_VOLUME_UNMOUNTED not handled */
    default:
        break;
    }
}

void add_volume( VFSVolume* vol, gboolean set_icon )
{
    GtkIconTheme * icon_theme;
    GdkPixbuf* icon;
    GtkTreeIter it;
    int pos;
    const char* mnt;

    if ( vfs_volume_is_removable( vol ) )
        pos = 2;
    else
        pos = 2 + n_vols;

    mnt = vfs_volume_get_mount_point( vol );
    if( mnt && !*mnt )
        mnt = NULL;
    /* FIXME: Volumes should be sorted */
    gtk_list_store_insert_with_values( GTK_LIST_STORE( model ), &it, pos,
                                       COL_NAME,
                                       vfs_volume_get_disp_name( vol ),
                                       COL_PATH,
                                       mnt,
                                       COL_DATA, vol, -1 );
    if( set_icon )
    {
        icon_theme = gtk_icon_theme_get_default();
        icon = gtk_icon_theme_load_icon ( icon_theme,
                                          vfs_volume_get_icon( vol ),
                                          20, 0, NULL );
        gtk_list_store_set( GTK_LIST_STORE( model ), &it, COL_ICON, icon, -1 );
        if ( icon )
            gdk_pixbuf_unref( icon );
    }
    ++n_vols;
}

void remove_volume( VFSVolume* vol )
{
    GtkTreeIter it;
    VFSVolume* v = NULL;

    gtk_tree_model_get_iter_first( model, &it );
    do
    {
        gtk_tree_model_get( model, &it, COL_DATA, &v, -1 );
    }
    while ( v != vol && gtk_tree_model_iter_next( model, &it ) );
    if ( v != vol )
        return ;
    gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
    --n_vols;
}

void update_volume( VFSVolume* vol )
{
    GtkIconTheme * icon_theme;
    GdkPixbuf* icon;
    GtkTreeIter it;
    VFSVolume* v = NULL;

    gtk_tree_model_get_iter_first( model, &it );
    do
    {
        gtk_tree_model_get( model, &it, COL_DATA, &v, -1 );
    }
    while ( v != vol && gtk_tree_model_iter_next( model, &it ) );
    if ( v != vol )
        return ;

    icon_theme = gtk_icon_theme_get_default();
    icon = gtk_icon_theme_load_icon ( icon_theme,
                                      vfs_volume_get_icon( vol ),
                                      20, 0, NULL );
    gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                        COL_ICON,
                        icon,
                        COL_NAME,
                        vfs_volume_get_disp_name( vol ),
                        COL_PATH,
                        vfs_volume_get_mount_point( vol ), -1 );
    if ( icon )
        gdk_pixbuf_unref( icon );
}

void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                       GtkTreeViewColumn *col, gpointer user_data )
{
    int i;
    VFSVolume* vol;
    GtkTreeIter it;
    GtkWidget* toplevel;
    GdkCursor* cursor;

#if 0
    i = gtk_tree_path_get_indices( tree_path ) [ 0 ];
    if ( i >= 2 && i < ( 2 + n_vols ) )   /* Volume */
    {
        if ( ! gtk_tree_model_get_iter( model, &it, tree_path ) )
            return ;
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );

        if ( ! vfs_volume_is_mounted( vol ) )
        {
            toplevel = gtk_widget_get_toplevel( GTK_WIDGET(view) );
            cursor = gdk_cursor_new_for_display( gdk_display_get_default(), GDK_WATCH );
            gdk_window_set_cursor( toplevel->window, cursor );
            gdk_cursor_unref( cursor );

            if ( ! vfs_volume_mount( vol ) )
            {
                ptk_show_error( GTK_WINDOW( toplevel ),
                                _( "Unable to mount device" ) );
            }
            gdk_window_set_cursor( toplevel->window, NULL );
            if( vfs_volume_get_mount_point( vol ) )
            {
                gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                    COL_PATH, vfs_volume_get_mount_point( vol ), -1 );
            }
        }
    }
#endif
}

static void on_mount( GtkMenuItem* item, VFSVolume* vol )
{
    if( ! vfs_volume_mount( vol ) )
        ptk_show_error( NULL, _("Unable to mount device") );
}

static void on_umount( GtkMenuItem* item, VFSVolume* vol )
{
    if( ! vfs_volume_umount( vol ) )
        ptk_show_error( NULL, _("Unable to unmount device") );
}

static void on_eject( GtkMenuItem* item, VFSVolume* vol )
{
    if( vfs_volume_is_mounted( vol ) )
        on_umount( item, vol );
    if( ! vfs_volume_eject( vol ) )
        ptk_show_error( NULL, _("Unable to eject device") );
}

gboolean ptk_location_view_is_item_bookmark( GtkTreeView* location_view,
                                             GtkTreeIter* it )
{
    int pos;
    GtkTreePath* tree_path;

    tree_path = gtk_tree_model_get_path( model, it );
    if( ! tree_path )
        return FALSE;

    pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
    gtk_tree_path_free( tree_path );

    return (pos > ( 2 + n_vols ));
}


gboolean on_button_press_event( GtkTreeView* view, GdkEventButton* evt,
                                gpointer user_data )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    int pos;
    GtkMenu* popup;
    GtkWidget* item;
    VFSVolume* vol;

    if( evt->button != 2 && evt->button != 3 )
        return FALSE;

    gtk_tree_view_get_path_at_pos( view, evt->x, evt->y, &tree_path, NULL, NULL, NULL );
    if( !tree_path )
        return FALSE;
    tree_sel = gtk_tree_view_get_selection( view );
    gtk_tree_model_get_iter( model, &it, tree_path );
    pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
    gtk_tree_path_free( tree_path );
    gtk_tree_selection_select_iter( tree_sel, &it );

    if ( evt->button == 3 )
    {
        if ( pos >= 2 )
        {
            if( pos < ( 2 + n_vols ) )  /* volume */
            {
                popup = GTK_MENU(gtk_menu_new());
                gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );

                item = gtk_menu_item_new_with_mnemonic( _( "_Mount File System" ) );
                g_signal_connect( item, "activate", G_CALLBACK(on_mount), vol );
                if( vfs_volume_is_mounted( vol ) )
                    gtk_widget_set_sensitive( item, FALSE );

                gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

                if( vfs_volume_requires_eject( vol ) )
                {
                    item = gtk_menu_item_new_with_mnemonic( _( "_Eject" ) );
                    g_signal_connect( item, "activate", G_CALLBACK(on_eject), vol );
                }
                else
                {
                    item = gtk_menu_item_new_with_mnemonic( _( "_Unmount File System" ) );
                    g_signal_connect( item, "activate", G_CALLBACK(on_umount), vol );
                }
                if( ! vfs_volume_is_mounted( vol ) )
                    gtk_widget_set_sensitive( item, FALSE );
                gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

                gtk_widget_show_all( GTK_WIDGET(popup) );
                g_signal_connect( popup, "selection-done",
                                  G_CALLBACK( gtk_widget_destroy ), NULL );
                gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
            }
        }
    }

    return FALSE;
}

void ptk_location_view_rename_selected_bookmark( GtkTreeView* location_view )
{
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeSelection* tree_sel;
    int pos;
    GtkTreeViewColumn* col;
    char *name, *path;
    GList *l, *renderers;

    tree_sel = gtk_tree_view_get_selection( location_view );
    if( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        tree_path = gtk_tree_model_get_path( model, &it );
        pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
        if( pos > (2 + n_vols) )
        {
            col = gtk_tree_view_get_column( location_view, 0 );
            renderers = gtk_tree_view_column_get_cell_renderers( col );
            for( l = renderers; l; l = l->next )
            {
                if( GTK_IS_CELL_RENDERER_TEXT(l->data) )
                {
                    g_object_set( G_OBJECT(l->data), "editable", TRUE, NULL );
                    gtk_tree_view_set_cursor_on_cell( location_view, tree_path,
                                                      col,
                                                      GTK_CELL_RENDERER( l->data ),
                                                      TRUE );
                    g_object_set( G_OBJECT(l->data), "editable", FALSE, NULL );
                    break;
                }
            }
            g_list_free( renderers );
        }
        gtk_tree_path_free( tree_path );
    }
}


gboolean ptk_location_view_is_item_volume(  GtkTreeView* location_view, GtkTreeIter* it )
{
    int pos;
    GtkTreePath* tree_path;

    if( n_vols == 0 )
        return FALSE;

    tree_path = gtk_tree_model_get_path( model, it );
    if( ! tree_path )
        return FALSE;

    pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
    gtk_tree_path_free( tree_path );

    return ( pos > 2 && pos < ( 2 + n_vols ));
}

VFSVolume* ptk_location_view_get_volume(  GtkTreeView* location_view, GtkTreeIter* it )
{
    VFSVolume* vol = NULL;
    gtk_tree_model_get( model, it, COL_DATA, &vol, -1 );
    return vol;
}
