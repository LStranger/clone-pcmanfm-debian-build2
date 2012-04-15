/*
*  C Implementation: mimeaction
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "mime-action.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "xdgmime.h"

const char mime_cache_group[] = "MIME Cache";

static GKeyFile *mime_cache = NULL;
static GKeyFile *user_profile = NULL;

static void init_mime_cache()
{
  gchar* full_path = g_build_filename( "/usr/share", /* FIXME */
                                       "applications/mimeinfo.cache",
                                       NULL );
  mime_cache = g_key_file_new();
  g_key_file_load_from_file( mime_cache,
                             full_path,
                             G_KEY_FILE_NONE, NULL );
  g_free( full_path );

  full_path = g_build_filename( g_get_home_dir(),
                                ".pcmanfm/mime_info",
                                NULL );

  user_profile = g_key_file_new();
  g_key_file_load_from_file( user_profile,
                             full_path,
                             G_KEY_FILE_NONE, NULL );
  g_free( full_path );
}

/* Final clean up */
void clean_mime_action()
{
  if( mime_cache )
    g_key_file_free( mime_cache );
  if( user_profile )
    g_key_file_free( user_profile );
}

/* Save user profile */
void save_mime_action()
{
  gchar* full_path;
  gchar* data;
  gsize len;
  int file;
  if( user_profile ) {
    if( data = g_key_file_to_data ( user_profile, &len, NULL ) )
    {
      full_path = g_build_filename( g_get_home_dir(),
                                    ".pcmanfm/mime_info",
                                    NULL );

      file = creat( full_path, S_IWUSR|S_IRUSR|S_IRGRP );
      g_free( full_path );

      if( file != -1 )
      {
        write( file, data, len );
        close( file );
      }
      else
        g_print("open file failed! %d\n", errno);

      g_free( data );
    }
  }
}

/*
* Get default app.desktop for specified file.
* The returned char* should be freed when no longer needed.
*/
char* get_default_app_for_mime_type( const char* mime_type )
{
  gchar *val, *sep;
  int i;
  GKeyFile* profiles[2];
  if( ! mime_cache )
    init_mime_cache();

  profiles[0] = user_profile;
  profiles[1] = mime_cache;

  for( i = 0; i < 2; ++i ) {
    val = g_key_file_get_string ( profiles[i], mime_cache_group,
                                  mime_type, NULL);
    if( val ){
      if( (sep = strchr( val, ';' )) ) {
        *sep = '\0';
      }
      return val;
    }
  }
  return NULL;
}

/*
* Set default app.desktop for specified file.
* app can be the name of the desktop file or a command line.
*/
void set_default_app_for_mime_type( const char* mime_type, const char* app )
{
  gchar **vals;
  gchar **new_vals;
  int i;
  gsize len;
  if( ! user_profile )
    init_mime_cache();

  vals = g_key_file_get_string_list ( user_profile, mime_cache_group,
                                      mime_type, &len, NULL);

  if( !vals ){
    g_key_file_set_string( user_profile, mime_cache_group,
                           mime_type, app );
    return;
  }

  new_vals = g_new0( gchar*, len + 2 );
  new_vals[0] = g_strdup( app );
  if( vals ){
    for( i = 0; i < len; ++i ) {
      if( strcmp( vals[i], app ) ) {
        new_vals[ i + 1 ] = vals[i];
      }
      else {
        g_free( vals[i] );
      }
      vals[i] = NULL;
    }
    g_strfreev( vals );
  }
  g_key_file_set_string_list( user_profile, mime_cache_group, mime_type,
                              new_vals, g_strv_length(new_vals) );
  g_strfreev( new_vals );
}

/*
* Join two string vector containing app lists to generate a new one.
* Duplicated app will be removed.
*/
static char** join_app_lists( char** list1, gsize len1,
                              char** list2, gsize len2 )
{
  gchar **ret = NULL;
  int i, j, k;

  if( len1 > 0 || len2 > 0 )
    ret = g_new0( char*, len1 + len2 + 1 );
  for( i = 0; i < len1; ++i ) {
    ret[ i ] = g_strdup( list1[i] );
  }
  for( j = 0, k = 0; j < len2; ++j ) {
    for( i = 0; i < len1; ++i ) {
      if( 0 == strcmp( ret[ i ], list2[j] ) )
        break;
    }
    if( i >= len1 ) {
      ret[ len1 + k ] = g_strdup( list2[ j ] );
      ++k;
    }
  }
  return ret;
}

/*
* Get all app.desktop for specified file.
* The returned char** is a NULL-terminated array, and
* should be freed when no longer needed.
*/
char** get_all_apps_for_mime_type( const char* mime_type )
{
  gchar **vals;
  gsize len;
  gchar **text_vals;
  gsize text_len;
  gchar **tmp_vals;
  gsize tmp_len;
  gchar **ret = NULL;
  gboolean is_text;
  int i, j, k;
  GKeyFile* profiles[2];
  char** apps[2];

  if( ! mime_cache )
    init_mime_cache();

  profiles[0] = user_profile;
  profiles[1] = mime_cache;

  is_text = xdg_mime_mime_type_subclass(mime_type, XDG_MIME_TYPE_PLAIN_TEXT);

  for( i = 0; i < 2; ++i ) {
    len = 0;
    vals = g_key_file_get_string_list ( profiles[i], mime_cache_group,
                                        mime_type, &len, NULL );

    /* Special process for text files */
    if( is_text )
    {
      text_len = 0;
      text_vals = g_key_file_get_string_list ( profiles[i], mime_cache_group,
                                               "text/plain", &text_len, NULL );
      tmp_vals = join_app_lists( vals, len, text_vals, text_len );
      if( vals )
        g_strfreev( vals );
      if( text_vals )
        g_strfreev( text_vals );
      vals = tmp_vals;
    }
    apps[ i ] = vals;
  }
  len = apps[0] ? g_strv_length(apps[0]) : 0;
  tmp_len = apps[1] ? g_strv_length(apps[1]) : 0;
  ret = join_app_lists( apps[0], len,
                        apps[1], tmp_len);
  if( apps[0] )
    g_strfreev( apps[0] );
  if( apps[1] )
    g_strfreev( apps[1] );
  return ret;
}

/*
* Get Exec command line from app desktop file.
* The returned char* should be freed when no longer needed.
*/
char* get_exec_from_desktop_file( const char* desktop )
{
  GKeyFile *desktop_file = g_key_file_new();
  char* ret = NULL;
  char* full_path;

  if( 0 == strncmp(desktop, "kde-", 4) ){ /* KDE application */
    full_path = g_build_filename( "/usr/share", /* FIXME */
                                  "applications/kde",
                                  (desktop + 4), NULL );
  }
  else
  {
    full_path = g_build_filename( "/usr/share", /* FIXME */
                                  "applications",
                                  desktop, NULL );
  }
  /*  g_print( "desktop: %s\npath: %s\n", desktop, full_path ); */

  if( g_key_file_load_from_file( desktop_file,
                                 full_path, G_KEY_FILE_NONE, NULL ) )
  {
    ret = g_key_file_get_string ( desktop_file, "Desktop Entry",
                                "Exec", NULL);
  }
  g_free( full_path );
  g_key_file_free(desktop_file);
  return ret;
}

/*
* Get displayed name of application from desktop file.
* The returned char* should be freed when no longer needed.
*/
char* get_app_name_from_desktop_file( const char* desktop )
{
  char* name = NULL;
  get_app_info_from_desktop_file( desktop, &name, NULL, 0 );
  return name;
}

/*
* Get Application icon of from desktop file.
* The returned char* should be freed when no longer needed.
*/
GdkPixbuf* get_app_icon_from_desktop_file( const char* desktop,
                                           gint icon_size)
{
  GdkPixbuf* icon = NULL;
  get_app_info_from_desktop_file( desktop, NULL, &icon, icon_size );
  return icon;
}

void get_app_info_from_desktop_file( const char* desktop,
                                     char** name,
                                     GdkPixbuf** icon,
                                     gint icon_size )
{
  GKeyFile *desktop_file = g_key_file_new();
  gchar* full_path;
  char* icon_name;
  char* tmp;

  if( name )
    *name = NULL;
  if( icon )
    *icon = NULL;

  if( 0 == strncmp(desktop, "kde-", 4) ){ /* KDE application */
    full_path = g_build_filename( "/usr/share", /* FIXME */
                                  "applications/kde",
                                  (desktop + 4), NULL );
  }
  else
  {
    full_path = g_build_filename( "/usr/share", /* FIXME */
                                    "applications",
                                    desktop, NULL );
  }
  if( g_key_file_load_from_file( desktop_file,
                                 full_path, G_KEY_FILE_NONE, NULL ) )
  {
    g_free( full_path );

    if( name )
      *name = g_key_file_get_locale_string ( desktop_file,
                                             "Desktop Entry",
                                             "Name", NULL, NULL);
    if( icon ) {
      icon_name = g_key_file_get_string ( desktop_file,
                                          "Desktop Entry",
                                          "Icon", NULL );
      if( icon_name ) { /* Has icon */
        if( icon_name[0] == '/' ) { /* Full path */
          full_path = icon_name;
          *icon = gdk_pixbuf_new_from_file_at_scale ( full_path,
                                          icon_size, icon_size, TRUE, NULL );
        }
        else {
          *icon = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default(),
                                             icon_name, icon_size, 0, NULL );
          if( !*icon && (tmp = strchr( icon_name, '.' )) ){
            *tmp = '\0';
            *icon = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default(),
                                               icon_name, icon_size, 0, NULL);
          }
        }
        g_free( icon_name );
      }
    }

    /* Fallback */
    if( icon && !*icon ) {
      icon_name = g_strdup( desktop );
      if( tmp = strchr( icon_name, '.' ) )
        *tmp = '\0';
      *icon = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default(),
                                         icon_name, icon_size, 0, NULL);
      g_free( icon_name );
    }

  }
  g_key_file_free(desktop_file);
}

/*
* Parse Exec command line of app desktop file, and translate
* it into a real command which can be passed to g_spawn_command_line_async().
* file_list is a null-terminated file list containing full
* paths of the files passed to app.
* returned char* should be freed when no longer needed.
*/
char* translate_app_exec_to_command_line( const char* exec,
                                          const char* app_desktop,
                                          char** file_list )
{
  const char* pexec = exec;
  char** ufile;
  char* file;
  gchar *tmp, *tmp2;
  GString* cmd = g_string_new("");
  gboolean add_files = FALSE;
  for( ; *pexec; ++pexec )
  {
    if( *pexec == '%' )
    {
      ++pexec;
      switch( *pexec )
      {
        case 'U':
          for( ufile = file_list; *ufile; ++ufile ){
            file = g_filename_from_utf8 ( *ufile, -1, NULL, NULL, NULL );
            tmp = g_filename_to_uri( file, NULL, NULL );
            g_free( file );
            file = g_shell_quote( tmp );
            g_free( tmp );
            g_string_append( cmd, file );
            g_string_append_c( cmd, ' ' );
            g_free( file );
          }
          add_files = TRUE;
          break;
        case 'u':
          file = g_filename_from_utf8 ( file_list[0], -1, NULL, NULL, NULL );
          tmp = g_filename_to_uri( file, NULL, NULL );
          g_free( file );
          file = g_shell_quote( tmp );
          g_free( tmp );
          g_string_append( cmd, file );
          g_free( file );
          add_files = TRUE;
          break;
        case 'F':
        case 'N':
          for( ufile = file_list; *ufile; ++ufile ){
            file = g_filename_from_utf8 ( *ufile, -1, NULL, NULL, NULL );
            tmp = g_shell_quote( file );
            g_free( file );
            g_string_append( cmd, tmp );
            g_string_append_c( cmd, ' ' );
            g_free( tmp );
          }
          add_files = TRUE;
          break;
        case 'f':
        case 'n':
          file = g_filename_from_utf8 ( file_list[0], -1, NULL, NULL, NULL );
          tmp = g_shell_quote( file );
          g_free( file );
          g_string_append( cmd, tmp );
          g_free( tmp );
          add_files = TRUE;
          break;
        case 'D':
          for( ufile = file_list; *ufile; ++ufile ){
            tmp = g_path_get_dirname( *ufile );
            file = g_filename_from_utf8 ( tmp, -1, NULL, NULL, NULL );
            g_free( tmp );
            tmp2 = g_shell_quote( file );
            g_free(file);
            g_string_append( cmd, tmp2 );
            g_string_append_c( cmd, ' ' );
            g_free( tmp2 );
          }
          add_files = TRUE;
          break;
        case 'd':
          tmp = g_path_get_dirname( file_list[0] );
          file = g_filename_from_utf8 ( tmp, -1, NULL, NULL, NULL );
          g_free( tmp );
          tmp2 = g_shell_quote( file );
          g_free( file );
          g_string_append( cmd, tmp2 );
          g_free( tmp2 );
          add_files = TRUE;
          break;
        case 'c':
          tmp = get_app_name_from_desktop_file(app_desktop);
          g_string_append( cmd, tmp );
          g_free(tmp);
          break;
        case 'i':
          /* Add icon name */
          break;
        case 'k':
          /* Location of the desktop file */
          break;
        case 'v':
          /* Device name */
          break;
        case '%':
          g_string_append_c ( cmd, '%' );
          break;
        case '\0':
          goto _finish;
          break;
      }
    }
    else  /* not % escaped part */
    {
      g_string_append_c ( cmd, *pexec );
    }
  }
_finish:
  if( ! add_files ) {
    g_string_append_c ( cmd, ' ' );
    for( ufile = file_list; *ufile; ++ufile ){
      g_print( "ufile = %s\n", *ufile );
      file = g_filename_from_utf8 ( *ufile, -1, NULL, NULL, NULL );
      g_print( "file = %s\n", file );
      tmp = g_shell_quote( file );
      g_free( file );
      g_string_append( cmd, tmp );
      g_string_append_c( cmd, ' ' );
      g_free( tmp );
    }
  }
  return g_string_free( cmd, FALSE );
}

/*
* char** get_apps_for_all_mime_types():
*
* Get all app.desktop files for all mime types.
* The returned string array contains a list of *.desktop file names,
* and should be freed when no longer needed.
*/

static void hash_to_strv (gpointer key, gpointer value, gpointer user_data)
{
  char***all_apps = (char***)user_data;
  **all_apps = (char*)key;
  ++*all_apps;
}

char** get_apps_for_all_mime_types()
{
  char** all_apps = NULL;
  GHashTable* hash;
  gchar **keys;
  gchar **key;
  gchar **apps;
  gchar **app;
  int len = 0;
  if( ! mime_cache )
    init_mime_cache();

  hash = g_hash_table_new(g_str_hash, g_str_equal);

  keys = g_key_file_get_keys( mime_cache, mime_cache_group, NULL, NULL );
  /* "MIME Cache" cannot be found. */
  if (NULL == keys)
    goto just_free_keys;

  for( key = keys; *key; ++key )
  {
    apps = g_key_file_get_string_list ( mime_cache, mime_cache_group, *key,
                                        NULL, NULL );
    if( !apps )
      continue;
    for( app = apps; *app; ++app )
    {
      /* duplicated */
      if( g_hash_table_lookup( hash, *app ) )
        continue;
      g_hash_table_insert( hash, g_strdup( *app ), TRUE );
      ++len;
    }
    g_strfreev( apps );
  }

just_free_keys:
  g_strfreev( keys );

  app = all_apps = g_new( char*, len + 1 );
  g_hash_table_foreach( hash, hash_to_strv, &app );
  all_apps[ len ] = NULL;

  g_hash_table_destroy( hash );

  return all_apps;
}

