/*
*  C Interface: mimeaction
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _MIME_ACTION_H_
#define _MIME_ACTION_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* NOTE: All file names used in these APIs are encode in UTF-8 */

/* Final clean up */
void clean_mime_action();

/* Save user profile */
void save_mime_action();

/*
* Get default app.desktop for specified file.
* The returned char* should be freed when no longer needed.
*/
G_GNUC_MALLOC char* get_default_app_for_mime_type( const char* mime_type );

/*
* Set default app.desktop for specified file.
* app can be the name of the desktop file or a command line.
*/
void set_default_app_for_mime_type( const char* mime_type, const char* app );


/*
* Get default app.desktop for specified file.
* The returned char** is a NULL-terminated array, and
* should be freed when no longer needed.
*/
G_GNUC_MALLOC char** get_all_apps_for_mime_type( const char* mime_type );

/*
* char** get_apps_for_all_mime_types():
*
* Get all app.desktop files for all mime types.
* The returned string array contains a list of *.desktop file names,
* and should be freed when no longer needed.
*/
G_GNUC_MALLOC  char** get_apps_for_all_mime_types();

/*
* Get Exec command line from app desktop file.
* The returned char* should be freed when no longer needed.
*/
G_GNUC_MALLOC char* get_exec_from_desktop_file( const char* desktop );

/*
* Get displayed name of application from desktop file.
* The returned char* should be freed when no longer needed.
*/
G_GNUC_MALLOC char* get_app_name_from_desktop_file( const char* desktop );

/*
* Get Application icon of from desktop file.
* The returned GdkPixbuf should be freed when no longer needed.
*/
GdkPixbuf* get_app_icon_from_desktop_file( const char* desktop,
                                           gint icon_size );

/*
* Get displayed name of application along with its icon from desktop file.
* The returned name and icon should be freed when no longer needed.
* variable name or icon can be NULL.
*/
void get_app_info_from_desktop_file( const char* desktop,
                                     char** name,
                                     GdkPixbuf** icon,
                                     gint icon_size);

/*
* Parse Exec command line of app desktop file, and translate
* it into a real command which can be passed to system().
* file_list is a null-terminated file list containing full
* paths of the files passed to app.
* returned char* should be freed when no longer needed.
*/
G_GNUC_MALLOC
char* translate_app_exec_to_command_line( const char* exec,
                                          const char* app_desktop,
                                          char** file_list );

G_END_DECLS

#endif

