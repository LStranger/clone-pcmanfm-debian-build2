#include "ptk-file-browser.h"
#include <gtk/gtk.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

/* MIME type processing library from FreeDesktop.org */
#include "xdgmime.h"

#include "file-properties.h"
#include "app-chooser-dialog.h"

#include "ptk-file-icon-renderer.h"
#include "ptk-utils.h"
#include "ptk-input-dialog.h"
#include "ptk-file-task.h"

#include "ptk-location-view.h"
#include "ptk-dir-tree-view.h"

#include "settings.h"

#include "vfs-dir.h"
#include "vfs-file-info.h"
#include "vfs-file-monitor.h"
#include "vfs-app-desktop.h"
#include "ptk-file-list.h"
#include "ptk-text-renderer.h"

#include "ptk-file-archiver.h"
#include "ptk-clipboard.h"

/* If set to FALSE, all selection changes in folder_view are prevented. */
static gboolean can_folder_view_sel_change = TRUE;

static void ptk_file_browser_class_init( PtkFileBrowserClass* klass );
static void ptk_file_browser_init( PtkFileBrowser* file_browser );
static void ptk_file_browser_finalize( GObject *obj );
static void ptk_file_browser_get_property ( GObject *obj,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec );
static void ptk_file_browser_set_property ( GObject *obj,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec );

/* Utility functions */
static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFileBrowserViewMode view_mode );

static void init_list_view( GtkTreeView* list_view );

static GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser );

static GtkTreeView* ptk_file_browser_create_location_view( PtkFileBrowser* file_browser );

static GList* folder_view_get_selected_items( PtkFileBrowser* file_browser,
                                              GtkTreeModel** model );

static void ptk_file_browser_chdir_state_cb( VFSDir* dir,
                                             int state,
                                             PtkFileBrowser* file_browser );

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                    const char* app_desktop );

static GtkWidget*
ptk_file_browser_create_popup_menu_for_file( PtkFileBrowser* file_browser,
                                             const char* file_path,
                                             VFSFileInfo* info );

static void
ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser,
                              gboolean copy );

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y );

/* sort functions of folder view */
static gint sort_files_by_name ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );

static gint sort_files_by_size ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );

static gint sort_files_by_time ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );


static gboolean filter_files ( GtkTreeModel *model,
                               GtkTreeIter *it,
                               gpointer user_data );

/* sort functions of dir tree */
static gint sort_dir_tree_by_name ( GtkTreeModel *model,
                                    GtkTreeIter *a,
                                    GtkTreeIter *b,
                                    gpointer user_data );

/* signal handlers */
static void
on_folder_view_item_activated ( PtkIconView *iconview,
                                GtkTreePath *path,
                                PtkFileBrowser* file_browser );
static void
on_folder_view_row_activated ( GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GtkTreeViewColumn* col,
                               PtkFileBrowser* file_browser );
static void
on_folder_view_item_sel_change ( PtkIconView *iconview,
                                 PtkFileBrowser* file_browser );
static gboolean
on_folder_view_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event,
                                 PtkFileBrowser* file_browser );
static gboolean
on_folder_view_button_press_event ( GtkWidget *widget,
                                    GdkEventButton *event,
                                    PtkFileBrowser* file_browser );
static gboolean
on_folder_view_button_release_event ( GtkWidget *widget,
                                      GdkEventButton *event,
                                      PtkFileBrowser* file_browser );
static void
on_folder_view_scroll_scrolled ( GtkAdjustment *adjust,
                                 PtkFileBrowser* file_browser );
static void
on_dir_tree_sel_changed ( GtkTreeSelection *treesel,
                          PtkFileBrowser* file_browser );
static void
on_location_view_row_activated ( GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column,
                                 PtkFileBrowser* file_browser );

static gboolean
on_location_view_button_press_event ( GtkTreeView* view,
                                      GdkEventButton* evt,
                                      PtkFileBrowser* file_browser );

/* Drag & Drop */

static void
on_folder_view_drag_data_received ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    gpointer user_data );

static void
on_folder_view_drag_data_get ( GtkWidget *widget,
                               GdkDragContext *drag_context,
                               GtkSelectionData *sel_data,
                               guint info,
                               guint time,
                               PtkFileBrowser *file_browser );

static void
on_folder_view_drag_begin ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            gpointer user_data );

static gboolean
on_folder_view_drag_motion ( GtkWidget *widget,
                             GdkDragContext *drag_context,
                             gint x,
                             gint y,
                             guint time,
                             PtkFileBrowser* file_browser );

static gboolean
on_folder_view_drag_leave ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            guint time,
                            PtkFileBrowser* file_browser );

static gboolean
on_folder_view_drag_drop ( GtkWidget *widget,
                           GdkDragContext *drag_context,
                           gint x,
                           gint y,
                           guint time,
                           PtkFileBrowser* file_browser );

static void
on_folder_view_drag_end ( GtkWidget *widget,
                          GdkDragContext *drag_context,
                          gpointer user_data );


/* Signal handlers for popup menu */
static void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         gpointer user_data );
static void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data );
static void
on_file_properties_activate ( GtkMenuItem *menuitem,
                              gpointer user_data );
static void
on_popup_run_app ( GtkMenuItem *menuitem,
                   PtkFileBrowser* file_browser );
static void
on_popup_open_in_new_tab_activate ( GtkMenuItem *menuitem,
                                    PtkFileBrowser* file_browser );
static void
on_popup_open_in_new_win_activate ( GtkMenuItem *menuitem,
                                    PtkFileBrowser* file_browser );
static void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        gpointer user_data );
static void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         gpointer user_data );
static void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          gpointer user_data );
static void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           gpointer user_data );
static void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           gpointer user_data );
static void
on_popup_compress_activate ( GtkMenuItem *menuitem,
                             gpointer user_data );
static void
on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
static void
on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data );
static void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data );


/* Default signal handlers */
static void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                           const char* path,
                                           gboolean* cancel );
static void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser );
static void ptk_file_browser_content_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_sel_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action );
static void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser );

static int
file_list_order_from_sort_order( FileBrowserSortOrder order );

static GtkPanedClass *parent_class = NULL;

enum {
    BEFORE_CHDIR_SIGNAL,
    AFTER_CHDIR_SIGNAL,
    OPEN_ITEM_SIGNAL,
    CONTENT_CHANGE_SIGNAL,
    SEL_CHANGE_SIGNAL,
    PANE_MODE_CHANGE_SIGNAL,
    N_SIGNALS
};

static guint signals[ N_SIGNALS ] = { 0 };

static guint folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = 0;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

GType ptk_file_browser_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( PtkFileBrowserClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) ptk_file_browser_class_init,
                NULL,
                NULL,
                sizeof ( PtkFileBrowser ),
                0,
                ( GInstanceInitFunc ) ptk_file_browser_init,
                NULL,
            };
        type = g_type_register_static ( GTK_TYPE_HPANED, "PtkFileBrowser", &info, 0 );
    }
    return type;
}

void ptk_file_browser_class_init( PtkFileBrowserClass* klass )
{
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;

    /* before-chdir is emitted when PtkFileBrowser is about to change
    * its working directory. The 1st param is the path of the dir (in UTF-8),
    * and the 2nd param is a gboolean*, which can be filled by the
    * signal handler with TRUE to cancel the operation.
    */
    signals[ BEFORE_CHDIR_SIGNAL ] =
        g_signal_new ( "before-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, before_chdir ),
                       NULL, NULL,
                       gtk_marshal_VOID__POINTER_POINTER,
                       G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER );

    signals[ AFTER_CHDIR_SIGNAL ] =
        g_signal_new ( "after-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, after_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    /*
    * This signal is sent when a directory is about to be opened
    * arg1 is the path to be opened, and arg2 is the type of action,
    * ex: open in tab, open in terminal...etc.
    */
    signals[ OPEN_ITEM_SIGNAL ] =
        g_signal_new ( "open-item",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, open_item ),
                       NULL, NULL,
                       gtk_marshal_VOID__POINTER_INT, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT );

    signals[ CONTENT_CHANGE_SIGNAL ] =
        g_signal_new ( "content-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, content_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ SEL_CHANGE_SIGNAL ] =
        g_signal_new ( "sel-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, sel_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ PANE_MODE_CHANGE_SIGNAL ] =
        g_signal_new ( "pane-mode-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, pane_mode_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

}

void ptk_file_browser_init( PtkFileBrowser* file_browser )
{
    file_browser->folder_view_scroll = gtk_scrolled_window_new ( NULL, NULL );
    gtk_paned_pack2 ( GTK_PANED ( file_browser ),
                      file_browser->folder_view_scroll, TRUE, TRUE );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ),
                                          GTK_SHADOW_IN );
}

void ptk_file_browser_finalize( GObject *obj )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( obj );
    if ( file_browser->dir )
    {
        vfs_dir_remove_state_callback( file_browser->dir,
                                       ptk_file_browser_chdir_state_cb,
                                       file_browser );
        vfs_dir_unref( file_browser->dir );
    }

    /* Remove all idle handlers which are not called yet. */
    g_source_remove_by_user_data( file_browser );

    if ( file_browser->file_list )
        g_object_unref( G_OBJECT( file_browser->file_list ) );

    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void ptk_file_browser_get_property ( GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec )
{}

void ptk_file_browser_set_property ( GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec )
{}


/* File Browser API */

GtkWidget* ptk_file_browser_new( GtkWidget* mainWindow,
                                 PtkFileBrowserViewMode view_mode )
{
    PtkFileBrowser * file_browser;
    file_browser = ( PtkFileBrowser* ) g_object_new( PTK_TYPE_FILE_BROWSER, NULL );

    file_browser->folder_view = create_folder_view( file_browser, view_mode );
    file_browser->view_mode = view_mode;

    gtk_container_add ( GTK_CONTAINER ( file_browser->folder_view_scroll ),
                        file_browser->folder_view );

    gtk_widget_show_all( file_browser->folder_view_scroll );
    return ( GtkWidget* ) file_browser;
}

static gboolean side_pane_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path )
{
    if ( file_browser->side_pane_mode == FB_SIDE_PANE_BOOKMARKS )
    {
        ptk_location_view_chdir( file_browser->side_view, folder_path );
        return TRUE;
    }
    else if ( file_browser->side_pane_mode == FB_SIDE_PANE_DIR_TREE )
    {
        return ptk_dir_tree_view_chdir( file_browser->side_view, folder_path );
    }
}


gboolean ptk_file_browser_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path,
                                 gboolean add_history )
{
    gboolean cancel = FALSE;
    GtkWidget* folder_view = file_browser->folder_view;
    GtkTreeView* dir_tree = file_browser->side_view;
    GtkEntry* addressBar;
    GtkStatusbar* statusBar;

    char* path_end;
    int ctxid = 0;
    char *cwd;
    char* path;
    gboolean is_current;

    /*
    * FIXME:
    * Do nothing when there is unfinished task running in the
    * working thread.
    * This should be fixed with a better way in the future.
    */
    if ( file_browser->busy || !folder_path )
        return ;

    if ( folder_path )
    {
        path = strdup( folder_path );
        /* remove redundent '/' */
        if ( strcmp( path, "/" ) )
        {
            path_end = path + strlen( path ) - 1;
            for ( ; path_end > path; --path_end )
            {
                if ( *path_end != '/' )
                    break;
                else
                    *path_end = '\0';
            }
        }
    }
    else
        path = NULL;

    if ( ! path || ! g_file_test( path, ( G_FILE_TEST_IS_DIR ) ) )
    {
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                        _( "Directory does'nt exist!" ) );
        if ( path )
            g_free( path );
        return FALSE;
    }
    g_signal_emit( file_browser, signals[ BEFORE_CHDIR_SIGNAL ], 0, path, &cancel );

    if ( add_history )
    {
        if ( ! file_browser->curHistory || strcmp( file_browser->curHistory->data, path ) )
        {
            /* Has forward history */
            if ( file_browser->curHistory && file_browser->curHistory->next )
            {
                g_list_foreach ( file_browser->curHistory->next, ( GFunc ) g_free, NULL );
                g_list_free( file_browser->curHistory->next );
                file_browser->curHistory->next = NULL;
            }
            /* Add path to history if there is no forward history */
            file_browser->history = g_list_append( file_browser->history, path );
            file_browser->curHistory = g_list_last( file_browser->history );
        }
    }
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
        ptk_icon_view_set_model( PTK_ICON_VIEW( folder_view ), NULL );
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( folder_view ), NULL );

    if ( file_browser->dir )
    {
        vfs_dir_remove_state_callback( file_browser->dir,
                                       ptk_file_browser_chdir_state_cb,
                                       file_browser );
        vfs_dir_unref( file_browser->dir );
    }

    file_browser->dir = vfs_get_dir( path );
    vfs_dir_add_state_callback( file_browser->dir,
                                ptk_file_browser_chdir_state_cb,
                                file_browser );

    file_browser->busy = TRUE;
    return TRUE;
}

static gboolean
ptk_file_browser_delayed_content_change( PtkFileBrowser* file_browser )
{
    GTimeVal t;
    g_get_current_time( &t );
    file_browser->prev_update_time = t.tv_sec;
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    file_browser->update_timeout = 0;
    return FALSE;
}

#if 0
void on_folder_content_update ( FolderContent* content,
                                PtkFileBrowser* file_browser )
{
    /*  FIXME: Newly added or deleted files should not be delayed.
        This must be fixed before 0.2.0 release.  */
    GTimeVal t;
    g_get_current_time( &t );
    /*
      Previous update is < 5 seconds before.
      Queue the update, and don't update the view too often
    */
    if ( ( t.tv_sec - file_browser->prev_update_time ) < 5 )
    {
        /*
          If the update timeout has been set, wait until the timeout happens, and
          don't do anything here.
        */
        if ( 0 == file_browser->update_timeout )
        { /* No timeout callback. Add one */
            /* Delay the update */
            file_browser->update_timeout = g_timeout_add( 5000,
                                                          ( GSourceFunc ) ptk_file_browser_delayed_content_change, file_browser );
        }
    }
    else if ( 0 == file_browser->update_timeout )
    { /* No timeout callback. Add one */
        file_browser->prev_update_time = t.tv_sec;
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    }
}
#endif

static gboolean ptk_file_browser_content_changed( PtkFileBrowser* file_browser )
{
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    return FALSE;
}

static void on_folder_content_changed( VFSDir* dir, VFSFileInfo* file,
                                       PtkFileBrowser* file_browser )
{
    g_idle_add( ( GSourceFunc ) ptk_file_browser_content_changed,
                file_browser );
}

static void on_sort_col_changed( GtkTreeSortable* sortable,
                                 PtkFileBrowser* file_browser )
{
    int col;

    gtk_tree_sortable_get_sort_column_id( sortable,
                                          &col, &file_browser->sort_type );

    switch ( col )
    {
    case COL_FILE_NAME:
        col = FB_SORT_BY_NAME;
        break;
    case COL_FILE_SIZE:
        col = FB_SORT_BY_SIZE;
        break;
    case COL_FILE_MTIME:
        col = FB_SORT_BY_MTIME;
        break;
    case COL_FILE_DESC:
        col = FB_SORT_BY_TYPE;
        break;
    case COL_FILE_PERM:
        col = FB_SORT_BY_PERM;
        break;
    case COL_FILE_OWNER:
        col = FB_SORT_BY_OWNER;
        break;
    }
    file_browser->sort_order = col;
}

static void ptk_file_browser_update_model( PtkFileBrowser* file_browser )
{
    PtkFileList * list, *old_list;

    list = ptk_file_list_new( file_browser->dir,
                              file_browser->show_hidden_files );
    old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL( list );
    if ( old_list )
        g_object_unref( G_OBJECT( old_list ) );

    gtk_tree_sortable_set_sort_column_id(
        GTK_TREE_SORTABLE( list ),
        file_list_order_from_sort_order( file_browser->sort_order ),
        file_browser->sort_type );

    ptk_file_list_show_thumbnails( list,
                                   ( file_browser->view_mode == FBVM_ICON_VIEW ),
                                   file_browser->max_thumbnail );
    g_signal_connect( list, "sort-column-changed",
                      G_CALLBACK( on_sort_col_changed ), file_browser );

    if ( file_browser->view_mode == FBVM_ICON_VIEW )
        ptk_icon_view_set_model( PTK_ICON_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( list ) );
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( list ) );
}

void ptk_file_browser_chdir_state_cb( VFSDir* dir,
                                      int state,
                                      PtkFileBrowser* file_browser )
{
    PtkFileList * list, *old_list;

    if ( state == 1 )
    {
        g_signal_connect( dir, "file-created",
                          G_CALLBACK( on_folder_content_changed ), file_browser );
        g_signal_connect( dir, "file-deleted",
                          G_CALLBACK( on_folder_content_changed ), file_browser );
        g_signal_connect( dir, "file-changed",
                          G_CALLBACK( on_folder_content_changed ), file_browser );

        ptk_file_browser_update_model( file_browser );

        file_browser->busy = FALSE;

        g_signal_emit( file_browser, signals[ AFTER_CHDIR_SIGNAL ], NULL );
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], NULL );
        g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], NULL );

        if ( file_browser->side_pane )
        {
            side_pane_chdir( file_browser,
                             ptk_file_browser_get_cwd( file_browser ) );
        }
    }
    else if ( state == 2 )
    {
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], NULL );
        if ( file_browser->file_list )
        {
            ptk_file_list_show_thumbnails( file_browser->file_list,
                                           file_browser->view_mode == FBVM_ICON_VIEW,
                                           file_browser->max_thumbnail );
        }
    }
}


const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser )
{
    if ( ! file_browser->curHistory )
        return NULL;
    return ( const char* ) file_browser->curHistory->data;
}

gboolean ptk_file_browser_is_busy( PtkFileBrowser* file_browser )
{
    return file_browser->busy;
}

gboolean ptk_file_browser_can_back( PtkFileBrowser* file_browser )
{
    /* there is back history */
    return ( file_browser->curHistory && file_browser->curHistory->prev );
}

void ptk_file_browser_go_back( PtkFileBrowser* file_browser )
{
    const char * path;

    /*
    * FIXME:
    * Do nothing when there is unfinished task running in the
    * working thread.
    * This should be fixed with a better way in the future.
    */
    if ( file_browser->busy )
        return ;

    /* there is no back history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->prev )
        return ;
    path = ( const char* ) file_browser->curHistory->prev->data;
    if ( ptk_file_browser_chdir( file_browser, path, FALSE ) )
    {
        file_browser->curHistory = file_browser->curHistory->prev;
    }
}

gboolean ptk_file_browser_can_forward( PtkFileBrowser* file_browser )
{
    /* If there is forward history */
    return ( file_browser->curHistory && file_browser->curHistory->next );
}

void ptk_file_browser_go_forward( PtkFileBrowser* file_browser )
{
    const char * path;

    /*
    * FIXME:
    * Do nothing when there is unfinished task running in the
    * working thread.
    * This should be fixed with a better way in the future.
      */
    if ( file_browser->busy )
        return ;

    /* If there is no forward history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->next )
        return ;
    path = ( const char* ) file_browser->curHistory->next->data;
    if ( ptk_file_browser_chdir( file_browser, path, FALSE ) )
    {
        file_browser->curHistory = file_browser->curHistory->next;
    }
}

GtkWidget* ptk_file_browser_get_folder_view( PtkFileBrowser* file_browser )
{
    return file_browser->folder_view;
}

GtkTreeView* ptk_file_browser_get_dir_tree( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_select_all( PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_select_all( PTK_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_select_all( tree_sel );
    }
}

static gboolean
invert_selection ( GtkTreeModel* model,
                   GtkTreePath *path,
                   GtkTreeIter* it,
                   PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        if ( ptk_icon_view_path_is_selected ( PTK_ICON_VIEW( file_browser->folder_view ), path ) )
            ptk_icon_view_unselect_path ( PTK_ICON_VIEW( file_browser->folder_view ), path );
        else
            ptk_icon_view_select_path ( PTK_ICON_VIEW( file_browser->folder_view ), path );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        if ( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
            gtk_tree_selection_unselect_path ( tree_sel, path );
        else
            gtk_tree_selection_select_path ( tree_sel, path );
    }
    return FALSE;
}

void ptk_file_browser_invert_selection( PtkFileBrowser* file_browser )
{
    GtkTreeModel * model;
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        model = ptk_icon_view_get_model( PTK_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
    }
    gtk_tree_model_foreach ( model,
                             ( GtkTreeModelForeachFunc ) invert_selection, file_browser );
}

/* signal handlers */

void
on_folder_view_item_activated ( PtkIconView *iconview,
                                GtkTreePath *path,
                                PtkFileBrowser* file_browser )
{
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void
on_folder_view_row_activated ( GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GtkTreeViewColumn* col,
                               PtkFileBrowser* file_browser )
{
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void on_folder_view_item_sel_change ( PtkIconView *iconview,
                                      PtkFileBrowser* file_browser )
{
    GList * sel_files;
    GList* sel;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;
    VFSFileInfo* file;

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;

    sel_files = folder_view_get_selected_items( file_browser, &model );

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        if ( gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( file )
            {
                file_browser->sel_size += vfs_file_info_get_size( file );
                vfs_file_info_unref( file );
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach( sel_files,
                    ( GFunc ) gtk_tree_path_free,
                    NULL );
    g_list_free( sel_files );

    g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], NULL );
}


gboolean
on_folder_view_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event,
                                 PtkFileBrowser* file_browser )
{

    return FALSE;
}

gboolean
on_folder_view_button_release_event ( GtkWidget *widget,
                                      GdkEventButton *event,
                                      PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    GtkTreePath* tree_path;
    GList* sel_files;
    VFSFileInfo* file;
    char* file_path;

    /*
    FIXME: what's this?
    if ( file_browser->view_mode == FBVM_LIST_VIEW && !can_folder_view_sel_change )
    {
        can_folder_view_sel_change = TRUE;
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) );
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ),
                                       event->x, event->y, &tree_path, NULL, NULL, NULL );
        if ( tree_path )
        {
            if ( gtk_tree_selection_path_is_selected( tree_sel, tree_path ) )
            {
                gtk_tree_selection_unselect_all( tree_sel );
                gtk_tree_selection_select_path( tree_sel, tree_path );
            }
            gtk_tree_path_free( tree_path );
        }
    }
    */ 
    return FALSE;
}

gboolean
on_folder_view_button_press_event ( GtkWidget *widget,
                                    GdkEventButton *event,
                                    PtkFileBrowser* file_browser )
{
    VFSFileInfo * file;
    GtkTreeModel * model;
    GtkTreePath *tree_path;
    GtkTreeIter it;
    gchar *file_path;
    GtkTreeSelection* tree_sel;
    GtkWidget* popup = NULL;

    if ( event->type == GDK_BUTTON_PRESS )
    {
        if ( file_browser->view_mode == FBVM_ICON_VIEW )
        {
            tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW( widget ),
                                                       event->x, event->y );
            if ( !tree_path && event->button == 3 )
            {
                ptk_icon_view_unselect_all ( PTK_ICON_VIEW( widget ) );
            }
            model = ptk_icon_view_get_model( PTK_ICON_VIEW( widget ) );
        }
        else if ( file_browser->view_mode == FBVM_LIST_VIEW )
        {
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ),
                                           event->x, event->y, &tree_path, NULL, NULL, NULL );
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) );

            /* If click on a selected row */
            if ( tree_path
                    && event->type == GDK_BUTTON_PRESS
                    && !( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) )
                    && gtk_tree_selection_path_is_selected ( tree_sel, tree_path ) )
            {
                can_folder_view_sel_change = FALSE;
            }
            else
                can_folder_view_sel_change = TRUE;
        }

        /* an item is selected */
        if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            file_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
        }
        else /* no item is selected */
        {
            file = NULL;
            file_path = NULL;
        }

        /* middle button */
        if ( event->button == 2 && file_path )           /* middle click on a item */
        {
            if ( G_LIKELY( file_path ) )
            {
                if ( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
                {
                    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                                   file_path, PTK_OPEN_NEW_TAB );
                }
            }
        }
        else if ( event->button == 3 )                          /* right click */
        {
            /* cancel all selection, and select the item if it's not selected */
            if ( file_browser->view_mode == FBVM_ICON_VIEW )
            {
                if ( tree_path &&
                        !ptk_icon_view_path_is_selected ( PTK_ICON_VIEW( widget ),
                                                          tree_path ) )
                {
                    ptk_icon_view_unselect_all ( PTK_ICON_VIEW( widget ) );
                    ptk_icon_view_select_path( PTK_ICON_VIEW( widget ), tree_path );
                }
            }

            if ( ! file_path )
            {
                file_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
                file = vfs_file_info_new();
                vfs_file_info_get( file, file_path, NULL );
            }

            if ( g_file_test( file_path, G_FILE_TEST_EXISTS ) )
            {
                popup = ptk_file_browser_create_popup_menu_for_file( file_browser,
                                                                     file_path, file );
                gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                                NULL, NULL, 3, event->time );
            }
        }
        if ( file )
            vfs_file_info_unref( file );
        if ( file_path )
            g_free( file_path );
        gtk_tree_path_free( tree_path );
    }
    return FALSE;
}

static gboolean on_dir_tree_update_sel ( PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_dir_tree_view_get_selected_dir( file_browser->side_view );

    if ( dir_path )
    {
        if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
        {
            ptk_file_browser_chdir( file_browser, dir_path, TRUE );
        }
        g_free( dir_path );
    }
    return FALSE;
}

void
on_dir_tree_sel_changed ( GtkTreeSelection *treesel,
                          PtkFileBrowser* file_browser )
{
    g_idle_add( ( GSourceFunc ) on_dir_tree_update_sel, file_browser );
}

void
on_location_view_row_activated ( GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column,
                                 PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_view );
    if ( dir_path )
    {
        if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
        {
            ptk_file_browser_chdir( file_browser, dir_path, TRUE );
        }
        g_free( dir_path );
    }
}


static void on_shortcut_new_tab_activate( GtkMenuItem* item,
                                          PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_view );
    if ( dir_path )
    {
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, dir_path, PTK_OPEN_NEW_TAB );
        g_free( dir_path );
    }
}

static void on_shortcut_new_window_activate( GtkMenuItem* item,
                                             PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_view );
    if ( dir_path )
    {
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, dir_path, PTK_OPEN_NEW_WINDOW );
        g_free( dir_path );
    }
}

static void on_shortcut_remove_activate( GtkMenuItem* item,
                                         PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_view );
    if ( dir_path )
    {
        ptk_bookmarks_remove( dir_path );
        g_free( dir_path );
    }
}

static void on_shortcut_rename_activate( GtkMenuItem* item,
                                         PtkFileBrowser* file_browser )
{
    ptk_location_view_rename_selected_bookmark( file_browser->side_view );
}

static PtkMenuItemEntry shortcut_popup_menu[] =
    {
        PTK_MENU_ITEM( N_( "Open in New _Tab" ), on_shortcut_new_tab_activate, 0, 0 ),
        PTK_MENU_ITEM( N_( "Open in New _Window" ), on_shortcut_new_window_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( GTK_STOCK_REMOVE, on_shortcut_remove_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_shortcut_rename_activate, 0, 0 ),
        PTK_MENU_END
    };

gboolean
on_location_view_button_press_event ( GtkTreeView* view,
                                      GdkEventButton* evt,
                                      PtkFileBrowser* file_browser )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;
    GtkTreePath* tree_path;
    int pos;
    GtkMenu* popup;
    GtkMenuItem* item;
    char * dir_path;

    tree_sel = gtk_tree_view_get_selection( view );
    if ( evt->button == 2 )
    {
        dir_path = ptk_location_view_get_selected_dir( file_browser->side_view );
        if ( dir_path )
        {
            g_signal_emit( file_browser,
                           signals[ OPEN_ITEM_SIGNAL ],
                           0, dir_path, PTK_OPEN_NEW_TAB );
            g_free( dir_path );
        }
        return FALSE;
    }
    if ( evt->button != 3 )
        return FALSE;

    if ( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        if ( ptk_location_view_is_item_bookmark( view, &it ) )
        {
            popup = gtk_menu_new();
            ptk_menu_add_items_from_data( popup, shortcut_popup_menu,
                                          file_browser, NULL );
            gtk_widget_show_all( popup );
            g_signal_connect( popup, "selection-done",
                              G_CALLBACK( gtk_widget_destroy ), NULL );
            gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
        }
    }
    return FALSE;
}

static gboolean can_sel_change ( GtkTreeSelection *selection,
                                 GtkTreeModel *model,
                                 GtkTreePath *path,
                                 gboolean path_currently_selected,
                                 gpointer data )
{
    return can_folder_view_sel_change;
}

static void select_file_name_part( GtkEntry* entry )
{
    GtkEditable * editable = GTK_EDITABLE( entry );
    const char* file_name = gtk_entry_get_text( entry );
    const char* dot;
    int pos;

    if ( !file_name[ 0 ] || file_name[ 0 ] == '.' )
        return ;
    /* FIXME: Simply finding '.' usually gets wrong filename suffix. */
    dot = g_utf8_strrchr( file_name, -1, '.' );
    if ( dot )
    {
        pos = g_utf8_pointer_to_offset( file_name, dot );
        gtk_editable_select_region( editable, 0, pos );
    }
}

static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFileBrowserViewMode view_mode )
{
    GtkWidget * folder_view;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;
    switch ( view_mode )
    {
    case FBVM_ICON_VIEW:
        folder_view = ptk_icon_view_new ();
        ptk_icon_view_set_selection_mode ( PTK_ICON_VIEW( folder_view ),
                                           GTK_SELECTION_MULTIPLE );

        ptk_icon_view_set_pixbuf_column ( PTK_ICON_VIEW( folder_view ), COL_FILE_BIG_ICON );
        ptk_icon_view_set_text_column ( PTK_ICON_VIEW( folder_view ), COL_FILE_NAME );

        ptk_icon_view_set_column_spacing( PTK_ICON_VIEW( folder_view ), 4 );
        ptk_icon_view_set_item_width ( PTK_ICON_VIEW( folder_view ), 110 );

        ptk_icon_view_set_enable_search( PTK_ICON_VIEW( folder_view ), TRUE );
        ptk_icon_view_set_search_column( PTK_ICON_VIEW( folder_view ), COL_FILE_NAME );

        gtk_cell_layout_clear ( GTK_CELL_LAYOUT ( folder_view ) );

        /* renderer = gtk_cell_renderer_pixbuf_new (); */
        renderer = ptk_file_icon_renderer_new();
        /* add the icon renderer */
        g_object_set ( G_OBJECT ( renderer ),
                       "follow_state", TRUE,
                       NULL );
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, FALSE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "pixbuf", COL_FILE_BIG_ICON );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "info", COL_FILE_INFO );
        /* add the name renderer */
        renderer = ptk_text_renderer_new ();

        g_object_set ( G_OBJECT ( renderer ),
                       "wrap-mode", PANGO_WRAP_WORD_CHAR,
                       "wrap-width", 110,
                       "xalign", 0.5,
                       "yalign", 0.0,
                       NULL );
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, TRUE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "text", COL_FILE_NAME );

        ptk_icon_view_enable_model_drag_source (
            PTK_ICON_VIEW( folder_view ),
            ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ),
            drag_targets,
            sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
            GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );

        ptk_icon_view_enable_model_drag_dest (
            PTK_ICON_VIEW( folder_view ),
            drag_targets,
            sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
            GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );

        g_signal_connect ( ( gpointer ) folder_view,
                           "item_activated",
                           G_CALLBACK ( on_folder_view_item_activated ),
                           file_browser );

        g_signal_connect ( ( gpointer ) folder_view,
                           "key_press_event",
                           G_CALLBACK ( on_folder_view_key_press_event ),
                           file_browser );
        g_signal_connect_after ( ( gpointer ) folder_view,
                                 "selection-changed",
                                 G_CALLBACK ( on_folder_view_item_sel_change ),
                                 file_browser );

        break;
    case FBVM_LIST_VIEW:
        folder_view = gtk_tree_view_new ();
        init_list_view( GTK_TREE_VIEW( folder_view ) );
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( folder_view ) );
        gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE );

        gtk_tree_view_enable_model_drag_source (
            GTK_TREE_VIEW( folder_view ),
            ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ),
            drag_targets,
            sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
            GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );

        gtk_tree_view_enable_model_drag_dest (
            GTK_TREE_VIEW( folder_view ),
            drag_targets,
            sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
            GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );

        g_signal_connect ( ( gpointer ) folder_view,
                           "row_activated",
                           G_CALLBACK ( on_folder_view_row_activated ),
                           file_browser );

        g_signal_connect_after ( ( gpointer ) tree_sel,
                                 "changed",
                                 G_CALLBACK ( on_folder_view_item_sel_change ),
                                 file_browser );

        gtk_tree_selection_set_select_function( tree_sel,
                                                can_sel_change, file_browser, NULL );
        break;
    }

    g_signal_connect ( ( gpointer ) folder_view,
                       "button-press-event",
                       G_CALLBACK ( on_folder_view_button_press_event ),
                       file_browser );
    g_signal_connect ( ( gpointer ) folder_view,
                       "button-release-event",
                       G_CALLBACK ( on_folder_view_button_release_event ),
                       file_browser );

    /* init drag & drop support */

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-received",
                       G_CALLBACK ( on_folder_view_drag_data_received ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-get",
                       G_CALLBACK ( on_folder_view_drag_data_get ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-begin",
                       G_CALLBACK ( on_folder_view_drag_begin ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-motion",
                       G_CALLBACK ( on_folder_view_drag_motion ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-leave",
                       G_CALLBACK ( on_folder_view_drag_leave ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-drop",
                       G_CALLBACK ( on_folder_view_drag_drop ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-end",
                       G_CALLBACK ( on_folder_view_drag_end ),
                       file_browser );

    return folder_view;
}


static void init_list_view( GtkTreeView* list_view )
{
    GtkTreeViewColumn * col;
    GtkCellRenderer *renderer;
    GtkCellRenderer *pix_renderer;

    int cols[] = { COL_FILE_NAME, COL_FILE_SIZE, COL_FILE_DESC,
                   COL_FILE_PERM, COL_FILE_OWNER, COL_FILE_MTIME };
    int i;

    const char* titles[] =
        {
            N_( "Name" ), N_( "Size" ), N_( "Type" ),
            N_( "Permission" ), N_( "Owner:Group" ), N_( "Last Modification" )
        };

    for ( i = 0; i < G_N_ELEMENTS( cols ); ++i )
    {
        col = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_resizable ( col, TRUE );

        renderer = gtk_cell_renderer_text_new();

        if ( i == 0 )
        { /* First column */
            gtk_object_set( GTK_OBJECT( renderer ),
                            /* "editable", TRUE, */
                            "ellipsize", PANGO_ELLIPSIZE_END, NULL );
            /*
            g_signal_connect( renderer, "editing-started",
                              G_CALLBACK( on_filename_editing_started ), NULL );
            */
            pix_renderer = ptk_file_icon_renderer_new();

            gtk_tree_view_column_pack_start( col, pix_renderer, FALSE );
            gtk_tree_view_column_set_attributes( col, pix_renderer,
                                                 "pixbuf", COL_FILE_SMALL_ICON,
                                                 "info", COL_FILE_INFO, NULL );

            gtk_tree_view_column_set_expand ( col, TRUE );
            gtk_tree_view_column_set_min_width( col, 200 );
        }

        gtk_tree_view_column_pack_start( col, renderer, TRUE );

        gtk_tree_view_column_set_attributes( col, renderer, "text", cols[ i ], NULL );
        gtk_tree_view_append_column ( list_view, col );
        gtk_tree_view_column_set_title( col, _( titles[ i ] ) );
        gtk_tree_view_column_set_sort_indicator( col, TRUE );
        gtk_tree_view_column_set_sort_column_id( col, cols[ i ] );
        gtk_tree_view_column_set_sort_order( col, GTK_SORT_DESCENDING );
    }

    col = gtk_tree_view_get_column( list_view, 2 );
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
    gtk_tree_view_column_set_fixed_width ( col, 120 );

    gtk_tree_view_set_rules_hint ( list_view, TRUE );
}

void ptk_file_browser_refresh( PtkFileBrowser* file_browser )
{
    /*
    * FIXME:
    * Do nothing when there is unfinished task running in the
    * working thread.
    * This should be fixed with a better way in the future.
    */
    if ( file_browser->busy )
        return ;

    ptk_file_browser_chdir( file_browser,
                            ptk_file_browser_get_cwd( file_browser ),
                            FALSE );
}

guint ptk_file_browser_get_n_all_files( PtkFileBrowser* file_browser )
{
    return file_browser->dir ? file_browser->dir->n_files : 0;
}

guint ptk_file_browser_get_n_visible_files( PtkFileBrowser* file_browser )
{
    return file_browser->file_list ?
           gtk_tree_model_iter_n_children( file_browser->file_list, NULL ) : 0;
}

GList* folder_view_get_selected_items( PtkFileBrowser* file_browser,
                                       GtkTreeModel** model )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        *model = ptk_icon_view_get_model( PTK_ICON_VIEW( file_browser->folder_view ) );
        return ptk_icon_view_get_selected_items( PTK_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        return gtk_tree_selection_get_selected_rows( tree_sel, model );
    }
    return NULL;
}

static char* folder_view_get_drop_dir( PtkFileBrowser* file_browser, int x, int y )
{
    GtkTreePath * tree_path = NULL;
    GtkTreeModel *model;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path;

    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_widget_to_icon_coords ( PTK_ICON_VIEW( file_browser->folder_view ),
                                              x, y, &x, &y );
        tree_path = folder_view_get_tree_path_at_pos( file_browser, x, y );
        model = ptk_icon_view_get_model( PTK_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                       NULL, &col, NULL, NULL );
        if ( col == gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), 0 ) )
        {
            gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                                &tree_path, NULL );
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        }
    }
    if ( tree_path )
    {
        if ( G_UNLIKELY( ! gtk_tree_model_get_iter( model, &it, tree_path ) ) )
            return NULL;

        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        if ( file )
        {
            if ( vfs_file_info_is_dir( file ) )
            {
                dest_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                              vfs_file_info_get_name( file ), NULL );
            }
            else  /* Drop on a file, not folder */
            {
                /* Return current directory */
                dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
            }
            vfs_file_info_unref( file );
        }
        gtk_tree_path_free( tree_path );
    }
    else
    {
        dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
    }
    return dest_path;
}

void on_folder_view_drag_data_received ( GtkWidget *widget,
                                         GdkDragContext *drag_context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData *sel_data,
                                         guint info,
                                         guint time,
                                         gpointer user_data )
{
    gchar **list, **puri;
    GList* files = NULL;
    PtkFileTask* task;
    VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    char* dest_dir;
    char* file_path;
    GtkWidget* parent_win;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-received" );

    if ( ( sel_data->length >= 0 ) && ( sel_data->format == 8 ) )
    {
        puri = list = gtk_selection_data_get_uris( sel_data );
        if ( puri )
        {
            if ( 0 == ( drag_context->action &
                        ( GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK ) ) )
            {
                drag_context->action = GDK_ACTION_MOVE;
            }
            gtk_drag_finish ( drag_context, TRUE, FALSE, time );

            while ( *puri )
            {
                if ( **puri == '/' )
                {
                    file_path = g_strdup( *puri );
                }
                else
                {
                    file_path = g_filename_from_uri( *puri, NULL, NULL );
                }

                if ( file_path )
                {
                    files = g_list_prepend( files, file_path );
                }
                ++puri;
            }
            g_strfreev( list );

            switch ( drag_context->action )
            {
            case GDK_ACTION_COPY:
                file_action = VFS_FILE_TASK_COPY;
                break;
            case GDK_ACTION_LINK:
                file_action = VFS_FILE_TASK_LINK;
                break;
            }

            if ( files )
            {
                dest_dir = folder_view_get_drop_dir( file_browser, x, y );
                /* g_print( "dest_dir = %s\n", dest_dir ); */
                parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
                task = ptk_file_task_new( file_action,
                                          files,
                                          dest_dir,
                                          GTK_WINDOW( parent_win ) );
                ptk_file_task_run( task );
                g_free( dest_dir );
            }
            gtk_drag_finish ( drag_context, TRUE, FALSE, time );
            return ;
        }
    }
    gtk_drag_finish ( drag_context, FALSE, FALSE, time );
}

void on_folder_view_drag_data_get ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    PtkFileBrowser *file_browser )
{
    GdkAtom type = gdk_atom_intern( "text/uri-list", FALSE );
    gchar* uri;
    GString* uri_list = g_string_sized_new( 8192 );
    GList* sels = ptk_file_browser_get_selected_files( file_browser );
    GList* sel;
    VFSFileInfo* file;
    char* full_path;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-get" );

    drag_context->actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
    drag_context->suggested_action = GDK_ACTION_MOVE;

    for ( sel = sels; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                      vfs_file_info_get_name( file ), NULL );
        uri = g_filename_to_uri( full_path, NULL, NULL );
        g_free( full_path );
        g_string_append( uri_list, uri );
        g_free( uri );

        g_string_append( uri_list, "\r\n" );
    }
    g_list_foreach( sels, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sels );
    gtk_selection_data_set ( sel_data, type, 8,
                             ( guchar* ) uri_list->str, uri_list->len + 1 );
    g_string_free( uri_list, TRUE );
}


void on_folder_view_drag_begin ( GtkWidget *widget,
                                 GdkDragContext *drag_context,
                                 gpointer user_data )
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-begin" );
    /* gtk_drag_set_icon_stock ( drag_context, GTK_STOCK_DND_MULTIPLE, 1, 1 ); */
    gtk_drag_set_icon_default( drag_context );
}

static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y )
{
    GtkScrolledWindow * scroll;
    GtkAdjustment *vadj;
    gint vy;
    gdouble vpos;
    GtkTreePath *tree_path;

    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW( file_browser->folder_view ), x, y );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                       &tree_path, NULL, NULL, NULL );
    }
    return tree_path;
}

gboolean on_folder_view_auto_scroll( GtkScrolledWindow* scroll )
{
    GtkAdjustment * vadj;
    gdouble vpos;

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );

    if ( folder_view_auto_scroll_direction == GTK_DIR_UP )
    {
        vpos -= vadj->step_increment;
        if ( vpos > vadj->lower )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, vadj->lower );
    }
    else
    {
        vpos += vadj->step_increment;
        if ( ( vpos + vadj->page_size ) < vadj->upper )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, ( vadj->upper - vadj->page_size ) );
    }

    return TRUE;
}

gboolean on_folder_view_drag_motion ( GtkWidget *widget,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      PtkFileBrowser* file_browser )
{
    GtkScrolledWindow * scroll;
    GtkAdjustment *hadj, *vadj;
    gint vx, vy;
    gdouble hpos, vpos;
    GtkTreeModel* model;
    GtkTreePath *tree_path;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    gchar *file_name;
    gchar* dir_path;
    VFSFileInfo* file;
    GdkDragAction suggested_action;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-motion" );

    scroll = GTK_SCROLLED_WINDOW( gtk_widget_get_parent ( widget ) );

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );

    if ( y < 32 )
    {
        /* Auto scroll up */
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_UP;
            folder_view_auto_scroll_timer = g_timeout_add(
                                                150,
                                                ( GSourceFunc ) on_folder_view_auto_scroll,
                                                scroll );
        }
    }
    else if ( y > ( widget->allocation.height - 32 ) )
    {
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_DOWN;
            folder_view_auto_scroll_timer = g_timeout_add(
                                                150,
                                                ( GSourceFunc ) on_folder_view_auto_scroll,
                                                scroll );
        }
    }
    else if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }

    tree_path = NULL;
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_widget_to_icon_coords( PTK_ICON_VIEW( widget ), x, y, &x, &y );
        tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW( widget ), x, y );
        model = ptk_icon_view_get_model( PTK_ICON_VIEW( widget ) );
    }
    else
    {
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ), x, y,
                                            NULL, &col, NULL, NULL ) )
        {
            if ( gtk_tree_view_get_column ( GTK_TREE_VIEW( widget ), 0 ) == col )
            {
                gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW( widget ), x, y,
                                                    &tree_path, NULL );
                model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            }
        }
    }

    if ( tree_path )
    {
        if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( ! file || ! vfs_file_info_is_dir( file ) )
            {
                gtk_tree_path_free( tree_path );
                tree_path = NULL;
            }
            vfs_file_info_unref( file );
        }
    }

    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_set_drag_dest_item ( PTK_ICON_VIEW( widget ),
                                           tree_path, PTK_ICON_VIEW_DROP_INTO );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ),
                                         tree_path,
                                         GTK_TREE_VIEW_DROP_INTO_OR_AFTER );
    }

    if ( tree_path )
        gtk_tree_path_free( tree_path );

    suggested_action = ( drag_context->actions & GDK_ACTION_MOVE ) ?
                       GDK_ACTION_MOVE : drag_context->suggested_action;
    gdk_drag_status ( drag_context, suggested_action, time );

    return TRUE;
}

gboolean on_folder_view_drag_leave ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     guint time,
                                     PtkFileBrowser* file_browser )
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-leave" );

    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
}


gboolean on_folder_view_drag_drop ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    guint time,
                                    PtkFileBrowser* file_browser )
{
    GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-drop" );

    gtk_drag_get_data ( widget, drag_context, target, time );
    return TRUE;
}


void on_folder_view_drag_end ( GtkWidget *widget,
                               GdkDragContext *drag_context,
                               gpointer user_data )
{
    PtkFileBrowser * file_browser = ( PtkFileBrowser* ) user_data;
    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_set_drag_dest_item( PTK_ICON_VIEW( widget ), NULL, 0 );
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ), NULL, 0 );
    }
}

void ptk_file_browser_rename_selected_file( PtkFileBrowser* file_browser )
{
    GtkWidget * input_dlg;
    GtkLabel* prompt;
    GList* files;
    VFSFileInfo* file;
    char* ufile_name;
    char* file_name;
    char* from_path;
    char* to_path;

    gtk_widget_grab_focus( file_browser->folder_view );
    files = ptk_file_browser_get_selected_files( file_browser );
    if ( ! files )
        return ;
    file = ( VFSFileInfo* ) files->data;
    vfs_file_info_ref( file );
    g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( files );

    input_dlg = ptk_input_dialog_new( _( "Rename File" ),
                                      _( "Please input new file name:" ),
                                      vfs_file_info_get_disp_name( file ),
                                      GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ) );
    gtk_window_set_default_size( GTK_WINDOW( input_dlg ),
                                 360, -1 );

    /* Without this line, selected region in entry cannot be set */
    gtk_widget_show( input_dlg );
    if ( ! vfs_file_info_is_dir( file ) )
        select_file_name_part( ptk_input_dialog_get_entry( input_dlg ) );

    while ( gtk_dialog_run( GTK_DIALOG( input_dlg ) ) == GTK_RESPONSE_OK )
    {
        prompt = GTK_LABEL( ptk_input_dialog_get_label( input_dlg ) );

        ufile_name = ptk_input_dialog_get_text( input_dlg );
        file_name = g_filename_from_utf8( ufile_name, -1, NULL, NULL, NULL );
        g_free( ufile_name );

        if ( file_name )
        {
            from_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
            to_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                        file_name, NULL );

            if ( g_file_test( to_path, G_FILE_TEST_EXISTS ) )
            {
                gtk_label_set_text( prompt,
                                    _( "The file name you specified has existed.\n"
                                       "Please input a new one:" ) );
            }
            else
            {
                if ( 0 == g_rename( from_path, to_path ) )
                    break;
                else
                {
                    gtk_label_set_text( prompt, strerror( errno ) );
                }
            }
            g_free( from_path );
            g_free( to_path );
        }
        else
        {
            gtk_label_set_text( prompt,
                                _( "The file name you specified has existed.\n"
                                   "Please input a new one:" ) );
        }
    }
    gtk_widget_destroy( input_dlg );

    vfs_file_info_unref( file );

#if 0
    /* In place editing causes problems. So, I disable it. */
    if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        gtk_tree_view_get_cursor( GTK_TREE_VIEW( file_browser->folder_view ),
                                  &tree_path, NULL );
        if ( ! tree_path )
            return ;
        col = gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), 0 );
        renderers = gtk_tree_view_column_get_cell_renderers( col );
        for ( l = renderers; l; l = l->next )
        {
            if ( GTK_IS_CELL_RENDERER_TEXT( l->data ) )
            {
                renderer = GTK_CELL_RENDERER( l->data );
                gtk_tree_view_set_cursor_on_cell( GTK_TREE_VIEW( file_browser->folder_view ),
                                                  tree_path,
                                                  col, renderer,
                                                  TRUE );
                break;
            }
        }
        g_list_free( renderers );
        gtk_tree_path_free( tree_path );
    }
    else if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        ptk_icon_view_get_cursor( PTK_ICON_VIEW( file_browser->folder_view ),
                                  &tree_path, &renderer );
        if ( ! tree_path || !renderer )
            return ;
        g_object_set( G_OBJECT( renderer ), "editable", TRUE, NULL );
        ptk_icon_view_set_cursor( PTK_ICON_VIEW( file_browser->folder_view ),
                                  tree_path,
                                  renderer,
                                  TRUE );
        gtk_tree_path_free( tree_path );
    }
#endif
}

gboolean ptk_file_browser_can_paste( PtkFileBrowser* file_browser )
{
    /* FIXME: return FALSE when we don't have write permission */
    return FALSE;
}

void ptk_file_browser_paste( PtkFileBrowser* file_browser )
{
    GList * sel_files, *sel;
    VFSFileInfo* file;
    gchar* dest_dir = NULL;

    sel_files = ptk_file_browser_get_selected_files( file_browser );
    if ( sel_files && sel_files->next == NULL &&
            ( file = ( VFSFileInfo* ) sel_files->data ) &&
            vfs_file_info_is_dir( ( VFSFileInfo* ) sel_files->data ) )
    {
        dest_dir = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                     vfs_file_info_get_name( file ), NULL );
    }
    ptk_clipboard_paste_files(
        GTK_WINDOW( gtk_widget_get_toplevel( file_browser ) ),
        dest_dir ? dest_dir : ptk_file_browser_get_cwd( file_browser ) );
    if ( dest_dir )
        g_free( dest_dir );
    if ( sel_files )
    {
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
}

gboolean ptk_file_browser_can_cut_or_copy( PtkFileBrowser* file_browser )
{
    return FALSE;
}

void ptk_file_browser_cut( PtkFileBrowser* file_browser )
{
    /* What "cut" and "copy" do are the same.
    *  The only difference is clipboard_action = GDK_ACTION_MOVE.
    */
    ptk_file_browser_cut_or_copy( file_browser, FALSE );
}

void ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser,
                                   gboolean copy )
{
    GList * sels;

    sels = ptk_file_browser_get_selected_files( file_browser );
    if ( ! sels )
        return ;
    ptk_clipboard_cut_or_copy_files( ptk_file_browser_get_cwd( file_browser ),
                                     sels, copy );
    g_list_foreach( sels, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sels );
}

void ptk_file_browser_copy( PtkFileBrowser* file_browser )
{
    ptk_file_browser_cut_or_copy( file_browser, TRUE );
}

gboolean ptk_file_browser_can_delete( PtkFileBrowser* file_browser )
{
    /* FIXME: return FALSE when we don't have write permission. */
    return TRUE;
}

void ptk_file_browser_delete( PtkFileBrowser* file_browser )
{
    GtkWidget * dlg;
    GList* file_list;
    gchar* file_path;
    gint ret;
    GList* sel;
    VFSFileInfo* file;
    VFSFileTask* task;
    GtkWidget* parent_win;

    if ( ! file_browser->n_sel_files )
        return ;

    dlg = gtk_message_dialog_new( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_WARNING,
                                  GTK_BUTTONS_YES_NO,
                                  _( "Really delete selected files?" ) );
    ret = gtk_dialog_run( GTK_DIALOG( dlg ) );
    gtk_widget_destroy( dlg );
    if ( ret != GTK_RESPONSE_YES )
    {
        return ;
    }

    file_list = ptk_file_browser_get_selected_files( file_browser );
    for ( sel = file_list; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        file_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                      vfs_file_info_get_name( file ), NULL );
        vfs_file_info_unref( file );
        sel->data = file_path;
    }
    parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    task = ptk_file_task_new( VFS_FILE_TASK_DELETE,
                              file_list,
                              NULL,
                              GTK_WINDOW( parent_win ) );
    ptk_file_task_run( task );
}

GList* ptk_file_browser_get_selected_files( PtkFileBrowser* file_browser )
{
    GList * sel_files;
    GList* sel;
    GList* file_list = NULL;
    GtkTreeModel* model;
    GtkTreeIter it;
    VFSFileInfo* file;

    sel_files = folder_view_get_selected_items( file_browser, &model );

    if ( !sel_files )
        return NULL;

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data );
        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        file_list = g_list_append( file_list, file );
    }
    g_list_foreach( sel_files,
                    ( GFunc ) gtk_tree_path_free,
                    NULL );
    g_list_free( sel_files );
    return file_list;
}

void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser )
{
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

static gboolean open_files_with_app( GList* files, const char* app_desktop,
                                     PtkFileBrowser* file_browser )
{
    gchar * name;
    GError* err = NULL;
    GList* l;
    VFSAppDesktop* app;

    /* Check whether this is an app desktop file or just a command line */
    /* Not a desktop entry name */
    if ( g_str_has_suffix ( app_desktop, ".desktop" ) )
    {
        app = vfs_app_desktop_new( app_desktop );
    }
    else
    {
        /*
        * If we are lucky enough, there might be a desktop entry
        * for this program
        */
        name = g_strdup_printf( app_desktop, ".desktop" );
        if ( g_file_test( name, G_FILE_TEST_EXISTS ) )
        {
            app = vfs_app_desktop_new( name );
        }
        else
        {
            /* dirty hack! */
            app = vfs_app_desktop_new( NULL );
            app->exec = g_strdup( app_desktop );
        }
        g_free( name );
    }

    if ( ! vfs_app_desktop_open_files( app, files, &err ) )
    {
        ptk_show_error( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ),
                        err->message );
        g_error_free( err );
    }

    vfs_app_desktop_unref( app );
}

static void open_files_with_each_app( gpointer key, gpointer value, gpointer user_data )
{
    const char * app_desktop = ( const char* ) key;
    GList* files = ( GList* ) value;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    open_files_with_app( files, app_desktop, file_browser );
}

static void free_file_list_hash( gpointer key, gpointer value, gpointer user_data )
{
    const char * app_desktop;
    GList* files;

    app_desktop = ( const char* ) key;
    files = ( GList* ) value;
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );
}

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                    const char* app_desktop )

{
    GList * sel_files = NULL, *l;
    gchar* full_path;
    const char* desktop_file;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    gboolean is_first_dir = TRUE;
    GList* files_to_open = NULL;
    GHashTable* file_list_hash = NULL;
    GError* err;
    char* new_dir = NULL;
    char* choosen_app = NULL;

    sel_files = ptk_file_browser_get_selected_files( file_browser );

    for ( l = sel_files; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        if ( G_UNLIKELY( ! file ) )
            continue;

        full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                      vfs_file_info_get_name( file ), NULL );
        if ( G_LIKELY( full_path ) )
        {
            if ( ! app_desktop )
            {
                if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    if ( ! new_dir )
                        new_dir = full_path;
                    else
                    {
                        g_signal_emit( file_browser,
                                       signals[ OPEN_ITEM_SIGNAL ],
                                       0, full_path, PTK_OPEN_NEW_TAB );
                        g_free( full_path );
                    }
                    continue;
                }
                /* If this file is an executable file, run it. */
                if ( vfs_file_info_is_executable( file, full_path ) )
                {
                    err = NULL;
                    if ( ! g_spawn_command_line_async ( full_path, &err ) )
                    {
                        ptk_show_error( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ),
                                        err->message );
                        g_error_free( err );
                    }
                    else
                    {
                        g_signal_emit( file_browser,
                                       signals[ OPEN_ITEM_SIGNAL ],
                                       0, full_path, PTK_OPEN_FILE );
                    }
                    g_free( full_path );
                    continue;
                }

                mime_type = vfs_file_info_get_mime_type( file );
                app_desktop = vfs_mime_type_get_default_action( mime_type );

                if ( !app_desktop && xdg_mime_is_text_file( full_path, mime_type->type ) )
                {
                    /* FIXME: special handling for plain text file */
                    /* app = get_default_app_for_mime_type( XDG_MIME_TYPE_PLAIN_TEXT ); */
                }
                if ( !app_desktop )
                {
                    /* Let the user choose an application */
                    choosen_app = ptk_choose_app_for_mime_type(
                                      gtk_widget_get_toplevel( file_browser ),
                                      mime_type );
                    app_desktop = choosen_app;
                }
                if ( ! app_desktop )
                {
                    g_free( full_path );
                    continue;
                }

                files_to_open = NULL;
                if ( ! file_list_hash )
                    file_list_hash = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
                else
                    files_to_open = g_hash_table_lookup( file_list_hash, app_desktop );
                files_to_open = g_list_append( files_to_open, full_path );
                g_hash_table_replace( file_list_hash, app_desktop, files_to_open );

                app_desktop = NULL;
                vfs_mime_type_unref( mime_type );
            }
            else
            {
                files_to_open = g_list_append( files_to_open, full_path );
            }
        }
    }

    if ( sel_files )
    {
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }

    if ( file_list_hash )
    {
        g_hash_table_foreach( file_list_hash,
                              open_files_with_each_app, file_browser );
        g_hash_table_foreach( file_list_hash,
                              free_file_list_hash, NULL );
        g_hash_table_destroy( file_list_hash );
    }
    else if ( files_to_open && app_desktop )
    {
        open_files_with_app( files_to_open,
                             app_desktop, file_browser );
        g_list_foreach( files_to_open, ( GFunc ) g_free, NULL );
        g_list_free( files_to_open );
    }

    if ( new_dir )
    {
        ptk_file_browser_chdir( file_browser, new_dir, TRUE );
        g_free( new_dir );
    }
}

static PtkMenuItemEntry create_new_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_popup_new_folder_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_popup_new_text_file_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry extract_menu[] =
    {
        PTK_MENU_ITEM( N_( "E_xtract Here" ), on_popup_extract_here_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Extract _To" ), "gtk-directory", on_popup_extract_to_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry basic_popup_menu[] =
    {
        PTK_MENU_ITEM( N_( "Open _with..." ), NULL, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_popup_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_popup_copy_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_popup_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_popup_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_popup_rename_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Compress" ), on_popup_compress_activate, 0, 0 ),
        PTK_POPUP_MENU( N_( "E_xtract" ), &extract_menu ),
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", &create_new_menu ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "_Properties" ), "gtk-info", on_popup_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_MENU_END
    };


static GtkWidget* ptk_file_browser_create_basic_popup_menu()
{
    GtkWidget * popup;
    GtkWidget *open_with;
    GtkWidget *create_new;
    GtkWidget *extract;
    GtkAccelGroup *accel_group;

    accel_group = gtk_accel_group_new ();

    popup = gtk_menu_new ();
    g_object_set_data( G_OBJECT( popup ), "accel", accel_group );

    basic_popup_menu[ 0 ].ret = &open_with;
    basic_popup_menu[ 9 ].ret = &extract;
    basic_popup_menu[ 10 ].ret = &create_new;
    ptk_menu_add_items_from_data( popup, basic_popup_menu, popup, accel_group );

    g_object_set_data( G_OBJECT( popup ), "open_with", open_with );
    g_object_set_data( G_OBJECT( popup ), "create_new", create_new );
    g_object_set_data( G_OBJECT( popup ), "extract", extract );

    gtk_widget_show_all( GTK_WIDGET( popup ) );

    g_signal_connect( ( gpointer ) popup, "selection-done",
                      G_CALLBACK ( gtk_widget_destroy ),
                      NULL );

    return popup;
}

/* Retrive popup menu for selected files */
GtkWidget* ptk_file_browser_create_popup_menu_for_file( PtkFileBrowser* file_browser,
                                                        const char* file_path,
                                                        VFSFileInfo* info )
{
    GtkWidget * popup = NULL;
    VFSMimeType* mime_type;

    GtkWidget *open;
    char* open_title;
    GtkWidget *open_in_new_tab;
    GtkWidget *open_in_new_win;
    GtkWidget *open_terminal;
    GtkWidget *open_with;
    GtkWidget *open_with_menu;
    GtkWidget *seperator;
    GtkWidget *open_with_another;

    GtkWidget *image;
    GtkWidget *extract;
    GtkWidget *create_new;

    GtkWidget *app_menu_item;
    gboolean is_dir;

    char **apps, **app;
    char* app_name, *default_app_name = NULL;
    VFSAppDesktop* desktop_file;

    GdkPixbuf* app_icon, *open_icon = NULL;
    int icon_w, icon_h;
    GtkWidget* app_img;

    popup = ptk_file_browser_create_basic_popup_menu();

    g_object_set_data( G_OBJECT( popup ), "PtkFileBrowser", file_browser );

    open_with = GTK_WIDGET( g_object_get_data( G_OBJECT( popup ), "open_with" ) );
    create_new = GTK_WIDGET( g_object_get_data( G_OBJECT( popup ), "create_new" ) );
    extract = GTK_WIDGET( g_object_get_data( G_OBJECT( popup ), "extract" ) );

    /* Add some special menu items */
    mime_type = vfs_file_info_get_mime_type( info );

    if ( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
    {
        seperator = gtk_separator_menu_item_new ();
        gtk_widget_show ( seperator );
        gtk_menu_shell_insert ( GTK_MENU_SHELL( popup ), seperator, 0 );

        open_in_new_tab = gtk_menu_item_new_with_mnemonic ( _( "Open in New _Tab" ) );
        gtk_widget_show ( open_in_new_tab );
        gtk_menu_shell_insert ( GTK_MENU_SHELL( popup ), open_in_new_tab, 1 );
        g_signal_connect ( ( gpointer ) open_in_new_tab, "activate",
                           G_CALLBACK ( on_popup_open_in_new_tab_activate ),
                           file_browser );

        open_in_new_win = gtk_menu_item_new_with_mnemonic ( _( "Open in New _Window" ) );
        gtk_widget_show ( open_in_new_win );
        gtk_menu_shell_insert ( GTK_MENU_SHELL( popup ), open_in_new_win, 2 );
        g_signal_connect ( ( gpointer ) open_in_new_win, "activate",
                           G_CALLBACK ( on_popup_open_in_new_win_activate ),
                           file_browser );

        open_terminal = gtk_image_menu_item_new_with_mnemonic ( _( "Open in Terminal" ) );
        gtk_menu_shell_insert ( GTK_MENU_SHELL( popup ), open_terminal, 3 );
        g_signal_connect_swapped ( ( gpointer ) open_terminal, "activate",
                                   G_CALLBACK ( ptk_file_browser_open_terminal ),
                                   file_browser );
        gtk_widget_add_accelerator( open_terminal, "activate",
                                    GTK_ACCEL_GROUP( g_object_get_data( G_OBJECT( popup ), "accel" ) ),
                                    GDK_F4, 0, GTK_ACCEL_VISIBLE );
        image = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU );
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open_terminal ), image );
        gtk_widget_show_all ( open_terminal );

        is_dir = TRUE;
        gtk_widget_show( create_new );
    }
    else
    {
        is_dir = FALSE;
        gtk_widget_show( open_with );
        gtk_widget_destroy( create_new );
    }

    /*  Add all of the apps  */
    open_with_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( open_with ), open_with_menu );

    apps = vfs_mime_type_get_actions( mime_type );
    if ( vfs_file_info_is_text( info, file_path ) ) /* for text files */
    {
        char **tmp, **txt_apps;
        VFSMimeType* txt_type;
        int len1, len2;
        txt_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
        txt_apps = vfs_mime_type_get_actions( txt_type );
        if( txt_apps )
        {
            len1 = apps ? g_strv_length( apps ) : 0;
            len2 = g_strv_length( txt_apps );
            apps = vfs_mime_type_join_actions( apps, len1, txt_apps, len2 );
            g_strfreev( txt_apps );
        }
        vfs_mime_type_unref( txt_type );
    }

    default_app_name = NULL;
    if ( apps )
    {
        for ( app = apps; *app; ++app )
        {
            if ( ( app - apps ) == 1 )                          /* Add a separator after default app */
            {
                seperator = gtk_separator_menu_item_new ();
                gtk_widget_show ( seperator );
                gtk_container_add ( GTK_CONTAINER ( open_with_menu ), seperator );
            }
            desktop_file = vfs_app_desktop_new( *app );
            app_name = vfs_app_desktop_get_disp_name( desktop_file );
            if ( app_name )
            {
                app_menu_item = gtk_image_menu_item_new_with_label ( app_name );
                if ( app == apps )
                    default_app_name = app_name;
            }
            else
                app_menu_item = gtk_image_menu_item_new_with_label ( *app );

            g_object_set_data_full( G_OBJECT( app_menu_item ), "desktop_file",
                                    desktop_file, vfs_app_desktop_unref );

            gtk_icon_size_lookup_for_settings( gtk_settings_get_default(),
                                               GTK_ICON_SIZE_MENU,
                                               &icon_w, &icon_h );

            app_icon = vfs_app_desktop_get_icon( desktop_file,
                                                 icon_w > icon_h ? icon_w : icon_h );
            if ( app_icon )
            {
                app_img = gtk_image_new_from_pixbuf( app_icon );
                gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM( app_menu_item ), app_img );
                if ( app == apps )                         /* Default app */
                    open_icon = app_icon;
                else
                    gdk_pixbuf_unref( app_icon );
            }
            gtk_container_add ( GTK_CONTAINER ( open_with_menu ), app_menu_item );

            g_signal_connect( G_OBJECT( app_menu_item ), "activate",
                              G_CALLBACK( on_popup_run_app ), ( gpointer ) file_browser );
        }

        seperator = gtk_separator_menu_item_new ();
        gtk_container_add ( GTK_CONTAINER ( open_with_menu ), seperator );

        g_strfreev( apps );
    }

    open_with_another = gtk_menu_item_new_with_mnemonic ( _( "_Open with another program" ) );
    gtk_container_add ( GTK_CONTAINER ( open_with_menu ), open_with_another );
    g_signal_connect ( ( gpointer ) open_with_another, "activate",
                       G_CALLBACK ( on_popup_open_with_another_activate ),
                       file_browser );

    gtk_widget_show_all( open_with_menu );

    /* Create Open menu item */
    open = NULL;
    if ( !is_dir )                          /* Not a dir */
    {
        if ( vfs_file_info_is_executable( info, file_path ) )
        {
            open = gtk_image_menu_item_new_with_mnemonic( _( "E_xecute" ) );
            app_img = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU );
            gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
        }
        else
        {
            /* FIXME: Only show default app name when all selected files have the same type. */
            if ( default_app_name )
            {
                open_title = g_strdup_printf( _( "_Open with \"%s\"" ), default_app_name );
                open = gtk_image_menu_item_new_with_mnemonic( open_title );
                g_free( open_title );
                if ( open_icon )
                {
                    app_img = gtk_image_new_from_pixbuf( open_icon );
                    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
                }
            }
            else
            {
                open = gtk_image_menu_item_new_with_mnemonic( _( "_Open" ) );
            }
        }
    }
    else
    {
        open = gtk_image_menu_item_new_with_mnemonic( _( "_Open" ) );
        app_img = gtk_image_new_from_icon_name( "gnome-fs-directory", GTK_ICON_SIZE_MENU );
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
    }
    gtk_widget_show( open );
    g_signal_connect( open, "activate", G_CALLBACK( on_popup_open_activate ), popup );
    gtk_menu_shell_insert( GTK_MENU_SHELL( popup ), open, 0 );

    if ( open_icon )
        gdk_pixbuf_unref( open_icon );

    /* Compress & Extract */
    if ( ! ptk_file_archiver_is_format_supported( mime_type, TRUE ) )
    {
        /* This is not a supported archive format */
        gtk_widget_destroy( extract );
    }

    vfs_mime_type_unref( mime_type );
    return popup;
}


void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    PtkFileBrowser * file_browser;
    file_browser = ( PtkFileBrowser* ) g_object_get_data( G_OBJECT( user_data ),
                                                          "PtkFileBrowser" );
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    char * app = NULL;
    GtkTreeModel* model;
    GtkTreeIter it;
    GList* sel_files;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    VFSFileInfo* file;
    VFSMimeType* mime_type;

    sel_files = ptk_file_browser_get_selected_files( file_browser );
    if ( sel_files )
    {
        file = ( VFSFileInfo* ) sel_files->data;
        mime_type = vfs_file_info_get_mime_type( file );
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
        if ( G_LIKELY( ! mime_type ) )
        {
            mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
        }
    }
    else
    {
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
    }

    app = ptk_choose_app_for_mime_type( gtk_widget_get_toplevel( file_browser ),
                                        mime_type );
    if ( app )
    {
        ptk_file_browser_open_selected_files_with_app( file_browser, app );
        g_free( app );
    }
    vfs_mime_type_unref( mime_type );
}

void on_popup_run_app( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
    VFSAppDesktop * desktop_file;
    const char* app;
    desktop_file = ( VFSAppDesktop* ) g_object_get_data( G_OBJECT( menuitem ),
                                                         "desktop_file" );
    if ( !desktop_file )
        return ;
    /* FIXME: How about VFSAppDesktop to PtkFileBrowser instead of app name? */
    app = vfs_app_desktop_get_name( desktop_file );
    ptk_file_browser_open_selected_files_with_app( file_browser, app );
}

void ptk_file_browser_open_terminal( PtkFileBrowser* file_browser )
{
    GList * sel;
    GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
    VFSFileInfo* file;
    char* full_path;
    char* dir;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, full_path, PTK_OPEN_TERMINAL );
            }
            else
            {
                dir = g_path_get_dirname( full_path );
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, dir, PTK_OPEN_TERMINAL );
                g_free( dir );
            }
            g_free( full_path );
        }
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
    else
    {
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                       ptk_file_browser_get_cwd( file_browser ), PTK_OPEN_TERMINAL );
    }
}


void on_popup_open_in_new_tab_activate( GtkMenuItem *menuitem,
                                        PtkFileBrowser* file_browser )
{
    GList * sel;
    GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
    VFSFileInfo* file;
    char* full_path;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, full_path, PTK_OPEN_NEW_TAB );
            }
            g_free( full_path );
        }
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
    else
    {
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                       ptk_file_browser_get_cwd( file_browser ), PTK_OPEN_NEW_TAB );
    }
}

void on_popup_open_in_new_win_activate( GtkMenuItem *menuitem,
                                        PtkFileBrowser* file_browser )
{
    GList * sel;
    GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
    VFSFileInfo* file;
    char* full_path;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, full_path, PTK_OPEN_NEW_WINDOW );
            }
            g_free( full_path );
        }
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
    else
    {
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                       ptk_file_browser_get_cwd( file_browser ), PTK_OPEN_NEW_WINDOW );
    }
}

void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( file_browser )
        ptk_file_browser_cut( file_browser );
}


void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( file_browser )
        ptk_file_browser_copy( file_browser );
}


void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( file_browser )
        ptk_file_browser_paste( file_browser );
}


void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( file_browser )
        ptk_file_browser_delete( file_browser );
}

void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( file_browser )
        ptk_file_browser_rename_selected_file( file_browser );
}

void on_popup_compress_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data )
{
    GList * files;
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( ! file_browser )
        return ;
    files = ptk_file_browser_get_selected_files( file_browser );
    ptk_file_archiver_create( gtk_widget_get_toplevel( file_browser ),
                              ptk_file_browser_get_cwd( file_browser ),
                              files );
    g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( files );
}

void on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data )
{
    GList * files;
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( ! file_browser )
        return ;
    files = ptk_file_browser_get_selected_files( file_browser );
    ptk_file_archiver_extract( gtk_widget_get_toplevel( file_browser ),
                               ptk_file_browser_get_cwd( file_browser ),
                               files, NULL );
    g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( files );
}

void on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    GList * files;
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    if ( ! file_browser )
        return ;
    files = ptk_file_browser_get_selected_files( file_browser );
    ptk_file_archiver_extract( gtk_widget_get_toplevel( file_browser ),
                               ptk_file_browser_get_cwd( file_browser ),
                               files,
                               ptk_file_browser_get_cwd( file_browser ) );
    g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( files );
}

void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );

    if ( file_browser )
        ptk_file_browser_create_new_file( file_browser, TRUE );
}

void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );

    if ( file_browser )
        ptk_file_browser_create_new_file( file_browser, FALSE );
}

static void on_properties_dlg_destroy( GtkObject* dlg, GList* sel_files )
{
    g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sel_files );
}

void ptk_file_browser_file_properties( PtkFileBrowser* file_browser )
{
    GtkWidget * dlg;
    GList* sel_files = NULL;
    VFSFileInfo* file;
    char* dir_path = NULL;

    if ( file_browser )
    {
        sel_files = ptk_file_browser_get_selected_files( file_browser );
        if ( !sel_files )
        {
            file = vfs_file_info_new();
            vfs_file_info_get( file,
                               ptk_file_browser_get_cwd( file_browser ),
                               NULL );
            sel_files = g_list_append( sel_files, file );
            dir_path = g_path_get_dirname( ptk_file_browser_get_cwd( file_browser ) );
        }
        dlg = file_properties_dlg_new( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                                       dir_path ? dir_path : ptk_file_browser_get_cwd( file_browser ),
                                       sel_files );
        g_signal_connect( dlg, "destroy",
                          G_CALLBACK( on_properties_dlg_destroy ), sel_files );
        gtk_widget_show( dlg );

        if ( dir_path )
            g_free( dir_path );
    }
}

void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    ptk_file_browser_file_properties( file_browser );
}

void ptk_file_browser_show_hidden_files( PtkFileBrowser* file_browser,
                                         gboolean show )
{
    PtkFileList * list, *old_list;

    if ( file_browser->show_hidden_files == show )
        return ;

    file_browser->show_hidden_files = show;

    if ( file_browser->file_list )
    {
        ptk_file_browser_update_model( file_browser );
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    }

    if ( file_browser->side_pane_mode == FB_SIDE_PANE_DIR_TREE )
    {
        ptk_dir_tree_view_show_hidden_files( file_browser->side_view,
                                             file_browser->show_hidden_files );
    }
}

GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser )
{
    GtkWidget * dir_tree;
    GtkTreeSelection* dir_tree_sel;
    dir_tree = ptk_dir_tree_view_new( file_browser->show_hidden_files );
    dir_tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( dir_tree ) );
    g_signal_connect ( dir_tree_sel, "changed",
                       G_CALLBACK ( on_dir_tree_sel_changed ),
                       file_browser );
    return GTK_TREE_VIEW ( dir_tree );
}

GtkTreeView* ptk_file_browser_create_location_view( PtkFileBrowser* file_browser )
{
    GtkTreeView * location_view = GTK_TREE_VIEW( ptk_location_view_new() );
    g_signal_connect ( location_view, "row-activated",
                       G_CALLBACK ( on_location_view_row_activated ),
                       file_browser );

    g_signal_connect ( location_view, "button-press-event",
                       G_CALLBACK ( on_location_view_button_press_event ),
                       file_browser );

    return location_view;
}

int file_list_order_from_sort_order( FileBrowserSortOrder order )
{
    int col;
    switch ( order )
    {
    case FB_SORT_BY_NAME:
        col = COL_FILE_NAME;
        break;
    case FB_SORT_BY_SIZE:
        col = COL_FILE_SIZE;
        break;
    case FB_SORT_BY_MTIME:
        col = COL_FILE_MTIME;
        break;
    case FB_SORT_BY_TYPE:
        col = COL_FILE_DESC;
        break;
    case FB_SORT_BY_PERM:
        col = COL_FILE_PERM;
        break;
    case FB_SORT_BY_OWNER:
        col = COL_FILE_OWNER;
        break;
    default:
        col = COL_FILE_NAME;
    }
    return col;
}

void ptk_file_browser_set_sort_order( PtkFileBrowser* file_browser,
                                      FileBrowserSortOrder order )
{
    int col;
    if ( order == file_browser->sort_order )
        return ;

    file_browser->sort_order = order;
    col = file_list_order_from_sort_order( order );

    if ( file_browser->file_list )
    {
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE( file_browser->file_list ),
            col,
            file_browser->sort_type );
    }
}

void ptk_file_browser_set_sort_type( PtkFileBrowser* file_browser,
                                     GtkSortType order )
{
    int col;
    GtkSortType old_order;

    if ( order != file_browser->sort_type )
    {
        file_browser->sort_type = order;
        if ( file_browser->file_list )
        {
            gtk_tree_sortable_get_sort_column_id(
                GTK_TREE_SORTABLE( file_browser->file_list ),
                &col, &old_order );
            gtk_tree_sortable_set_sort_column_id(
                GTK_TREE_SORTABLE( file_browser->file_list ),
                col, order );
        }
    }
}

FileBrowserSortOrder ptk_file_browser_get_sort_order( PtkFileBrowser* file_browser )
{
    return file_browser->sort_order;
}

GtkSortType ptk_file_browser_get_sort_type( PtkFileBrowser* file_browser )
{
    return file_browser->sort_type;
}

void ptk_file_browser_view_as_icons( PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == FBVM_ICON_VIEW )
        return ;

    ptk_file_list_show_thumbnails( file_browser->file_list, TRUE,
                                   file_browser->max_thumbnail );

    file_browser->view_mode = FBVM_ICON_VIEW;
    gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, FBVM_ICON_VIEW );
    ptk_icon_view_set_model( PTK_ICON_VIEW( file_browser->folder_view ),
                             file_browser->file_list );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
}

void ptk_file_browser_view_as_list ( PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == FBVM_LIST_VIEW )
        return ;

    ptk_file_list_show_thumbnails( file_browser->file_list, FALSE,
                                   file_browser->max_thumbnail );

    file_browser->view_mode = FBVM_LIST_VIEW;
    gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, FBVM_LIST_VIEW );
    gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                             file_browser->file_list );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );

}

void ptk_file_browser_create_new_file( PtkFileBrowser* file_browser,
                                       gboolean create_folder )
{
    gchar * ufull_path;
    gchar* full_path;
    gchar* ufile_name;
    GtkLabel* prompt;
    int result;
    GtkWidget* dlg;

    if ( create_folder )
    {
        dlg = ptk_input_dialog_new( _( "New Folder" ),
                                    _( "Input a name for the new folder:" ),
                                    _( "New Folder" ),
                                    GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ) );
    }
    else
    {
        dlg = ptk_input_dialog_new( _( "New File" ),
                                    _( "Input a name for the new file:" ),
                                    _( "New File" ),
                                    GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ) );
    }

    while ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        ufile_name = ptk_input_dialog_get_text( dlg );
        ufull_path = g_build_filename(
                         ptk_file_browser_get_cwd( file_browser ),
                         ufile_name, NULL );
        full_path = g_filename_from_utf8( ufull_path, -1,
                                          NULL, NULL, NULL );
        g_free( ufull_path );
        g_free( ufile_name );
        if ( g_file_test( full_path, G_FILE_TEST_EXISTS ) )
        {
            prompt = GTK_LABEL( ptk_input_dialog_get_label( dlg ) );
            gtk_label_set_text( prompt,
                                _( "The file name you specified has existed.\n"
                                   "Please input a new one:" ) );
            g_free( full_path );
            continue;
        }
        if ( create_folder )
        {
            result = g_mkdir( full_path, S_IRWXU | S_IRUSR | S_IRGRP );
        }
        else
        {
            result = g_open( full_path, O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH );
            if ( result != -1 )
                close( result );
        }
        g_free( full_path );
        break;
    }
    gtk_widget_destroy( dlg );

    if ( result == -1 )
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                        "The new file cannot be created!" );

}

guint ptk_file_browser_get_n_sel( PtkFileBrowser* file_browser,
                                  guint64* sel_size )
{
    if ( sel_size )
        * sel_size = file_browser->sel_size;
    return file_browser->n_sel_files;
}

void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                    const char* path,
                                    gboolean* cancel )
{}

void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_content_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_sel_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action )
{}

/* Side pane */

void ptk_file_browser_set_side_pane_mode( PtkFileBrowser* file_browser,
                                          PtkFileBrowserSidePaneMode mode )
{
    GtkAdjustment * adj;
    if ( file_browser->side_pane_mode == mode )
        return ;
    file_browser->side_pane_mode = mode;

    if ( ! file_browser->side_pane )
        return ;

    gtk_widget_destroy( GTK_WIDGET( file_browser->side_view ) );
    adj = gtk_scrolled_window_get_hadjustment(
              GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ) );
    gtk_adjustment_set_value( adj, 0 );
    switch ( file_browser->side_pane_mode )
    {
    case FB_SIDE_PANE_DIR_TREE:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
        gtk_toggle_tool_button_set_active ( file_browser->dir_tree_btn, TRUE );
        break;
    case FB_SIDE_PANE_BOOKMARKS:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
        gtk_toggle_tool_button_set_active ( file_browser->location_btn, TRUE );
        break;
    }
    gtk_container_add( GTK_CONTAINER( file_browser->side_view_scroll ),
                       GTK_WIDGET( file_browser->side_view ) );
    gtk_widget_show( GTK_WIDGET( file_browser->side_view ) );

    if ( file_browser->side_pane && file_browser->file_list )
    {
        side_pane_chdir( file_browser, ptk_file_browser_get_cwd( file_browser ) );
    }

    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

PtkFileBrowserSidePaneMode
ptk_file_browser_get_side_pane_mode( PtkFileBrowser* file_browser )
{
    return file_browser->side_pane_mode;
}

static void on_show_location_view( GtkWidget* item, PtkFileBrowser* file_browser )
{
    if ( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON( item ) ) )
        ptk_file_browser_set_side_pane_mode( file_browser, FB_SIDE_PANE_BOOKMARKS );
}

static void on_show_dir_tree( GtkWidget* item, PtkFileBrowser* file_browser )
{
    if ( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON( item ) ) )
        ptk_file_browser_set_side_pane_mode( file_browser, FB_SIDE_PANE_DIR_TREE );
}

static PtkToolItemEntry side_pane_bar[] = {
                                              PTK_RADIO_TOOL_ITEM( NULL, "gtk-harddisk", N_( "Location" ), on_show_location_view ),
                                              PTK_RADIO_TOOL_ITEM( NULL, "gtk-open", N_( "Directory Tree" ), on_show_dir_tree ),
                                              PTK_TOOL_END
                                          };

static void ptk_file_browser_create_side_pane( PtkFileBrowser* file_browser )
{
    GtkWidget * toolbar = gtk_toolbar_new ();
    GtkTooltips* tooltips = gtk_tooltips_new ();
    file_browser->side_pane = gtk_vbox_new( FALSE, 0 );
    file_browser->side_view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                         GTK_SHADOW_IN );

    switch ( file_browser->side_pane_mode )
    {
    case FB_SIDE_PANE_DIR_TREE:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
        break;
    case FB_SIDE_PANE_BOOKMARKS:
    default:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
        break;
    }

    gtk_container_add( GTK_CONTAINER( file_browser->side_view_scroll ),
                       GTK_WIDGET( file_browser->side_view ) );
    gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                        GTK_WIDGET( file_browser->side_view_scroll ),
                        TRUE, TRUE, 0 );

    gtk_toolbar_set_style( GTK_TOOLBAR( toolbar ), GTK_TOOLBAR_ICONS );
    side_pane_bar[ 0 ].ret = ( GtkWidget** ) & file_browser->location_btn;
    side_pane_bar[ 1 ].ret = ( GtkWidget** ) & file_browser->dir_tree_btn;
    ptk_toolbar_add_items_from_data( toolbar, side_pane_bar,
                                     file_browser, tooltips );

    gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                        toolbar, FALSE, FALSE, 0 );
    gtk_widget_show_all( file_browser->side_pane );
    gtk_paned_pack1( GTK_PANED( file_browser ),
                     file_browser->side_pane, FALSE, TRUE );
}

void ptk_file_browser_show_side_pane( PtkFileBrowser* file_browser,
                                      PtkFileBrowserSidePaneMode mode )
{
    file_browser->side_pane_mode = mode;
    if ( ! file_browser->side_pane )
    {
        ptk_file_browser_create_side_pane( file_browser );

        if ( file_browser->file_list )
        {
            side_pane_chdir( file_browser, ptk_file_browser_get_cwd( file_browser ) );
        }

        switch ( mode )
        {
        case FB_SIDE_PANE_BOOKMARKS:
            gtk_toggle_tool_button_set_active( file_browser->location_btn, TRUE );
            break;
        case FB_SIDE_PANE_DIR_TREE:
            gtk_toggle_tool_button_set_active( file_browser->dir_tree_btn, TRUE );
            break;
        }
    }
    gtk_widget_show( file_browser->side_pane );
    file_browser->show_side_pane = TRUE;
}

void ptk_file_browser_hide_side_pane( PtkFileBrowser* file_browser )
{
    if ( file_browser->side_pane )
    {
        file_browser->show_side_pane = FALSE;
        gtk_widget_destroy( file_browser->side_pane );
        file_browser->side_pane = NULL;
        file_browser->side_view = NULL;
        file_browser->side_view_scroll = NULL;
    }
}

gboolean ptk_file_browser_is_side_pane_visible( PtkFileBrowser* file_browser )
{
    return file_browser->show_side_pane;
}

void ptk_file_browser_show_thumbnails( PtkFileBrowser* file_browser,
                                       int max_file_size )
{
    file_browser->max_thumbnail = max_file_size;
    if ( file_browser->file_list )
    {
        ptk_file_list_show_thumbnails( file_browser->file_list,
                                       file_browser->view_mode == FBVM_ICON_VIEW,
                                       max_file_size );
    }
}

void ptk_file_browser_update_display( PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    GList *sel, *l;
    GtkTreePath* tree_path;

    if ( ! file_browser->file_list )
        return ;
    g_object_ref( G_OBJECT( file_browser->file_list ) );

    if ( file_browser->max_thumbnail )
        ptk_file_list_show_thumbnails( file_browser->file_list,
                                       file_browser->view_mode == FBVM_ICON_VIEW,
                                       file_browser->max_thumbnail );

    if ( file_browser->view_mode == FBVM_ICON_VIEW )
    {
        sel = ptk_icon_view_get_selected_items( PTK_ICON_VIEW( file_browser->folder_view ) );

        ptk_icon_view_set_model( PTK_ICON_VIEW( file_browser->folder_view ), NULL );
        ptk_icon_view_set_model( PTK_ICON_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            ptk_icon_view_select_path( PTK_ICON_VIEW( file_browser->folder_view ),
                                       tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    else if ( file_browser->view_mode == FBVM_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        sel = gtk_tree_selection_get_selected_rows( tree_sel, NULL );

        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), NULL );
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            gtk_tree_selection_select_path( tree_sel, tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    g_list_free( sel );
    g_object_unref( G_OBJECT( file_browser->file_list ) );
}

