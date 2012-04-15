/*
*  C Interface: PtkRemoteFSView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef  _PTK_REMOTE_FS_VIEW_H_
#define  _PTK_REMOTE_FS_VIEW_H_

#include <gtk/gtk.h>
#include <sys/types.h>

G_BEGIN_DECLS

/* Create a new remote fs view */
GtkWidget* ptk_remote_fs_view_new();

gboolean ptk_remote_fs_view_chdir( GtkTreeView* remote_fs_view, const char* path );

char* ptk_remote_fs_view_get_selected_dir( GtkTreeView* remote_fs_view );

void ptk_remote_fs_view_rename_selected_item( GtkTreeView* remote_fs_view );

/* VFSRemoteFS* ptk_remote_fs_view_get_item(  GtkTreeView* remote_fs_view, GtkTreeIter* it ); */

G_END_DECLS

#endif
