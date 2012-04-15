/*
*  C Implementation: ptkdirtreeview
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-dir-tree-view.h"

#include <glib.h>
#include <glib/gi18n.h>
#include "glib-mem.h"

#include "ptk-dir-tree.h"

#include "vfs-file-info.h"
#include "vfs-file-monitor.h"

static GQuark dir_tree_view_data = 0;

static GtkTreeModel* get_dir_tree_model();

static void
on_dir_tree_row_expanded(GtkTreeView     *treeview,
                         GtkTreeIter     *iter,
                         GtkTreePath     *path,
                         gpointer user_data );

static void
on_dir_tree_row_collapsed(GtkTreeView     *treeview,
                          GtkTreeIter     *iter,
                          GtkTreePath     *path,
                          gpointer user_data );

static gboolean sel_func (GtkTreeSelection *selection,
                          GtkTreeModel *model,
                          GtkTreePath *path,
                          gboolean path_currently_selected,
                          gpointer data );

struct _DirTreeNode
{
    VFSFileInfo* file;
    GList* children;
    int n_children;
    VFSFileMonitor* monitor;
    int n_expand;
};

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[]=
    {
        { "text/uri-list", 0 , 0 }
    };

static gboolean filter_func( GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data )
{
    VFSFileInfo* file;
    const char* name;
    GtkTreeView* view = (GtkTreeView*)data;
    gboolean show_hidden = (gboolean)g_object_get_qdata( G_OBJECT(view),
                                                         dir_tree_view_data );

    if( show_hidden )
        return TRUE;

    gtk_tree_model_get( model, iter, COL_DIR_TREE_INFO, &file, -1 );
    if( G_LIKELY( file ) )
    {
        name = vfs_file_info_get_name(file);
        if( G_UNLIKELY( name && name[0] == '.' ) )
        {
            vfs_file_info_unref( file );
            return FALSE;
        }
        vfs_file_info_unref( file );
    }
    return TRUE;
}

/* Create a new dir tree view */
GtkWidget* ptk_dir_tree_view_new( gboolean show_hidden )
{
    GtkTreeView* dir_tree_view;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    GtkTreeModel* filter;

    dir_tree_view = GTK_TREE_VIEW( gtk_tree_view_new () );
    gtk_tree_view_set_headers_visible( dir_tree_view, FALSE );

    gtk_tree_view_enable_model_drag_dest ( dir_tree_view,
                                           drag_targets,
                                           sizeof(drag_targets)/sizeof(GtkTargetEntry),
                                           GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

    gtk_tree_view_enable_model_drag_source ( dir_tree_view,
                                             (GDK_CONTROL_MASK|GDK_BUTTON1_MASK|GDK_BUTTON3_MASK),
                                             drag_targets,
                                             sizeof(drag_targets)/sizeof(GtkTargetEntry),
                                             GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);
    col = gtk_tree_view_column_new ();

    renderer = (GtkCellRenderer*)ptk_file_icon_renderer_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer, "pixbuf", COL_DIR_TREE_ICON,
                                         "info", COL_DIR_TREE_INFO, NULL );
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer, "text", COL_DIR_TREE_DISP_NAME, NULL );

    gtk_tree_view_append_column ( dir_tree_view, col );

    tree_sel = gtk_tree_view_get_selection( dir_tree_view );
    gtk_tree_selection_set_select_function( tree_sel, sel_func, NULL, NULL );

    if( G_UNLIKELY(!dir_tree_view_data) )
        dir_tree_view_data = g_quark_from_static_string("show_hidden");
    g_object_set_qdata( G_OBJECT(dir_tree_view),
                        dir_tree_view_data, (gpointer)show_hidden );
    model = get_dir_tree_model();
    filter = gtk_tree_model_filter_new( model, NULL );
    g_object_unref( G_OBJECT( model ) );
    gtk_tree_model_filter_set_visible_func( GTK_TREE_MODEL_FILTER(filter),
                                            filter_func, dir_tree_view, NULL );
    gtk_tree_view_set_model( dir_tree_view, filter );
    g_object_unref( G_OBJECT( filter ) );

    g_signal_connect ( dir_tree_view, "row-expanded",
                       G_CALLBACK (on_dir_tree_row_expanded),
                       model );

    g_signal_connect_data ( dir_tree_view, "row-collapsed",
                            G_CALLBACK (on_dir_tree_row_collapsed),
                            model, NULL, G_CONNECT_AFTER );

    tree_path = gtk_tree_path_new_first();
    gtk_tree_view_expand_row( dir_tree_view, tree_path, FALSE );
    gtk_tree_path_free( tree_path );

    return GTK_WIDGET(dir_tree_view);
}

gboolean ptk_dir_tree_view_chdir( GtkTreeView* dir_tree_view, const char* path )
{
    GtkTreeModel* model;
    GtkTreeIter it, parent_it;
    GtkTreePath* tree_path = NULL;
    gchar **dirs, **dir;
    gboolean found;
    VFSFileInfo* info;

    if( !path || *path != '/' )
        return FALSE;

    dirs = g_strsplit( path + 1, "/", -1 );

    if( !dirs )
        return FALSE;

    model = gtk_tree_view_get_model( dir_tree_view );

    if( ! gtk_tree_model_iter_children ( model, &parent_it, NULL ) )
    {
        g_strfreev( dirs );
        return FALSE;
    }

    /* special case: root dir */
    if( ! dirs[0] )
    {
        it = parent_it;
        tree_path = gtk_tree_model_get_path ( model, &parent_it );
        goto _found;
    }

    for( dir = dirs; *dir; ++dir )
    {
        if( ! gtk_tree_model_iter_children ( model, &it, &parent_it ) )
        {
            g_strfreev( dirs );
            return FALSE;
        }
        found = FALSE;
        do
        {
            gtk_tree_model_get( model, &it, COL_DIR_TREE_INFO, &info, -1 );
            if(!info)
                continue;
            if( 0 == strcmp( vfs_file_info_get_name( info ), *dir ) )
            {
                tree_path = gtk_tree_model_get_path( model, &it );

                gtk_tree_view_expand_row (dir_tree_view, tree_path, FALSE);
                gtk_tree_model_get_iter( model, &parent_it, tree_path );
                found = TRUE;
                vfs_file_info_unref( info );
                break;
            }
            vfs_file_info_unref( info );
        }
        while( gtk_tree_model_iter_next( model, &it ) );

        if( ! found )
            return FALSE; /* Error! */

        if( tree_path && dir[1] )
        {
            gtk_tree_path_free( tree_path );
            tree_path = NULL;
        }
    }
_found:
    g_strfreev( dirs );
    gtk_tree_selection_select_path (
        gtk_tree_view_get_selection(dir_tree_view), tree_path );

    gtk_tree_view_scroll_to_cell ( dir_tree_view, tree_path, NULL, FALSE, 0.5, 0.5 );

    gtk_tree_path_free( tree_path );

    return TRUE;
}

/* Return a newly allocated string containing path of current selected dir. */
char* ptk_dir_tree_view_get_selected_dir( GtkTreeView* dir_tree_view )
{
    GtkTreeModel* model, *tree;
    GtkTreeIter it;
    GtkTreeIter real_it;
    GtkTreePath* tree_path;
    char* dir_path = NULL;
    GtkTreeSelection* tree_sel;

    tree_sel = gtk_tree_view_get_selection( dir_tree_view );
    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        gtk_tree_model_filter_convert_iter_to_child_iter(
            GTK_TREE_MODEL_FILTER(model), &real_it, &it );
        tree = gtk_tree_model_filter_get_model( model );
        dir_path = ptk_dir_tree_get_dir_path( PTK_DIR_TREE(tree), &real_it );
    }
    return dir_path;
}

GtkTreeModel* get_dir_tree_model()
{
    static PtkDirTree* dir_tree_model = NULL;

    if( G_UNLIKELY( ! dir_tree_model ) )
    {
        dir_tree_model = ptk_dir_tree_new( TRUE );
        g_object_add_weak_pointer( G_OBJECT( dir_tree_model ),
                                   &dir_tree_model );
    }
    else
    {
        g_object_ref( G_OBJECT( dir_tree_model ) );
    }
    return GTK_TREE_MODEL( dir_tree_model );
}

gboolean sel_func (GtkTreeSelection *selection,
                   GtkTreeModel *model,
                   GtkTreePath *path,
                   gboolean path_currently_selected,
                   gpointer data )
{
    GtkTreeIter it;
    VFSFileInfo* file;

    if( ! gtk_tree_model_get_iter( model, &it, path ) )
        return FALSE;
    gtk_tree_model_get( model, &it, COL_DIR_TREE_INFO, &file, -1 );
    if( !file )
        return FALSE;
    vfs_file_info_unref( file );
    return TRUE;
}

void ptk_dir_tree_view_show_hidden_files( GtkTreeView* dir_tree_view,
                                          gboolean show_hidden )
{
    GtkTreeModel* filter;
    g_object_set_qdata( G_OBJECT(dir_tree_view),
                        dir_tree_view_data, (gpointer)show_hidden );
    filter = gtk_tree_view_get_model( dir_tree_view );
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(filter) );
}

void on_dir_tree_row_expanded( GtkTreeView     *treeview,
                               GtkTreeIter     *iter,
                               GtkTreePath     *path,
                               gpointer user_data )
{
    GtkTreeIter real_it;
    GtkTreePath* real_path;
    GtkTreeModel* filter = gtk_tree_view_get_model( treeview );
    PtkDirTree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER( filter ), &real_it, iter );
    real_path = gtk_tree_model_filter_convert_path_to_child_path(
                    GTK_TREE_MODEL_FILTER( filter ), path );
    ptk_dir_tree_expand_row( tree, &real_it, real_path );
    gtk_tree_path_free( real_path );
}

void on_dir_tree_row_collapsed( GtkTreeView     *treeview,
                                GtkTreeIter     *iter,
                                GtkTreePath     *path,
                                gpointer user_data )
{
    GtkTreeIter real_it;
    GtkTreePath* real_path;
    GtkTreeModel* filter = gtk_tree_view_get_model( treeview );
    PtkDirTree* tree = PTK_DIR_TREE(user_data);
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER( filter ), &real_it, iter );
    real_path = gtk_tree_model_filter_convert_path_to_child_path(
                    GTK_TREE_MODEL_FILTER( filter ), path );
    ptk_dir_tree_collapse_row( tree, &real_it, real_path );
    gtk_tree_path_free( real_path );
}

