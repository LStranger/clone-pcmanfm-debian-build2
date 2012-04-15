/*
*  C Implementation: ptk-file-list
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-file-list.h"
#include <gdk/gdk.h>

#include "glib-mem.h"
#include "vfs-file-info.h"

static void ptk_file_list_init ( PtkFileList *list );

static void ptk_file_list_class_init ( PtkFileListClass *klass );

static void ptk_file_list_tree_model_init ( GtkTreeModelIface *iface );

static void ptk_file_list_tree_sortable_init ( GtkTreeSortableIface *iface );

static void ptk_file_list_drag_source_init ( GtkTreeDragSourceIface *iface );

static void ptk_file_list_drag_dest_init ( GtkTreeDragDestIface *iface );

static void ptk_file_list_finalize ( GObject *object );

static GtkTreeModelFlags ptk_file_list_get_flags ( GtkTreeModel *tree_model );

static gint ptk_file_list_get_n_columns ( GtkTreeModel *tree_model );

static GType ptk_file_list_get_column_type ( GtkTreeModel *tree_model,
                                             gint index );

static gboolean ptk_file_list_get_iter ( GtkTreeModel *tree_model,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path );

static GtkTreePath *ptk_file_list_get_path ( GtkTreeModel *tree_model,
                                             GtkTreeIter *iter );

static void ptk_file_list_get_value ( GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      gint column,
                                      GValue *value );

static gboolean ptk_file_list_iter_next ( GtkTreeModel *tree_model,
                                          GtkTreeIter *iter );

static gboolean ptk_file_list_iter_children ( GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent );

static gboolean ptk_file_list_iter_has_child ( GtkTreeModel *tree_model,
                                               GtkTreeIter *iter );

static gint ptk_file_list_iter_n_children ( GtkTreeModel *tree_model,
                                            GtkTreeIter *iter );

static gboolean ptk_file_list_iter_nth_child ( GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent,
                                               gint n );

static gboolean ptk_file_list_iter_parent ( GtkTreeModel *tree_model,
                                            GtkTreeIter *iter,
                                            GtkTreeIter *child );

static gboolean ptk_file_list_get_sort_column_id( GtkTreeSortable* sortable,
                                                  gint* sort_column_id,
                                                  GtkSortType* order );

static void ptk_file_list_set_sort_column_id( GtkTreeSortable* sortable,
                                              gint sort_column_id,
                                              GtkSortType order );

static void ptk_file_list_set_sort_func( GtkTreeSortable *sortable,
                                         gint sort_column_id,
                                         GtkTreeIterCompareFunc sort_func,
                                         gpointer user_data,
                                         GtkDestroyNotify destroy );

static void ptk_file_list_set_default_sort_func( GtkTreeSortable *sortable,
                                                 GtkTreeIterCompareFunc sort_func,
                                                 gpointer user_data,
                                                 GtkDestroyNotify destroy );

static void ptk_file_list_sort ( PtkFileList* list );

/* signal handlers */

static void ptk_file_list_file_created( VFSDir* dir, VFSFileInfo* file,
                                        PtkFileList* list );

static void ptk_file_list_file_deleted( VFSDir* dir, VFSFileInfo* file,
                                        PtkFileList* list );

static void ptk_file_list_file_changed( VFSDir* dir, VFSFileInfo* file,
                                        PtkFileList* list );


static GObjectClass* parent_class = NULL;

static GType column_types[ N_FILE_LIST_COLS ];


typedef struct _ThumbThreadData
{
    PtkFileList* list;
    gboolean big;
}ThumbThreadData;


GType ptk_file_list_get_type ( void )
{
    static GType type = 0;
    if ( G_UNLIKELY( !type ) )
    {
        static const GTypeInfo type_info =
            {
                sizeof ( PtkFileListClass ),
                NULL,                                           /* base_init */
                NULL,                                           /* base_finalize */
                ( GClassInitFunc ) ptk_file_list_class_init,
                NULL,                                           /* class finalize */
                NULL,                                           /* class_data */
                sizeof ( PtkFileList ),
                0,                                             /* n_preallocs */
                ( GInstanceInitFunc ) ptk_file_list_init
            };

        static const GInterfaceInfo tree_model_info =
            {
                ( GInterfaceInitFunc ) ptk_file_list_tree_model_init,
                NULL,
                NULL
            };

        static const GInterfaceInfo tree_sortable_info =
            {
                ( GInterfaceInitFunc ) ptk_file_list_tree_sortable_init,
                NULL,
                NULL
            };

        static const GInterfaceInfo drag_src_info =
            {
                ( GInterfaceInitFunc ) ptk_file_list_drag_source_init,
                NULL,
                NULL
            };

        static const GInterfaceInfo drag_dest_info =
            {
                ( GInterfaceInitFunc ) ptk_file_list_drag_dest_init,
                NULL,
                NULL
            };

        type = g_type_register_static ( G_TYPE_OBJECT, "PtkFileList",
                                        &type_info, ( GTypeFlags ) 0 );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_MODEL, &tree_model_info );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_SORTABLE, &tree_sortable_info );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_src_info );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info );
    }
    return type;
}

void ptk_file_list_init ( PtkFileList *list )
{
    list->n_files = 0;
    list->files = NULL;
    list->sort_order = -1;
    list->sort_col = -1;
    /* Random int to check whether an iter belongs to our model */
    list->stamp = g_random_int();
}

void ptk_file_list_class_init ( PtkFileListClass *klass )
{
    GObjectClass * object_class;

    parent_class = ( GObjectClass* ) g_type_class_peek_parent ( klass );
    object_class = ( GObjectClass* ) klass;

    object_class->finalize = ptk_file_list_finalize;
}

void ptk_file_list_tree_model_init ( GtkTreeModelIface *iface )
{
    iface->get_flags = ptk_file_list_get_flags;
    iface->get_n_columns = ptk_file_list_get_n_columns;
    iface->get_column_type = ptk_file_list_get_column_type;
    iface->get_iter = ptk_file_list_get_iter;
    iface->get_path = ptk_file_list_get_path;
    iface->get_value = ptk_file_list_get_value;
    iface->iter_next = ptk_file_list_iter_next;
    iface->iter_children = ptk_file_list_iter_children;
    iface->iter_has_child = ptk_file_list_iter_has_child;
    iface->iter_n_children = ptk_file_list_iter_n_children;
    iface->iter_nth_child = ptk_file_list_iter_nth_child;
    iface->iter_parent = ptk_file_list_iter_parent;

    column_types [ COL_FILE_BIG_ICON ] = GDK_TYPE_PIXBUF;
    column_types [ COL_FILE_SMALL_ICON ] = GDK_TYPE_PIXBUF;
    column_types [ COL_FILE_NAME ] = G_TYPE_STRING;
    column_types [ COL_FILE_DESC ] = G_TYPE_STRING;
    column_types [ COL_FILE_SIZE ] = G_TYPE_STRING;
    column_types [ COL_FILE_DESC ] = G_TYPE_STRING;
    column_types [ COL_FILE_PERM ] = G_TYPE_STRING;
    column_types [ COL_FILE_OWNER ] = G_TYPE_STRING;
    column_types [ COL_FILE_MTIME ] = G_TYPE_STRING;
    column_types [ COL_FILE_INFO ] = G_TYPE_POINTER;
}

void ptk_file_list_tree_sortable_init ( GtkTreeSortableIface *iface )
{
    /* iface->sort_column_changed = ptk_file_list_sort_column_changed; */
    iface->get_sort_column_id = ptk_file_list_get_sort_column_id;
    iface->set_sort_column_id = ptk_file_list_set_sort_column_id;
    iface->set_sort_func = ptk_file_list_set_sort_func;
    iface->set_default_sort_func = ptk_file_list_set_default_sort_func;
    iface->has_default_sort_func = (gboolean(*)(GtkTreeSortable *))gtk_false;
}

void ptk_file_list_drag_source_init ( GtkTreeDragSourceIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void ptk_file_list_drag_dest_init ( GtkTreeDragDestIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void ptk_file_list_finalize ( GObject *object )
{
    PtkFileList *list = ( PtkFileList* ) object;

    ptk_file_list_set_dir( list, NULL );
    /* must chain up - finalize parent */
    ( * parent_class->finalize ) ( object );
}

PtkFileList *ptk_file_list_new ( VFSDir* dir, gboolean show_hidden )
{
    PtkFileList * list;
    list = ( PtkFileList* ) g_object_new ( PTK_TYPE_FILE_LIST, NULL );
    list->show_hidden = show_hidden;
    ptk_file_list_set_dir( list, dir );
    return list;
}

void ptk_file_list_set_dir( PtkFileList* list, VFSDir* dir )
{
    GList* l;
    int i;
    VFSFileInfo* file;

    if( list->dir == dir )
        return;

    if ( list->dir )
    {
        if( list->max_thumbnail > 0 )
        {
            for( l = list->files; l; l = l->next )
            {
                file = (VFSFileInfo*)l->data;
                if( vfs_file_info_is_image( file )
                    && vfs_file_info_get_size( file ) < list->max_thumbnail )
                {
                    /* FIXME: vfs_dir_cancel_thumbnail_request(  ); */
                }
            }
        }
        g_list_foreach( list->files, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( list->files );
        g_signal_handlers_disconnect_by_func( list->dir,
                                              ptk_file_list_file_created, list );
        g_signal_handlers_disconnect_by_func( list->dir,
                                              ptk_file_list_file_deleted, list );
        g_signal_handlers_disconnect_by_func( list->dir,
                                              ptk_file_list_file_changed, list );
        vfs_dir_unref( list->dir );
    }

    list->dir = dir;
    list->files = NULL;
    list->n_files = 0;
    if( ! dir )
        return;

    vfs_dir_ref( list->dir );

    g_signal_connect( list->dir, "file-created",
                      G_CALLBACK(ptk_file_list_file_created),
                      list );
    g_signal_connect( list->dir, "file-deleted",
                      G_CALLBACK(ptk_file_list_file_deleted),
                      list );
    g_signal_connect( list->dir, "file-changed",
                      G_CALLBACK(ptk_file_list_file_changed),
                      list );

    if( dir && dir->file_list )
    {
        for( l = dir->file_list; l; l = l->next )
        {
            if( list->show_hidden ||
                    ((VFSFileInfo*)l->data)->disp_name[0] != '.' )
            {
                list->files = g_list_prepend( list->files, l->data );
                vfs_file_info_ref( (VFSFileInfo*)l->data );
                ++list->n_files;
            }
        }
    }
}

GtkTreeModelFlags ptk_file_list_get_flags ( GtkTreeModel *tree_model )
{
    g_return_val_if_fail ( PTK_IS_FILE_LIST( tree_model ), ( GtkTreeModelFlags ) 0 );
    return ( GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST );
}

gint ptk_file_list_get_n_columns ( GtkTreeModel *tree_model )
{
    return N_FILE_LIST_COLS;
}

GType ptk_file_list_get_column_type ( GtkTreeModel *tree_model,
                                      gint index )
{
    g_return_val_if_fail ( PTK_IS_FILE_LIST( tree_model ), G_TYPE_INVALID );
    g_return_val_if_fail ( index < G_N_ELEMENTS( column_types ) && index >= 0, G_TYPE_INVALID );
    return column_types[ index ];
}

gboolean ptk_file_list_get_iter ( GtkTreeModel *tree_model,
                                  GtkTreeIter *iter,
                                  GtkTreePath *path )
{
    PtkFileList *list;
    gint *indices, n, depth;
    GList* l;

    g_assert(PTK_IS_FILE_LIST(tree_model));
    g_assert(path!=NULL);

    list = PTK_FILE_LIST(tree_model);

    indices = gtk_tree_path_get_indices(path);
    depth   = gtk_tree_path_get_depth(path);

    /* we do not allow children */
    g_assert(depth == 1); /* depth 1 = top level; a list only has top level nodes and no children */

    n = indices[0]; /* the n-th top level row */

    if ( n >= list->n_files || n < 0 )
        return FALSE;

    l = g_list_nth( list->files, n );

    g_assert(l != NULL);

    /* We simply store a pointer in the iter */
    iter->stamp = list->stamp;
    iter->user_data  = l;
    iter->user_data2 = l->data;
    iter->user_data3 = NULL;   /* unused */

    return TRUE;
}

GtkTreePath *ptk_file_list_get_path ( GtkTreeModel *tree_model,
                                      GtkTreeIter *iter )
{
    GtkTreePath* path;
    GList* l;
    PtkFileList* list = PTK_FILE_LIST(tree_model);

    g_return_val_if_fail (list, NULL);
    g_return_val_if_fail (iter->stamp == list->stamp, NULL);
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (iter->user_data != NULL, NULL);

    l = (GList*) iter->user_data;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, g_list_index(list->files, l->data) );
    return path;
}

void ptk_file_list_get_value ( GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               gint column,
                               GValue *value )
{
    GList* l;
    PtkFileList* list = PTK_FILE_LIST(tree_model);
    VFSFileInfo* info;
    GdkPixbuf* icon;

    g_return_if_fail (PTK_IS_FILE_LIST (tree_model));
    g_return_if_fail (iter != NULL);
    g_return_if_fail (column < G_N_ELEMENTS(column_types) );

    g_value_init (value, column_types[column] );

    l = (GList*) iter->user_data;
    g_return_if_fail ( l != NULL );

    info = (VFSFileInfo*)iter->user_data2;

    switch(column)
    {
    case COL_FILE_BIG_ICON:
        icon = NULL;
        /* special file can use special icons saved as thumbnails*/
        if( list->max_thumbnail > vfs_file_info_get_size( info ) 
            && info->flags == VFS_FILE_INFO_NONE )
            icon = vfs_file_info_get_big_thumbnail( info );
        if( ! icon )
            icon = vfs_file_info_get_big_icon( info );
        if( icon )
        {
            g_value_set_object( value, icon );
            gdk_pixbuf_unref( icon );
        }
        break;
    case COL_FILE_SMALL_ICON:
        icon = NULL;
        /* special file can use special icons saved as thumbnails*/
        if( list->max_thumbnail > vfs_file_info_get_size( info ) )
            icon = vfs_file_info_get_small_thumbnail( info );
        if( !icon )
            icon = vfs_file_info_get_small_icon( info );
        if( icon )
        {
            g_value_set_object( value, icon );
            gdk_pixbuf_unref( icon );
        }
        break;
    case COL_FILE_NAME:
        g_value_set_string( value, vfs_file_info_get_disp_name(info) );
        break;
    case COL_FILE_SIZE:
        g_value_set_string( value, vfs_file_info_get_disp_size(info) );
        break;
    case COL_FILE_DESC:
        g_value_set_string( value, vfs_file_info_get_mime_type_desc( info ) );
        break;
    case COL_FILE_PERM:
        g_value_set_string( value, vfs_file_info_get_disp_perm(info) );
        break;
    case COL_FILE_OWNER:
        g_value_set_string( value, vfs_file_info_get_disp_owner(info) );
        break;
    case COL_FILE_MTIME:
        g_value_set_string( value, vfs_file_info_get_disp_mtime(info) );
        break;
    case COL_FILE_INFO:
        vfs_file_info_ref( info );
        g_value_set_pointer( value, info );
        break;
    }
}

gboolean ptk_file_list_iter_next ( GtkTreeModel *tree_model,
                                   GtkTreeIter *iter )
{
    GList* l;
    PtkFileList* list;

    g_return_val_if_fail (PTK_IS_FILE_LIST (tree_model), FALSE);

    if (iter == NULL || iter->user_data == NULL)
        return FALSE;

    list = PTK_FILE_LIST(tree_model);
    l = (GList *) iter->user_data;

    /* Is this the last l in the list? */
    if ( ! l->next )
        return FALSE;

    iter->stamp = list->stamp;
    iter->user_data = l->next;
    iter->user_data2 = l->next->data;

    return TRUE;
}

gboolean ptk_file_list_iter_children ( GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent )
{
    PtkFileList* list;
    g_return_val_if_fail ( parent == NULL || parent->user_data != NULL, FALSE );

    /* this is a list, nodes have no children */
    if ( parent )
        return FALSE;

    /* parent == NULL is a special case; we need to return the first top-level row */
    g_return_val_if_fail ( PTK_IS_FILE_LIST ( tree_model ), FALSE );
    list = PTK_FILE_LIST( tree_model );

    /* No rows => no first row */
    if ( list->dir->n_files == 0 )
        return FALSE;

    /* Set iter to first item in list */
    iter->stamp = list->stamp;
    iter->user_data = list->files;
    iter->user_data2 = list->files->data;
    return TRUE;
}

gboolean ptk_file_list_iter_has_child ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter )
{
    return FALSE;
}

gint ptk_file_list_iter_n_children ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter )
{
    PtkFileList* list;
    g_return_val_if_fail ( PTK_IS_FILE_LIST ( tree_model ), -1 );
    g_return_val_if_fail ( iter == NULL || iter->user_data != NULL, FALSE );
    list = PTK_FILE_LIST( tree_model );
    /* special case: if iter == NULL, return number of top-level rows */
    if ( !iter )
        return list->n_files;
    return 0; /* otherwise, this is easy again for a list */
}

gboolean ptk_file_list_iter_nth_child ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter,
                                        GtkTreeIter *parent,
                                        gint n )
{
    GList* l;
    PtkFileList* list;

    g_return_val_if_fail (PTK_IS_FILE_LIST (tree_model), FALSE);
    list = PTK_FILE_LIST(tree_model);

    /* a list has only top-level rows */
    if(parent)
        return FALSE;

    /* special case: if parent == NULL, set iter to n-th top-level row */
    if( n >= list->n_files || n < 0 )
        return FALSE;

    l = g_list_nth( list->files, n );
    g_assert( l != NULL );

    iter->stamp = list->stamp;
    iter->user_data = l;
    iter->user_data2 = l->data;

    return TRUE;
}

gboolean ptk_file_list_iter_parent ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     GtkTreeIter *child )
{
    return FALSE;
}

gboolean ptk_file_list_get_sort_column_id( GtkTreeSortable* sortable,
                                           gint* sort_column_id,
                                           GtkSortType* order )
{
    PtkFileList* list = (PtkFileList*)sortable;
    if( sort_column_id )
        *sort_column_id = list->sort_col;
    if( order )
        *order = list->sort_order;
    return TRUE;
}

void ptk_file_list_set_sort_column_id( GtkTreeSortable* sortable,
                                       gint sort_column_id,
                                       GtkSortType order )
{
    PtkFileList* list = (PtkFileList*)sortable;
    if( list->sort_col == sort_column_id && list->sort_order == order )
        return;
    list->sort_col = sort_column_id;
    list->sort_order = order;
    gtk_tree_sortable_sort_column_changed (sortable);
    ptk_file_list_sort (list);
}

void ptk_file_list_set_sort_func( GtkTreeSortable *sortable,
                                  gint sort_column_id,
                                  GtkTreeIterCompareFunc sort_func,
                                  gpointer user_data,
                                  GtkDestroyNotify destroy )
{
    g_warning( "ptk_file_list_set_sort_func: Not supported\n" );
}

void ptk_file_list_set_default_sort_func( GtkTreeSortable *sortable,
                                          GtkTreeIterCompareFunc sort_func,
                                          gpointer user_data,
                                          GtkDestroyNotify destroy )
{
    g_warning( "ptk_file_list_set_default_sort_func: Not supported\n" );
}

static gint ptk_file_list_compare( gconstpointer a,
                                   gconstpointer b,
                                   gpointer user_data)
{
    VFSFileInfo* file1 = (VFSFileInfo*)a;
    VFSFileInfo* file2 = (VFSFileInfo*)b;
    PtkFileList* list = (PtkFileList*)user_data;
    int ret;
    /* put folders before files */
    ret = vfs_file_info_is_dir(file1) - vfs_file_info_is_dir(file2);
    if( ret )
        return -ret;

    /* FIXME: strings should not be treated as ASCII when sorted  */
    switch( list->sort_col )
    {
    case COL_FILE_NAME:
        ret = g_ascii_strcasecmp( vfs_file_info_get_disp_name(file1),
                                  vfs_file_info_get_disp_name(file2) );
        break;
    case COL_FILE_SIZE:
        ret = file1->size - file2->size;
        break;
    case COL_FILE_DESC:
        ret = g_ascii_strcasecmp( vfs_file_info_get_mime_type_desc(file1),
                                  vfs_file_info_get_mime_type_desc(file2) );
        break;
    case COL_FILE_PERM:
        ret = g_ascii_strcasecmp( vfs_file_info_get_disp_perm(file1),
                                  vfs_file_info_get_disp_perm(file2) );
        break;
    case COL_FILE_OWNER:
        ret = g_ascii_strcasecmp( vfs_file_info_get_disp_owner(file1),
                                  vfs_file_info_get_disp_owner(file2) );
        break;
    case COL_FILE_MTIME:
        ret = file1->mtime - file2->mtime;
        break;
    }
    return list->sort_order == GTK_SORT_ASCENDING ? ret : -ret;
}

void ptk_file_list_sort ( PtkFileList* list )
{
    GHashTable* old_order;
    gint *new_order;
    GtkTreePath *path;
    GList* l;
    int i;

    if( list->n_files <=1 )
        return;

    old_order = g_hash_table_new( g_direct_hash, g_direct_equal );
    /* save old order */
    for( i = 0, l = list->files; l; l = l->next, ++i )
        g_hash_table_insert( old_order, l, GINT_TO_POINTER(i) );

    /* sort the list */
    list->files = g_list_sort_with_data( list->files,
                                         ptk_file_list_compare, list );

    /* save new order */
    new_order = g_new( int, list->n_files );
    for( i = 0, l = list->files; l; l = l->next, ++i )
        new_order[i] = (guint)g_hash_table_lookup( old_order, l );
    g_hash_table_destroy( old_order );
    path = gtk_tree_path_new ();
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (list),
                                   path, NULL, new_order);
    gtk_tree_path_free (path);
    g_free( new_order );
}

void ptk_file_list_file_created( VFSDir* dir,
                                 VFSFileInfo* file,
                                 PtkFileList* list )
{
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;
    VFSFileInfo* file2;

    if( ! list->show_hidden && vfs_file_info_get_name(file)[0] == '.' )
        return;

    for( l = list->files; l; l = l->next )
    {
        file2 = (VFSFileInfo*)l->data;
        if( ptk_file_list_compare( file2, file, list ) >= 0 )
        {
            break;
        }
    }

    list->files = g_list_insert_before( list->files, l, file );
    vfs_file_info_ref( file );
    ++list->n_files;

    if( l )
        l = l->prev;
    else
        l = g_list_last( list->files );

    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = file;

    path = gtk_tree_path_new_from_indices( g_list_index(list->files, l->data), -1 );

    gtk_tree_model_row_inserted( GTK_TREE_MODEL(list), path, &it );

    gtk_tree_path_free( path );
}

void ptk_file_list_file_deleted( VFSDir* dir,
                                 VFSFileInfo* file,
                                 PtkFileList* list )
{
    GList* l;
    GtkTreePath* path;

    if( ! list->show_hidden && vfs_file_info_get_name(file)[0] == '.' )
        return;

    l = g_list_find( list->files, file );

    if( ! l )
        return;

    path = gtk_tree_path_new_from_indices( g_list_index(list->files, l->data), -1 );

    gtk_tree_model_row_deleted( GTK_TREE_MODEL(list), path );

    gtk_tree_path_free( path );

    list->files = g_list_delete_link( list->files, l );
    vfs_file_info_unref( file );
    --list->n_files;
}

void ptk_file_list_file_changed( VFSDir* dir,
                                 VFSFileInfo* file,
                                 PtkFileList* list )
{
    GList* l;
    GtkTreeIter it;
    GtkTreePath* path;

    if( ! list->show_hidden && vfs_file_info_get_name(file)[0] == '.' )
        return;
    l = g_list_find( list->files, file );

    if( ! l )
        return;

    it.stamp = list->stamp;
    it.user_data = l;
    it.user_data2 = l->data;

    path = gtk_tree_path_new_from_indices( g_list_index(list->files, l->data), -1 );

    gtk_tree_model_row_changed( GTK_TREE_MODEL(list), path, &it );

    gtk_tree_path_free( path );
}


void ptk_file_list_show_thumbnails( PtkFileList* list, gboolean big,
                                    int max_file_size )
{
    GList* l;
    VFSFileInfo* file;

    list->max_thumbnail = max_file_size;
    /* FIXME: This is extremely buggy!!! must be fixed before release
        Loading of big icons and small ones should be separate threads.
    */
    if( 0 == max_file_size )
        return;

    for( l = list->files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if( vfs_file_info_is_image( file )
            && vfs_file_info_get_size( file ) < list->max_thumbnail )
        {
            if( vfs_file_info_is_thumbnail_loaded( file, big ) )
                ptk_file_list_file_changed( list->dir, file, list );
            else
                vfs_dir_request_thumbnail( list->dir, file, big );
        }
    }
}

#if 0
/* I still have doubt if this works */

void ptk_file_list_show_hidden_files( PtkFileList* list )
{
    GList *l, *l2, *new_item;
    VFSFileInfo* info;
    int i = 0;
    GtkTreePath* path;
    GtkTreeIter it;

    if( list->show_hidden )
        return;
    for( l = list->dir->file_list; l; l = l->next )
    {
        info = (VFSFileInfo*)l->data;
        if( vfs_file_info_get_disp_name(info)[0] != '.' )
            continue;
        for( i = 0, l2 = list->files; l2; l2 = l2->next, ++i )
        {
            if( ptk_file_list_compare(info, l2->data, list) >= 0 )
            {
                list->files = g_list_insert_before( list->files, info, l2 );
                it.user_data = l2->prev;
                it.user_data2 = info;
                it.stamp = list->stamp;
                path = gtk_tree_path_new_from_indices(i, -1);
                gtk_tree_model_row_inserted( list, path, &it );
                gtk_tree_path_free( path );
                break;
            }
        }
    }
}

void ptk_file_list_hide_hidden_files( PtkFileList* list )
{
    GList* l;
    VFSFileInfo* info;
    GtkTreePath* path;

    if( ! list->show_hidden )
        return;
    path = gtk_tree_path_new_first();
    for( l = list->files; l; )
    {
        info = (VFSFileInfo*)l->data;
        /* hidden file */
        if( G_UNLIKELY(vfs_file_info_get_disp_name(info)[0] == '.') )
        {
            l = l->next;
            list->files = g_list_delete_link( list->files, l->prev );
            gtk_tree_model_row_deleted( GTK_TREE_MODEL(list), path );
        }
        else
        {
            l = l->next;
            gtk_tree_path_next( path );
        }
    }
    gtk_tree_path_free( path );
}

#endif
