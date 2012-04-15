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

#include "folder-content.h"

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[]={
{"text/uri-list", 0 , 0 }
};

static void
on_dir_tree_row_expanded        (GtkTreeView     *treeview,
                                 GtkTreeIter     *iter,
                                 GtkTreePath     *path,
                                 gpointer user_data );


/* Create a new dir tree view */
GtkWidget* ptk_dir_tree_view_new()
{
  GtkTreeView* dir_tree_view;
  GtkTreeViewColumn* col;
  GtkCellRenderer* renderer;

  dir_tree_view = GTK_TREE_VIEW( gtk_tree_view_new () );
  gtk_tree_view_set_headers_visible( dir_tree_view, FALSE );

  gtk_tree_view_enable_model_drag_dest ( dir_tree_view,
                                         drag_targets,
                                         sizeof(drag_targets)/sizeof(GtkTargetEntry),
                                         GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

  /* This causes serious problems because of some bugs of GTK+,
     so I disabled it currently
  gtk_tree_view_enable_model_drag_source ( dir_tree_view,
                                           (GDK_CONTROL_MASK|GDK_BUTTON1_MASK|GDK_BUTTON3_MASK),
                                           drag_targets,
                                           sizeof(drag_targets)/sizeof(GtkTargetEntry),
                                           GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);
  */
  g_signal_connect ( dir_tree_view, "row-expanded",
                     G_CALLBACK (on_dir_tree_row_expanded),
                     NULL);

  col = gtk_tree_view_column_new ();

  renderer = (GtkCellRenderer*)ptk_file_icon_renderer_new();
  gtk_tree_view_column_pack_start( col, renderer, FALSE );
  gtk_tree_view_column_set_attributes( col, renderer, "pixbuf", COL_DIRTREE_ICON,
                                                        /*"stat", COL_FILE_STAT,*/ NULL );
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start( col, renderer, TRUE );
  /*  g_object_set( renderer, "editable", TRUE, NULL ); */
  gtk_tree_view_column_set_attributes( col, renderer, "text", COL_DIRTREE_TEXT, NULL );

  gtk_tree_view_append_column ( dir_tree_view, col );
  return GTK_WIDGET(dir_tree_view);
}

gboolean ptk_dir_tree_view_chdir( GtkTreeView* dir_tree_view, const char* path )
{
  GtkTreeStore* tree;
  GtkTreeModel* model;
  GtkTreeIter it, parent_it;
  GtkTreePath* tree_path = NULL;
  gchar *name;
  gchar **dirs, **dir;
  gboolean found;

  if( !path || *path != '/' )
    return FALSE;

  dirs = g_strsplit( path + 1, "/", -1 );

  if( !dirs )
    return FALSE;

  model = gtk_tree_view_get_model( dir_tree_view );

  if( ! gtk_tree_model_iter_children ( model, &parent_it, NULL ) )
    return FALSE;

  /* special case: root dir */
  if( ! dirs[0] ) {
    it = parent_it;
    tree_path = gtk_tree_model_get_path ( model, &parent_it );
    goto _found;
  }

  for( dir = dirs; *dir; ++dir ){
    if( ! gtk_tree_model_iter_children ( model, &it, &parent_it ) )
      return FALSE;

    found = FALSE;
    if( tree_path )
      gtk_tree_path_free( tree_path );
    tree_path = gtk_tree_model_get_path( model, &it );
    do{
      if( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        break;
      gtk_tree_model_get( model, &it, COL_DIRTREE_TEXT, &name, -1 );

      if(!name)
        continue;
      if( 0 == strcmp( name, *dir ) ){
        gtk_tree_view_expand_row (dir_tree_view, tree_path, FALSE);
        gtk_tree_model_get_iter( model, &parent_it, tree_path );
        g_free( name );
        found = TRUE;
        break;
      }
      g_free( name );
      gtk_tree_path_next (tree_path);
    }while( TRUE );
    if( ! found ){
      return FALSE; /* Error! */
    }
  }
_found:
    g_strfreev( dirs );
  gtk_tree_selection_select_path (
      gtk_tree_view_get_selection(dir_tree_view), tree_path );

  gtk_tree_view_scroll_to_cell ( dir_tree_view, tree_path, NULL, FALSE, 0, 0 );

  gtk_tree_path_free( tree_path );
  return TRUE;
}

static char* dir_path_from_tree_path( GtkTreeModel* model, GtkTreePath* path )
{
  gint* idx = gtk_tree_path_get_indices( path );
  gint depth = gtk_tree_path_get_depth( path );
  GtkTreeIter it, subit, child, *parent;
  char dir_path[1024] = "";
  gchar* name;
  int i;

  if( !idx )
    return NULL;

  for( i = 0, parent = NULL; i < depth; ++i )
  {
    gtk_tree_model_iter_nth_child( model, &child, parent, idx[i] );
    name = NULL;
    gtk_tree_model_get( model, &child, COL_DIRTREE_TEXT, &name, -1 );
    if( ! name )
      return NULL;

    if( i > 1 )
      strcat( dir_path, "/" );
    strcat( dir_path, name );
    g_free( name );

    subit = child;
    parent = &subit;
  }
  return g_strdup( dir_path );
}

void on_dir_tree_row_expanded        (GtkTreeView     *treeview,
                                      GtkTreeIter     *iter,
                                      GtkTreePath     *tree_path,
                                      gpointer user_data )
{
  gint* idx;
  GtkTreeIter it, subit, child, *parent;
  GtkTreeRowReference* row_ref, *del_ref;
  FolderContent* content;
  GtkTreePath* real_path;

  char* dir_path = NULL;
  gchar* name;
  GtkTreeModel* model;
  GtkTreeModel* store;
  GtkTreeModelFilter* filter;

  model = gtk_tree_view_get_model(treeview );
  filter = GTK_TREE_MODEL_FILTER(model);

  if( !gtk_tree_model_iter_children( model, &child, iter ) ) {
    return;
  }
  gtk_tree_model_get( model, &child, COL_DIRTREE_TEXT, &name, -1 );
  if( name ){
    g_free( name );
    return;
  }

  real_path = gtk_tree_model_filter_convert_path_to_child_path( filter, tree_path );
  if( real_path )
  {
    store = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER(model) );
    dir_path = dir_path_from_tree_path( store, real_path );
    if( ! dir_path ) {
      return;
    }

    row_ref = gtk_tree_row_reference_new( store, real_path );
    content = folder_content_tree_get( dir_path, row_ref, NULL, NULL );

    gtk_tree_model_get_iter( model, &it, tree_path );
    if( gtk_tree_model_iter_children( model, &child, &it ) ) {
      gtk_tree_model_filter_convert_iter_to_child_iter( GTK_TREE_MODEL_FILTER(model),
          &it, &child );
      gtk_tree_store_remove( GTK_TREE_STORE(store), &it );
    }
    gtk_tree_path_free( real_path );
    g_free( dir_path );
  }
  return;
}

/* Return a newly allocated string containing path of current selected dir. */
char* ptk_dir_tree_view_get_selected_dir( GtkTreeView* dir_tree_view )
{
  GtkTreeModel* model;
  GtkTreeIter it;
  GtkTreePath* tree_path;
  char* dir_path = NULL;
  GtkTreeSelection* tree_sel;
  tree_sel = gtk_tree_view_get_selection( dir_tree_view );
  if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
  {
    tree_path = gtk_tree_model_get_path ( model, &it );
    dir_path = dir_path_from_tree_path( model, tree_path );
    gtk_tree_path_free( tree_path );
  }
  return dir_path;
}

