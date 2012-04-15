/*
*  C Interface: PtkLocationView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef  _PTK_LOCATION_VIEW_H_
#define  _PTK_LOCATION_VIEW_H_

#include <gtk/gtk.h>
#include <sys/types.h>

G_BEGIN_DECLS

/* Create a new location view */
GtkWidget* ptk_location_view_new();

gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path );

char* ptk_location_view_get_selected_dir( GtkTreeView* location_view );

G_END_DECLS

#endif
