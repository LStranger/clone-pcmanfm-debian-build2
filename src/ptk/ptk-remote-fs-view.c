/*
*  C Implementation: PtkRemoteFSView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "ptk-remote-fs-view.h"
#include <stdio.h>
#include <string.h>

#include "ptk-utils.h"
#include "ptk-remote-volume.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "vfs-dir.h"
#include "vfs-utils.h" /* for vfs_load_icon */
#include "vfs-remote-fs-mgr.h"

static GtkTreeModel* model = NULL;
VFSRemoteFSMgr* mgr = NULL;

static guint icon_size = 20;    /* FIXME: icon size should not be hard coded */
static guint theme_changed = 0; /* GtkIconTheme::"changed" handler */

static void ptk_remote_fs_view_init_model( GtkListStore* list );

static void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                              GtkTreeViewColumn *col, gpointer user_data );
static gboolean on_button_press_event( GtkTreeView* view, GdkEventButton* evt,
                                       gpointer user_data );

static void on_mount( GtkMenuItem* item, VFSRemoteVolume* vol );

static void on_umount( GtkMenuItem* item, VFSRemoteVolume* vol );

enum {
    COL_ICON = 0,
    COL_NAME,
    COL_DATA,
    N_COLS
};

#if 0
/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = { {"text/uri-list", 0 , 0 } };
#endif

static void show_busy( GtkWidget* view )
{
    GtkWidget* toplevel;
    GdkCursor* cursor;

    toplevel = gtk_widget_get_toplevel( GTK_WIDGET(view) );
    cursor = gdk_cursor_new_for_display( gtk_widget_get_display(GTK_WIDGET( view )), GDK_WATCH );
    gdk_window_set_cursor( toplevel->window, cursor );
    gdk_cursor_unref( cursor );

    /* update the  GUI */
    while (gtk_events_pending ())
        gtk_main_iteration ();
}

static void show_ready( GtkWidget* view )
{
    GtkWidget* toplevel;
    toplevel = gtk_widget_get_toplevel( GTK_WIDGET(view) );
    gdk_window_set_cursor( toplevel->window, NULL );
}

static void on_model_destroy( gpointer data, GObject* object )
{
    GtkIconTheme* icon_theme;

    g_signal_handlers_disconnect_matched( mgr, G_SIGNAL_MATCH_DATA,
                    0, 0, NULL, NULL, model );

    model = NULL;

    g_object_unref( mgr );
    mgr = NULL;

    icon_theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect( icon_theme, theme_changed );
}

static void update_icons()
{
    GtkIconTheme* icon_theme;
    GtkTreeIter it;
    GdkPixbuf* icon;
    int i;

    GtkListStore* list = GTK_LIST_STORE( model );
    icon_theme = gtk_icon_theme_get_default();
    icon = vfs_load_icon ( icon_theme, "gnome-fs-share", icon_size );

    if( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do{
            gtk_list_store_set( list, &it, COL_ICON, icon, -1 );
        }while( gtk_tree_model_iter_next( model, &it ) );
    }

    g_object_unref( icon );
}

static void add_volume( GtkListStore* list, VFSRemoteVolume* vol )
{
    GtkTreeIter it;
    gtk_list_store_append( list, &it );
    gtk_list_store_set( list, &it,
                        COL_NAME, vol->name ? vol->name : vol->host,
                        COL_DATA, vfs_remote_volume_ref(vol), -1 );
}

static void remove_volume( GtkListStore* list, VFSRemoteVolume* vol )
{
    GtkTreeIter it;
    if( gtk_tree_model_get_iter_first( (GtkTreeModel*)list, &it ) )
    {
        VFSRemoteVolume* vol2;
        do{
            gtk_tree_model_get( (GtkTreeModel*)list, COL_DATA, &vol2, -1 );
            if( vol2 == vol )
            {
                gtk_list_store_remove( list, &it );
                vfs_remote_volume_unref( vol2 );
                break;
            }
        }while( gtk_tree_model_iter_next( (GtkTreeModel*)list, &it ) );
    }
}

static void update_volume()
{

}

void ptk_remote_fs_view_init_model( GtkListStore* list )
{
    const GList* l;
    for( l = mgr->vols; l; l = l->next )
    {
        VFSRemoteVolume* vol = (VFSRemoteVolume*)l->data;
        add_volume( list, vol );
    }
    update_icons();
}

static void on_item_edited( GtkCellRendererText *cell,
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
        VFSRemoteVolume* vol;
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1);
        g_free( vol->name );
        vol->name = g_strdup( new_text );
        /* FIXME: rename mount point! */
        vfs_remote_fs_mgr_change_volume( mgr, vol );
    }
}

static void on_vol_added( VFSRemoteFSMgr* _mgr, VFSRemoteVolume* vol, GtkListStore* list )
{
    GdkPixbuf* icon;
    GtkTreeIter smb_it, it;
    int n = gtk_tree_model_iter_n_children( (GtkTreeModel*)list, NULL );
    gtk_tree_model_iter_nth_child( (GtkTreeModel*)list, &smb_it, NULL, n - 1 );
    gtk_tree_model_get( (GtkTreeModel*)list, &smb_it, COL_ICON, &icon, -1 );
    gtk_list_store_insert_before( list, &it, &smb_it );
    gtk_list_store_set( list, &it,
                        COL_NAME, vol->name ? vol->name : vol->host,
                        COL_ICON, icon,
                        COL_DATA, vfs_remote_volume_ref(vol), -1 );
    g_object_unref( icon );
}

static void on_vol_removed( VFSRemoteFSMgr* _mgr, VFSRemoteVolume* vol, GtkListStore* list )
{
    GtkTreeIter it;
    gtk_tree_model_get_iter_first( (GtkTreeModel*)list, &it );
    do{
        VFSRemoteVolume* _vol;
        gtk_tree_model_get( (GtkTreeModel*)list, &it, COL_DATA, &_vol, -1 );
        if( _vol == vol )
        {
            gtk_list_store_remove( list, &it );
            break;
        }
    }while( gtk_tree_model_iter_next( (GtkTreeModel*)list, &it ));
}

static void on_vol_changed( VFSRemoteFSMgr* _mgr, VFSRemoteVolume* vol, GtkListStore* list )
{
    GtkTreeIter it;
    gtk_tree_model_get_iter_first( (GtkTreeModel*)list, &it );
    do{
        VFSRemoteVolume* _vol;
        gtk_tree_model_get( (GtkTreeModel*)list, &it, COL_DATA, &_vol, -1 );
        if( _vol == vol )
        {
            gtk_list_store_set( list, &it, COL_NAME, vol->name, -1 );
            break;
        }
    }while( gtk_tree_model_iter_next( (GtkTreeModel*)list, &it ));
}

GtkWidget* ptk_remote_fs_view_new()
{
    GtkTreeView * view;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkListStore* list;
    GtkIconTheme* icon_theme;

    if ( ! model )
    {
        mgr = vfs_remote_fs_mgr_get();

        list = gtk_list_store_new( N_COLS,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_POINTER );
        g_object_weak_ref( G_OBJECT( list ), on_model_destroy, NULL );
        model = ( GtkTreeModel* ) list;

        g_signal_connect( mgr, "add", G_CALLBACK(on_vol_added), model );
        g_signal_connect( mgr, "remove", G_CALLBACK(on_vol_removed), model );
        g_signal_connect( mgr, "change", G_CALLBACK(on_vol_changed), model );

        ptk_remote_fs_view_init_model( list );
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
/*  FIXME: This should be enabled in future releases.
    gtk_tree_view_enable_model_drag_dest (
        GTK_TREE_VIEW( view ),
        drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_LINK );
*/
    gtk_tree_view_set_headers_visible( view, FALSE );

    col = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "pixbuf", COL_ICON, NULL );

    renderer = gtk_cell_renderer_text_new();
    g_signal_connect( renderer, "edited", G_CALLBACK(on_item_edited), view );
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", COL_NAME, NULL );

    gtk_tree_view_append_column ( view, col );

    g_signal_connect( view, "row-activated", G_CALLBACK( on_row_activated ), NULL );

    g_signal_connect( view, "button-press-event", G_CALLBACK( on_button_press_event ), NULL );

    return GTK_WIDGET( view );
}

gboolean ptk_remote_fs_view_chdir( GtkTreeView* remote_fs_view, const char* path )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    if ( !path )
        return FALSE;

    tree_sel = gtk_tree_view_get_selection( remote_fs_view );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        VFSRemoteVolume* vol;
        do
        {
            gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );
            if ( vol && g_str_has_prefix( path, vfs_remote_volume_get_mount_point(vol) ) )
            {
                gtk_tree_selection_select_iter( tree_sel, &it );
                return TRUE;
            }
        }
        while ( gtk_tree_model_iter_next ( model, &it ) );
    }
    gtk_tree_selection_unselect_all ( tree_sel );
    return FALSE;
}

#if 0
static gboolean try_mount( GtkTreeView* view, VFSRemoteVolume* vol )
{
    GError* err = NULL;
    GtkWidget* toplevel = gtk_widget_get_toplevel( GTK_WIDGET(view) );
    gboolean ret = TRUE;

    show_busy( (GtkWidget*)view );

    if ( ! vfs_volume_mount( vol, &err ) )
    {
        ptk_show_error( GTK_WINDOW( toplevel ),
                        _( "Unable to mount device" ),
                        err->message);
        g_error_free( err );
        ret = FALSE;
    }

    /* Run main loop to process HAL events or volume info won't get updated correctly. */
    while(gtk_events_pending () )
        gtk_main_iteration ();

    show_ready( GTK_WIDGET(view) );
    return ret;
}
#endif

char* ptk_remote_fs_view_get_selected_dir( GtkTreeView* remote_fs_view )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    char* real_path = NULL;
    int i;

    tree_sel = gtk_tree_view_get_selection( remote_fs_view );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        VFSRemoteVolume* vol;
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );
        return g_strdup( vfs_remote_volume_get_mount_point(vol) );
    }
    return NULL;
}

#if 0
void add_volume( VFSRemoteVolume* vol, gboolean set_icon )
{
    GtkIconTheme * icon_theme;
    GdkPixbuf* icon;
    GtkTreeIter it;
    int pos;
    const char* mnt;

    if ( vfs_volume_is_removable( vol ) )
        pos = first_vol_idx;
    else
        pos = sep_idx;

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
        icon = vfs_load_icon ( icon_theme,
                                          vfs_volume_get_icon( vol ),
                                          icon_size );
        gtk_list_store_set( GTK_LIST_STORE( model ), &it, COL_ICON, icon, -1 );
        if ( icon )
            gdk_pixbuf_unref( icon );
    }
    ++n_vols;
    ++sep_idx;
}


void remove_volume( VFSRemoteVolume* vol )
{
    GtkTreeIter it;
    VFSRemoteVolume* v = NULL;

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
    --sep_idx;
}


void update_volume( VFSRemoteVolume* vol )
{
    GtkIconTheme * icon_theme;
    GdkPixbuf* icon;
    GtkTreeIter it;
    VFSRemoteVolume* v = NULL;

    gtk_tree_model_get_iter_first( model, &it );
    do
    {
        gtk_tree_model_get( model, &it, COL_DATA, &v, -1 );
    }
    while ( v != vol && gtk_tree_model_iter_next( model, &it ) );
    if ( v != vol )
        return ;

    icon_theme = gtk_icon_theme_get_default();
    icon = vfs_load_icon ( icon_theme,
                                      vfs_volume_get_icon( vol ),
                                      icon_size );
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
#endif

static void do_mount( GtkTreeView* view, VFSRemoteVolume* vol )
{
    GError* err = NULL;
    GtkWidget* parent = gtk_widget_get_toplevel( (GtkWidget*)view );

    gtk_widget_set_sensitive( parent, FALSE );
    show_busy( view );
    if( ! vfs_remote_volume_mount( vol, &err ) )
    {
        gtk_widget_set_sensitive( parent, TRUE );
        show_ready( view );
        ptk_show_error( NULL, _("Unable to connect"), err->message );
        g_error_free( err );
    }
    else
    {
        show_ready( view );
        gtk_widget_set_sensitive( parent, TRUE );
    }
}

void on_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                       GtkTreeViewColumn *col, gpointer user_data )
{
    int i;
    GtkTreeIter it;
    GtkTreeModel* model = gtk_tree_view_get_model(view);
    if( gtk_tree_model_get_iter( model, &it, tree_path ) )
    {
        VFSRemoteVolume* vol = NULL;
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );
        if( ! vfs_remote_volume_is_mounted( vol ) )
            do_mount( view, vol );
    }
}

void on_mount( GtkMenuItem* item, VFSRemoteVolume* vol )
{
    GtkWidget* view = (GtkWidget*)g_object_get_data( G_OBJECT(item), "view" );
    do_mount( view, vol );
}

void on_umount( GtkMenuItem* item, VFSRemoteVolume* vol )
{
    GError* err = NULL;
    GtkWidget* view = (GtkWidget*)g_object_get_data( G_OBJECT(item), "view" );
    show_busy( view );
    if( ! vfs_remote_volume_unmount( vol, &err ) )
    {
        show_ready( view );
        ptk_show_error( NULL, _("Unable to disconnect"), err->message );
        g_error_free( err );
    }
    else
        show_ready( view );
}

static void on_add( GtkMenuItem* item, GtkTreeView* view )
{
    ptk_remote_volume_add_new();
}

static void on_remove( GtkMenuItem* item, VFSRemoteVolume* vol )
{
    GtkWidget* dlg = gtk_message_dialog_new_with_markup( NULL, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                _("Are you sure you want to remove this network drive?\n\n"
                                  "<b>Warning: Once deleted, it cannot be recovered.</b>") );
    gtk_window_set_title( (GtkWindow*)dlg, _("Confirm") );
    if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_YES )
        vfs_remote_fs_mgr_remove_volume( mgr, vol );
    gtk_widget_destroy( dlg );
}

static void on_edit( GtkMenuItem* item, VFSRemoteVolume* vol )
{
    ptk_remote_volume_edit( vol );
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

    if( evt->type != GDK_BUTTON_PRESS )
        return FALSE;

    gtk_tree_view_get_path_at_pos( view, evt->x, evt->y, &tree_path, NULL, NULL, NULL );
    if( !tree_path ) /* no item is clicked */
    {
        if( evt->button == 3 )    /* right button */
        {
            popup = GTK_MENU(gtk_menu_new());

            item = gtk_image_menu_item_new_from_stock( GTK_STOCK_ADD, NULL );
            g_signal_connect( item, "activate", G_CALLBACK(on_add), view );
            gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

            gtk_widget_show_all( GTK_WIDGET(popup) );
            g_signal_connect( popup, "selection-done",
                              G_CALLBACK( gtk_widget_destroy ), NULL );
            gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );

            return TRUE;
        }
        return FALSE;
    }
    tree_sel = gtk_tree_view_get_selection( view );
    gtk_tree_model_get_iter( model, &it, tree_path );
    pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
    gtk_tree_selection_select_iter( tree_sel, &it );

    if ( evt->button == 1 ) /* left button */
    {
        gtk_tree_view_row_activated( view, tree_path, NULL );
    }
    else if ( evt->button == 3 )    /* right button */
    {
        gboolean mounted;
        VFSRemoteVolume* vol;

        popup = GTK_MENU(gtk_menu_new());
        gtk_tree_model_get( model, &it, COL_DATA, &vol, -1 );

        mounted = vfs_remote_volume_is_mounted( vol );

        item = gtk_menu_item_new_with_mnemonic( _( "_Connect" ) );
        g_object_set_data( G_OBJECT(item), "view", view );
        g_signal_connect( item, "activate", G_CALLBACK(on_mount), vol );
        if( mounted )
            gtk_widget_set_sensitive( item, FALSE );

        gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

        item = gtk_menu_item_new_with_mnemonic( _( "_Disconnect" ) );
        g_object_set_data( G_OBJECT(item), "view", view );
        g_signal_connect( item, "activate", G_CALLBACK(on_umount), vol );
        if( ! mounted )
            gtk_widget_set_sensitive( item, FALSE );
        gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

        item = gtk_separator_menu_item_new();
        gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

        item = gtk_image_menu_item_new_from_stock( GTK_STOCK_ADD, NULL );
        g_signal_connect( item, "activate", G_CALLBACK(on_add), view );
        gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

        if( strcmp( vol->type, "smb" ) ) /* smb is built-in and cannot be removed */
        {
            item = gtk_image_menu_item_new_from_stock( GTK_STOCK_EDIT, NULL );
            g_signal_connect( item, "activate", G_CALLBACK(on_edit), vol );
            gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

            item = gtk_separator_menu_item_new();
            gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );

            item = gtk_image_menu_item_new_from_stock( GTK_STOCK_REMOVE, NULL );
            g_signal_connect( item, "activate", G_CALLBACK(on_remove), vol );
            gtk_menu_shell_append( GTK_MENU_SHELL( popup ), item );
        }

        gtk_widget_show_all( GTK_WIDGET(popup) );
        g_signal_connect( popup, "selection-done",
                          G_CALLBACK( gtk_widget_destroy ), NULL );
        gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
    }
    gtk_tree_path_free( tree_path );

    return FALSE;
}

void ptk_remote_fs_view_rename_selected_item( GtkTreeView* remote_fs_view )
{
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeSelection* tree_sel;
    int pos;
    GtkTreeViewColumn* col;
    GList *l, *renderers;
#if 0
    tree_sel = gtk_tree_view_get_selection( remote_fs_view );
    if( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        tree_path = gtk_tree_model_get_path( model, &it );
        pos = gtk_tree_path_get_indices( tree_path ) [ 0 ];
        if( pos > sep_idx )
        {
            col = gtk_tree_view_get_column( remote_fs_view, 0 );
            renderers = gtk_tree_view_column_get_cell_renderers( col );
            for( l = renderers; l; l = l->next )
            {
                if( GTK_IS_CELL_RENDERER_TEXT(l->data) )
                {
                    g_object_set( G_OBJECT(l->data), "editable", TRUE, NULL );
                    gtk_tree_view_set_cursor_on_cell( remote_fs_view, tree_path,
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
#endif
}

/*
VFSRemoteFS* ptk_remote_fs_view_get_item(  GtkTreeView* remote_fs_view, GtkTreeIter* it )
{
    VFSRemoteFS* data = NULL;
    gtk_tree_model_get( model, it, COL_DATA, &data, -1 );
    return data;
}
*/
