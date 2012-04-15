/*
*  C Implementation: foldercontent
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xdgmime.h"
#include "folder-content.h"

#include "file-icon.h"

#include <sys/types.h>  /* for stat */
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
  FolderContentUpdateFunc callback;
  gpointer user_data;
}FolderContentCallback;

static GHashTable* folder_hash = NULL;
static FAMConnection fam;
static GIOChannel* fam_io_channel = NULL;
static guint fam_io_watch = 0;

typedef enum{
  FCM_FOLDER_VIEW = 1 << 0,
  FCM_DIR_TREE =    1 << 1
}FolderContentMode;


static void folder_content_create_file( FolderContent* content,
                                        const char* filename );
static void folder_content_delete_file( FolderContent* content,
                                        const char* filename );
static void folder_content_update_file( FolderContent* content,
                                        const char* filename );

/*
    *  Get a GtkTreeModel containing an automatically maintained
    *  file list with all its related data.
    *  mode is bitwise-OR combination of FCM_FOLDER_VIEW and FCM_DIR_TREE
    *  If you are getting FolderContent for Dir Tree, pass the parent node of
    *  the sub dir tree in tree_parent; otherwise, tree_parent should be NULL.
*/
static FolderContent* folder_content_get( const char* path,
                                          FolderContentMode mode,
                                          GtkTreeRowReference* tree_parent,
                                          FolderContentUpdateFunc cb,
                                          gpointer user_data );

static void
folder_content_unref( FolderContent* folder_content, FolderContentMode mode,
                      FolderContentUpdateFunc cb, gpointer user_data );

/* event handler of all FAM events */
static gboolean on_fam_event(GIOChannel *channel,
                             GIOCondition cond,
                             gpointer user_data );

static gboolean connect_to_fam()
{
  if( FAMOpen( &fam ) )
  {
    fam_io_channel = NULL;
    fam.fd = -1;
    g_warning("There is no gamin server\n");
    return FALSE;
  }

#if HAVE_FAMNOEXISTS
  /*
  * Disable the initital directory content loading.
  * This can greatly speed up directory loading, but
  * unfortunately, it's not compatible with original FAM.
  */
  FAMNoExists( &fam );  /* This is an extension of gamin */
#endif

  fam_io_channel = g_io_channel_unix_new( fam.fd );
  g_io_channel_set_encoding(fam_io_channel, NULL, NULL );
  g_io_channel_set_buffered(fam_io_channel, FALSE);

  fam_io_watch = g_io_add_watch( fam_io_channel,
                                 G_IO_IN|G_IO_HUP,
                                 on_fam_event,
                                 NULL );
/*
  g_print("Connected to FAM server\n");
*/
}

static void disconnect_from_fam()
{
  if( fam_io_channel )
  {
    g_io_channel_unref(fam_io_channel);
    fam_io_channel = NULL;
    g_source_remove(fam_io_watch);

    FAMClose( &fam );
/*
  g_print("Disonnected from FAM server\n");
*/
  }
}

/*
* Initialize the folder content manager.
* Establish connection with gamin/fam.
*/
gboolean folder_content_init()
{
  folder_hash = g_hash_table_new(g_str_hash, g_str_equal);
  if( ! connect_to_fam() )
    return FALSE;
  return TRUE;
}

/* final cleanup */
void folder_content_clean()
{
  disconnect_from_fam();
  if( folder_hash )
    g_hash_table_destroy( folder_hash );
}

/* file names and path are encoded in on-disk encoding */
static void add_file_to_list( GtkListStore* list,
                              const char* path,
                              const char* file_name )
{
  GtkTreeIter it;
  struct stat *file_stat;
  gchar* full_path;
  gchar* ufile_name;
  const char* mime_type;

  GdkPixbuf* icon;
  GdkPixbuf *folder_icon = get_folder_icon32();
  GdkPixbuf *file_icon = get_regular_file_icon32();

  /* FIXME: Some strange bugs here. Maybe it's the bug
  *         of gtk+ itself?
  */
  ufile_name = g_filename_to_utf8( file_name, -1,
                                   NULL, NULL, NULL );

  if( ufile_name ) {
    full_path = g_build_filename( path, file_name, NULL );

    file_stat = g_new( struct stat, 1 );
    lstat( full_path, file_stat );

    g_free( full_path );

    if( S_ISDIR( file_stat->st_mode ) ){
      icon = folder_icon;
      mime_type = XDG_MIME_TYPE_DIRECTORY;
    }
    else  {
      mime_type = xdg_mime_get_mime_type_from_file_name( ufile_name );

      if( mime_type ) {
        if( mime_type == XDG_MIME_TYPE_UNKNOWN ||
            0 == strncmp(mime_type, "image/", 6) )
        {
          mime_type = NULL;
          icon = file_icon;
        }
        else
          icon = get_mime_icon( mime_type );
      }
      else {
        icon = file_icon;
      }
    }

    /* We don't care about the order since the list will be sorted */
    gtk_list_store_insert_with_values( GTK_LIST_STORE(list), &it,
                                       0,
                                       COL_FILE_ICON, icon,
                                       COL_FILE_NAME, ufile_name,
                                       COL_FILE_STAT, file_stat,
                                       COL_FILE_TYPE, mime_type, -1 );

    g_free( ufile_name );
  }
  else
    g_warning("invalid utf8!!!\n");
}

static GtkListStore* folder_view_model_new( const char* path )
{
  GtkListStore* list;
  const gchar* file_name;
  GDir* dir;

  list = gtk_list_store_new( NUM_COL_FOLDERVIEW,
                             GDK_TYPE_PIXBUF,
                             G_TYPE_STRING,
                             G_TYPE_POINTER,
                             G_TYPE_POINTER,
                             G_TYPE_STRING,
                             G_TYPE_STRING,
                             G_TYPE_STRING,
                             G_TYPE_STRING,
                             G_TYPE_STRING );

  dir = g_dir_open( path, 0, NULL );

  while( file_name = g_dir_read_name(dir) )
  {
    add_file_to_list( list, path, file_name );
    /* gtk_main_iteration_do ( FALSE ); */
  }
  g_dir_close( dir );
  return list;
}

static gboolean has_sub_dir( const char* dir_path )
{
  const gchar* name;
  gchar* full_path;
  GDir* subdir;
  gboolean ret = FALSE;
  struct stat file_stat;

  subdir = g_dir_open( dir_path, 0, NULL );

  if( subdir )
  {
    while( (name = g_dir_read_name( subdir )) )
    {
      full_path = g_build_filename (dir_path, name, NULL);
      memset( &file_stat, 0, sizeof(file_stat) );
      stat( full_path, &file_stat );
      if( S_ISDIR( file_stat.st_mode ) )
      {
        ret = TRUE;
        g_free( full_path );
        break;
      }
      g_free( full_path );
    }
    g_dir_close( subdir );
  }
  return ret;
}

static gboolean is_link_to_dir( const char* link )
{
  gboolean ret = FALSE;
  char* tmp = NULL;
  char* dir_name;
  struct stat statbuf;
  char* target = g_file_read_link(link, NULL);
  if( G_LIKELY(target) )
  {
    if( G_LIKELY( !g_path_is_absolute(target) ) )
    {
      dir_name = g_path_get_dirname( link );
      if( G_LIKELY( dir_name ) ) {
        tmp = g_build_filename( dir_name, target, NULL );
        g_free( dir_name );
        g_free( target );
        target = tmp;
      }
    }
    if( G_LIKELY( stat(target, &statbuf) == 0 ) ) {
      if( S_ISDIR(statbuf.st_mode) )
        ret = TRUE;
      else if( G_UNLIKELY( S_ISLNK(statbuf.st_mode) ) )
        ret = is_link_to_dir( target );
    }
    if( tmp )
      g_free(tmp);
  }
  return ret;
}

/* file names and path are encoded in on-disk encoding */
static void add_file_to_dir_tree( GtkTreeStore* tree,
                                  GtkTreeIter* parent_it,
                                  const char* path,
                                  const char* filename)
{
  GtkTreeIter it, sub_it;
  GDir *subdir;
  gchar* full_path;
  gchar* ufilename;

  struct stat file_stat, *pstat;
  GdkPixbuf* folder_icon;

  full_path = g_build_filename ( path, filename, NULL );

  memset( &file_stat, 0, sizeof(file_stat) );
  lstat( full_path, &file_stat );

  if( S_ISDIR( file_stat.st_mode )
      ||  ( S_ISLNK(file_stat.st_mode) && is_link_to_dir(full_path) ) )
  {
    folder_icon = get_folder_icon20();

    gtk_tree_store_append( tree, &it, parent_it );
    ufilename = g_filename_to_utf8( filename, -1, NULL, NULL, NULL );

    gtk_tree_store_set ( tree, &it,
                        COL_DIRTREE_ICON, folder_icon,
                        COL_DIRTREE_TEXT, ufilename, -1 );
    g_free( ufilename );

    if( has_sub_dir( full_path ) )   {
      gtk_tree_store_append( tree, &sub_it, &it );
    }
  }
  g_free(full_path);
}

static void
dir_tree_sub_folders_new( FolderContent* folder_content )
{
  GtkTreeModel* tree;
  GtkTreeModel* list;
  GtkTreePath* tree_path;
  GtkTreeIter it, parent_it, sub_it, list_it;
  GtkTreeRowReference* tree_node = folder_content->tree_node;
  GtkTreeSortable *sortable;
  GDir *dir, *subdir;
  gchar* uname;
  gchar* name;
  gchar* full_path;
  int i;
  struct stat file_stat, *pstat;
  GdkPixbuf* folder_icon;
  gboolean is_dir;

  if( ! tree_node )
    return;

  tree = gtk_tree_row_reference_get_model (tree_node);
  tree_path = gtk_tree_row_reference_get_path (tree_node);
  if( !tree || !tree_path ){
    return;
  }

  gtk_tree_model_get_iter( tree, &parent_it, tree_path );
  folder_icon = get_folder_icon20();

  /* Use cached data from list store to reduce dulicated I/O */
  if( folder_content->list )
  {
    list = GTK_TREE_MODEL(folder_content->list);
    if( ! gtk_tree_model_get_iter_first ( list, &list_it) )
      return;

    do{
      gtk_tree_model_get( list, &list_it,
                          COL_FILE_STAT, &pstat, -1 );
      if( ! pstat )
        continue;
      is_dir = S_ISDIR(pstat->st_mode);
      name = uname = NULL;
      if( G_UNLIKELY( S_ISLNK(pstat->st_mode) ) )
      {
        gtk_tree_model_get( list, &list_it,
                            COL_FILE_NAME, &uname, -1 );
        name = g_filename_from_utf8( uname, -1, NULL, NULL, NULL );
        if( G_LIKELY( name ) ) {
          full_path = g_build_filename( folder_content->path, name, NULL );
          if( G_LIKELY(full_path) ) {
            is_dir = is_link_to_dir( full_path );
            g_free( full_path );
          }
        }
      }

      if( is_dir )
      {
        if( !uname ) {
          gtk_tree_model_get( list, &list_it,
                              COL_FILE_NAME, &uname, -1 );
          name = g_filename_from_utf8( uname, -1, NULL, NULL, NULL );
        }
        gtk_tree_store_append( GTK_TREE_STORE(tree),
                               &it, &parent_it );
        gtk_tree_store_set ( GTK_TREE_STORE(tree), &it,
                             COL_DIRTREE_ICON, folder_icon,
                             COL_DIRTREE_TEXT, uname, -1 );
        g_free( uname );
        full_path = g_build_filename( folder_content->path,
                                      name, NULL );
        g_free( name );
        if( full_path ) {
          if( has_sub_dir( full_path ) )   {
            gtk_tree_store_append( GTK_TREE_STORE(tree),
                                  &sub_it, &it );
          }
          g_free( full_path );
        }
      }
    } while( gtk_tree_model_iter_next (list, &list_it) );
    return;
  }

  dir = g_dir_open( folder_content->path, 0, NULL );

  while( (name = (gchar*)g_dir_read_name(dir) ) )
  {
    add_file_to_dir_tree( GTK_TREE_STORE(tree), &parent_it,
                          folder_content->path, name );
  }
  g_dir_close( dir );

/*
  sortable = GTK_TREE_SORTABLE(model);
  gtk_tree_sortable_set_sort_func( sortable, COL_DIRTREE_TEXT,
                                   sort_dir_tree_by_name, NULL, NULL );

  gtk_tree_sortable_set_sort_column_id( sortable,
                                        COL_DIRTREE_TEXT,
                                        GTK_SORT_ASCENDING );
*/

}

/*
*  Get a GtkTreeModel containing an automatically maintained
*  file list with all its related data.
*  mode is bitwise-OR combination of FCM_FOLDER_VIEW and FCM_DIR_TREE
*  If you are getting FolderContent for Dir Tree, pass the parent node of
*  the sub dir tree in tree_parent; otherwise, tree_parent should be NULL.
*/
FolderContent* folder_content_get( const char* path,
                                   FolderContentMode mode,
                                   GtkTreeRowReference* tree_node,
                                   FolderContentUpdateFunc callback,
                                   gpointer user_data )
{
  FolderContent* folder_content;
  FolderContentCallback cb_item;
  gboolean add_new = FALSE;
  folder_content = (FolderContent*)g_hash_table_lookup (
                                          folder_hash, path);

  if( ! folder_content )
  {
    folder_content = g_new0( FolderContent, 1 );
    folder_content->path = g_strdup( path );
    folder_content->callbacks = g_array_new (FALSE, FALSE, sizeof(FolderContentCallback));

    g_hash_table_insert ( folder_hash,
                          folder_content->path,
                          folder_content );
    add_new = TRUE;
  }

  if( mode & FCM_FOLDER_VIEW )
  {
    if( ! folder_content->list )  {
      folder_content->list = folder_view_model_new( path );
    }
    ++folder_content->n_ref_list;
  }

  if( mode & FCM_DIR_TREE )
  {
    /*
    * FIXME: when the sub dir tree is used in more then one node
    * of dir tree model, there'll be more than one tree_paret getting
    * the same path.  Then we should hold a GList of tree_parents in
    * our FolderContent.
    */
    if( ! folder_content->tree_node )
    {
      folder_content->tree_node = tree_node;
      dir_tree_sub_folders_new( folder_content );
    }
    ++folder_content->n_ref_tree;
  }

  /* First new instance */
  if( add_new ){
    FAMMonitorDirectory( &fam,
                          path,
                          &folder_content->request,
                          folder_content );
  }

  if( callback ) /* Install a callback */
  {
    cb_item.callback = callback;
    cb_item.user_data = user_data;
    folder_content->callbacks = g_array_append_val( folder_content->callbacks,
                                                    cb_item );
  }

  return folder_content;
}

/*
*  Unreference the object and decrease its reference count.
*/
static gboolean
list_cleanup (GtkTreeModel *model, GtkTreePath *path,
              GtkTreeIter *iter, gpointer data)
{
  struct stat* pstat;
  gtk_tree_model_get( model, iter, COL_FILE_STAT, &pstat, -1 );
  if( pstat )
    g_free( pstat );
  return FALSE;
}

void folder_content_unref( FolderContent* folder_content, FolderContentMode mode,
                           FolderContentUpdateFunc cb, gpointer user_data )
{
  int i;
  FolderContentCallback* callbacks = (FolderContentCallback*)folder_content->callbacks->data;
  if( cb && folder_content->callbacks )
  {
    for(i = 0; i < folder_content->callbacks->len; ++i )
    {
      if( callbacks[i].callback == cb && callbacks[i].user_data == user_data )
      {
        folder_content->callbacks = g_array_remove_index_fast ( folder_content->callbacks, i );
        break;
      }
    }
  }

  if( mode == FCM_FOLDER_VIEW ){
    --folder_content->n_ref_list;
    if( 0 >= folder_content->n_ref_list )
    {
      /*  list store clean up...  */
      gtk_tree_model_foreach( GTK_TREE_MODEL(folder_content->list),
                              (GtkTreeModelForeachFunc)list_cleanup, NULL);
      g_object_unref( folder_content->list );
      folder_content->list = NULL;
    }
  }

  if( mode == FCM_DIR_TREE ){
    --folder_content->n_ref_tree;
    if( 0 >= folder_content->n_ref_tree )
    {
      gtk_tree_row_reference_free(folder_content->tree_node);
      folder_content->tree_node = NULL;
    }
  }

  if( 0 >= folder_content->n_ref_list
      && 0 >= folder_content->n_ref_tree )
  {
    /* g_print("cancel monitor!\n"); */
    FAMCancelMonitor( &fam, &folder_content->request );
    g_hash_table_remove( folder_hash, folder_content->path );
    g_free( folder_content->path );
    g_array_free( folder_content->callbacks, TRUE );
    g_free( folder_content );
  }
}

typedef struct {
  FolderContent* content;
  FolderContentCallback cb;
}FolderContentCallbackParam;


/* event handler of all FAM events */
static gboolean on_fam_event(GIOChannel *channel,
                             GIOCondition cond,
                             gpointer user_data )
{
  FAMEvent evt;
  FolderContent* content = NULL;
  GdkPixbuf *folder_icon = get_folder_icon32();
  GdkPixbuf *file_icon = get_regular_file_icon32();
  GdkPixbuf* icon;
  struct stat *file_stat;

  GtkTreeIter it, parent_it;
  GtkTreePath* tree_path;
  GtkTreeStore* tree;
  gchar* ufile_name;

  int i;
  FolderContentCallback* cb;
  FolderContentCallbackParam cb_param;

  if( cond & G_IO_HUP )
  {
    disconnect_from_fam();
    return FALSE; /* remove the event source */
  }

  while( FAMPending( &fam ) )
  {
    if( FAMNextEvent(&fam, &evt) > 0 )
    {
      content = (FolderContent*)evt.userdata;
      switch( evt.code )
      {
        case FAMCreated:
          folder_content_create_file( content, evt.filename );
          break;
        case FAMDeleted:
          folder_content_delete_file( content, evt.filename );
          break;
        case FAMChanged:
          folder_content_update_file( content, evt.filename );
          break;
        default:
          return TRUE;  /* Other events are not supported */
      }
      /* Call the callback functions set by the user */
      if( content->callbacks && content->callbacks->len ) {
        cb = (FolderContentCallback*)content->callbacks->data;
        for( i = 0; i < content->callbacks->len; ++i ){
          FolderContentUpdateFunc func = cb[i].callback;
          func( content, cb[i].user_data );
          /* g_idle_add( (GSourceFunc)call_update_callback, content ); */
        }
      }
    }
  }
  return TRUE;
}

FolderContent* folder_content_list_get( const char* path,
                                        FolderContentUpdateFunc cb,
                                        gpointer user_data )
{
  return folder_content_get( path, FCM_FOLDER_VIEW, NULL, cb, user_data );
}

FolderContent* folder_content_tree_get( const char* path,
                                        GtkTreeRowReference* tree_parent,
                                        FolderContentUpdateFunc cb,
                                        gpointer user_data )
{
  return folder_content_get( path, FCM_DIR_TREE, tree_parent, cb, user_data );
}

void folder_content_list_unref( FolderContent* folder_content,
                                FolderContentUpdateFunc cb, gpointer user_data )
{
  folder_content_unref( folder_content, FCM_FOLDER_VIEW, cb, user_data );
}

void folder_content_tree_unref( FolderContent* folder_content,
                                FolderContentUpdateFunc cb, gpointer user_data )
{
  folder_content_unref( folder_content, FCM_DIR_TREE, cb, user_data );
}

/*
* Find iter by name in the folder View
* filename is encoded in on-disk encoding (locale dependent)
*/
static gboolean find_file_iter( GtkTreeModel* model,
                                const char* file_name,
                                GtkTreeIter* it )
{
  gchar* name;
  gchar* ufile_name;
  if( gtk_tree_model_get_iter_first ( model, it ) )
  {
    ufile_name = g_filename_to_utf8(file_name, -1, NULL, NULL, NULL);
    if( !ufile_name )
      return FALSE;
    do  {
      gtk_tree_model_get( model, it, COL_FILE_NAME, &name, -1);
      if( ! name )
        continue;
      if( 0 == strcmp( name, ufile_name ) ) {
        g_free( name );
        g_free( ufile_name );
        return TRUE;
      }
      g_free( name );
    }while( gtk_tree_model_iter_next( model, it) );
    g_free( ufile_name );
  }
  return FALSE;
}


static void folder_content_create_file( FolderContent* content,
                                        const char* filename )
{
  GtkTreePath* tree_path;
  GtkTreeIter it, parent_it;
  GtkTreeModel* model;

  if( content->list ) {
    /* Prevent duplicated "create" notification sent by gamin */
    if( ! find_file_iter( GTK_TREE_MODEL(content->list), filename, &it ) )
      add_file_to_list( content->list, content->path, filename );
  }

  if( content->tree_node ) {
    model = gtk_tree_row_reference_get_model( content->tree_node );
    tree_path = gtk_tree_row_reference_get_path(
        content->tree_node );
    gtk_tree_model_get_iter( model, &parent_it, tree_path );

    add_file_to_dir_tree( GTK_TREE_STORE(model), &parent_it,
                          content->path, filename );
  }
}

static void folder_content_delete_file( FolderContent* content,
                                        const char* filename )
{
  GtkTreePath* tree_path;
  GtkTreeIter it, parent_it;
  GtkTreeModel* model;
  gchar* name;
  struct stat *pstat;

  /*
  * FIXME: Should remove cached data of folder content from the
  *        hash table if the removed file is a dir and its content
  *        has been cached.
  */
  if( content->list ) {
    model = GTK_TREE_MODEL(content->list);
    if( find_file_iter( model, filename, &it ) )
    {
      gtk_tree_model_get(model, &it, COL_FILE_STAT, &pstat, -1);
      g_free( pstat );
      gtk_list_store_remove ( content->list, &it );
    }
  }

  if( content->tree_node ) {
    model = gtk_tree_row_reference_get_model( content->tree_node );
    tree_path = gtk_tree_row_reference_get_path(
                                  content->tree_node );
    gtk_tree_model_get_iter( model, &parent_it, tree_path );
    if( ! gtk_tree_model_iter_children (model, &it, &parent_it) )
      return;

    do  {
      gtk_tree_model_get(model, &it, COL_DIRTREE_TEXT, &name, -1);
      if( 0 == strcmp( name, filename ) ) {
        g_free( name );
        gtk_tree_store_remove ( GTK_TREE_STORE(model), &it );
        return;
      }
      g_free( name );
    }while( gtk_tree_model_iter_next(model, &it) );
  }
}

static void folder_content_update_file( FolderContent* content,
                                        const char* filename )
{
  GtkTreePath* tree_path;
  GtkTreeIter it, parent_it;
  GtkTreeModel* model;
  gchar* name;
  struct stat *pstat;
  gchar* ufile_name;
  gchar* full_path;

  if( content->list ) {
    model = GTK_TREE_MODEL(content->list);
    if( find_file_iter( model, filename, &it ) )
    {
      gtk_tree_model_get(model, &it, COL_FILE_STAT, &pstat, -1);
      full_path = g_build_filename( content->path, filename, NULL );

      /* Update file stat */
      lstat( full_path, pstat );
      /* Reset the mime-type */
      gtk_list_store_set( content->list, &it, COL_FILE_TYPE, NULL,
                                              COL_FILE_DESC, NULL,
                                              COL_FILE_SIZE, NULL,
                                              COL_FILE_OWNER, NULL,
                                              COL_FILE_PERM, NULL,
                                              COL_FILE_MTIME, NULL, -1 );
      g_free(full_path);
    }
  }

  /* There is no need to update the dir tree currently. */
  if( content->tree_node ) {
    model = gtk_tree_row_reference_get_model( content->tree_node );
    tree_path = gtk_tree_row_reference_get_path(
        content->tree_node );
    gtk_tree_model_get_iter( model, &parent_it, tree_path );
    if( ! gtk_tree_model_iter_children (model, &it, &parent_it) )
      return;

    do  {
      gtk_tree_model_get(model, &it, COL_DIRTREE_TEXT, &name, -1);
      if( 0 == strcmp( name, filename ) ) {
        /* DO SOMETHING TO UPDATE THE ITEM */
        return;
      }
      g_free( name );
    }while( gtk_tree_model_iter_next(model, &it) );
  }
}

