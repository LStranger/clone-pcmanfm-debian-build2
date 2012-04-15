/*
*  C Interface: foldercontent
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _FOLDER_CONTENT_H_
#define _FOLDER_CONTENT_H_

#include <gtk/gtk.h>
#include <glib.h>
#include <fam.h>

/* Columns of folderView */
enum{
  COL_FILE_ICON = 0,
  COL_FILE_NAME,
  COL_FILE_STAT,
  COL_FILE_TYPE,
  COL_FILE_SIZE,
  COL_FILE_DESC,
  COL_FILE_PERM,
  COL_FILE_OWNER,
  COL_FILE_MTIME,
  NUM_COL_FOLDERVIEW
};

/* Columns of dirTree */
enum
{
  COL_DIRTREE_ICON = 0,
  COL_DIRTREE_TEXT,
  NUM_COL_DIRTREE
};

typedef struct{
  GtkListStore* list; /* for Folder View */
  int n_ref_list;  /* reference counting */
  GtkTreeRowReference* tree_node;  /* for Dir Tree */
  int n_ref_tree;

  FAMRequest request;
  gchar* path;
  int n_files;

  GArray* callbacks;
}FolderContent;

/* Callback function which will be called when FAM events happen */
typedef void (*FolderContentUpdateFunc)( FolderContent*,
                                         gpointer user_data );

/*
* Initialize the folder content manager.
* Establish connection with gamin/fam.
*/
gboolean folder_content_init();

/* final cleanup */
void folder_content_clean();


/* Set a callback which will be called when FAM events happen */
void folder_content_set_update_callback(FolderContentUpdateFunc func,
                                        gpointer user_data);

/*
    *  Get a GtkTreeModel containing an automatically maintained
    *  file list with all its related data.
    *  If you are getting FolderContent for Dir Tree, pass the parent node of
    *  the sub dir tree in tree_parent; otherwise, tree_parent should be NULL.
*/
FolderContent* folder_content_list_get( const char* path,
                                        FolderContentUpdateFunc callback,
                                        gpointer user_data );

FolderContent* folder_content_tree_get( const char* path,
                                        GtkTreeRowReference* tree_node,
                                        FolderContentUpdateFunc callback,
                                        gpointer user_data );

/*
*  Unreference the object and decrease its reference count.
*/
void folder_content_list_unref( FolderContent* folder_content,
                                FolderContentUpdateFunc cb, gpointer user_data );
void folder_content_tree_unref( FolderContent* folder_content,
                                FolderContentUpdateFunc cb, gpointer user_data );

/*
gboolean folder_content_find_file_iter( FolderContent* folder_content,
                                        const char* file_name,
                                        int max_len,
                                        GtkTreeIter* it );
*/

#endif

