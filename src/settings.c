#include "settings.h"
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "ptk-file-browser.h"
#include "mime-action.h"

AppSettings appSettings = {0};
const gboolean singleInstanceDefault = TRUE;
const gboolean showHiddenFilesDefault = FALSE;
const gboolean showSidePaneDefault = FALSE;
const int sidePaneModeDefault = FB_SIDE_PANE_BOOKMARKS;
const gboolean showThumbnailDefault = TRUE;
const int maxThumbSizeDefault = 1<<20;
const int openBookmarkMethodDefault = 1;
const int viewModeDefault = FBVM_ICON_VIEW;
const int sortOrderDefault = FB_SORT_BY_NAME;
const int sortTypeDefault = GTK_SORT_ASCENDING;

typedef void (*SettingsParseFunc)( char* line );

void parse_general_settings( char* line )
{
  char* sep = strstr( line, "=" );
  char* name;
  char* value;
  if( !sep )
    return;
  name = line;
  value = sep + 1;
  *sep = '\0';
  /*  g_print("name: %s = value: %s\n", name, value); */
  if( 0 == strcmp( name, "encoding" ) ) {
    strcpy( appSettings.encoding, value );
  }
  else if( 0 == strcmp( name, "showHiddenFiles" ) ) {
    appSettings.showHiddenFiles = atoi(value);
  }
  else if( 0 == strcmp( name, "showSidePane" ) ) {
    appSettings.showSidePane = atoi(value);
  }
  else if( 0 == strcmp( name, "sidePaneMode" ) ) {
    appSettings.sidePaneMode = atoi(value);
  }
  else if( 0 == strcmp( name, "showThumbnail" ) ) {
    appSettings.showThumbnail = atoi(value);
  }
  else if( 0 == strcmp( name, "maxThumbSize" ) ) {
    appSettings.maxThumbSize = atoi(value)<<10;
  }
  else if( 0 == strcmp( name, "viewMode" ) ){
    appSettings.viewMode = atoi(value);
  }
  else if( 0 == strcmp( name, "sortOrder" ) ){
    appSettings.sortOrder = atoi(value);
  }
  else if( 0 == strcmp( name, "sortType" ) ){
    appSettings.sortType = atoi(value);
  }
  else if( 0 == strcmp( name, "openBookmarkMethod" ) ){
    appSettings.openBookmarkMethod = atoi(value);
  }
  else if( 0 == strcmp( name, "iconTheme" ) ){
    if( value && *value )
      appSettings.iconTheme = strdup(value);
  }
  else if( 0 == strcmp( name, "terminal" ) ){
    if( value && *value )
      appSettings.terminal = strdup(value);
  }
  else if( 0 == strcmp( name, "singleInstance" ) ){
    appSettings.singleInstance = atoi(value);
  }
}

void parse_window_state( char* line )
{
  char* sep = strstr( line, "=" );
  char* name;
  char* value;
  int v;
  if( !sep )
    return;
  name = line;
  value = sep + 1;
  *sep = '\0';
  if( 0 == strcmp( name, "splitterPos" ) ) {
    v = atoi(value);
    appSettings.splitterPos = (v > 0 ? v : 160);
  }
  if( 0 == strcmp( name, "width" ) ) {
    v = atoi(value);
    appSettings.width = (v > 0 ? v : 640);
  }
  if( 0 == strcmp( name, "height" ) ) {
    v = atoi(value);
    appSettings.height = (v > 0 ? v : 480);
  }
}


void load_settings()
{
  FILE* file;
  gchar* path;
  char line[1024];
  char* section_name;
  SettingsParseFunc func = NULL;

  /* set default value */
  /* General */
  appSettings.singleInstance = singleInstanceDefault;
  appSettings.encoding[0] = '\0';
  appSettings.showHiddenFiles = showHiddenFilesDefault;
  appSettings.showSidePane = showSidePaneDefault;
  appSettings.sidePaneMode = sidePaneModeDefault;
  appSettings.showThumbnail = showThumbnailDefault;
  appSettings.maxThumbSize = maxThumbSizeDefault;
  appSettings.viewMode = viewModeDefault;
  appSettings.openBookmarkMethod = openBookmarkMethodDefault;
  appSettings.iconTheme = NULL;
  appSettings.terminal = NULL;

  /* Window State */
  appSettings.splitterPos = 160;
  appSettings.width = 640;
  appSettings.height = 480;

  /* load settings */
  path = g_build_filename( g_get_home_dir(), ".pcmanfm/main", NULL );
  file = fopen( path, "r" );
  g_free( path );
  if( file )
  {
    while( fgets( line, sizeof(line), file ) )
    {
      strtok( line, "\r\n" );
      if( ! line[0] )
        continue;
      if( line[0] == '[' ){
        section_name = strtok( line, "]" );
        if( 0 == strcmp( line + 1, "General" ) )
          func = &parse_general_settings;
        else if( 0 == strcmp( line + 1, "Window" ) )
          func = &parse_window_state;
        else
          func = NULL;
        continue;
      }
      if( func )
        (*func)( line );
    }
    fclose( file );
  }

  if( appSettings.encoding[0] ){
    setenv( "G_FILENAME_ENCODING", appSettings.encoding, 1 );
  }

  /* Load bookmarks */
  appSettings.bookmarks = ptk_bookmarks_get();
}


void save_settings()
{
  FILE* file;
  gchar* path;
  /* load settings */
  path = g_build_filename( g_get_home_dir(), ".pcmanfm", NULL );

  if( ! g_file_test( path, G_FILE_TEST_EXISTS ) ){
    g_mkdir( path, 0766 );
  }
  g_chdir( path );
  g_free( path );
  file = fopen( "main", "w" );
  if( file )
  {
    /* General */
    fputs("[General]\n", file);
    if( appSettings.singleInstance != singleInstanceDefault )
      fprintf( file, "singleInstance=%d\n", !!appSettings.singleInstance );
    if( appSettings.encoding[0] )
      fprintf( file, "encoding=%s\n", appSettings.encoding );
    if( appSettings.showHiddenFiles != showHiddenFilesDefault )
      fprintf( file, "showHiddenFiles=%d\n", !!appSettings.showHiddenFiles );
    if( appSettings.showSidePane != showSidePaneDefault )
      fprintf( file, "showSidePane=%d\n", appSettings.showSidePane );
    if( appSettings.sidePaneMode != sidePaneModeDefault )
      fprintf( file, "sidePaneMode=%d\n", appSettings.sidePaneMode );
    if( appSettings.showThumbnail != showThumbnailDefault )
      fprintf( file, "showThumbnail=%d\n", !!appSettings.showThumbnail );
    if( appSettings.maxThumbSize != maxThumbSizeDefault )
      fprintf( file, "maxThumbSize=%d\n", appSettings.maxThumbSize>>10 );
    if( appSettings.viewMode != viewModeDefault )
      fprintf( file, "viewMode=%d\n", appSettings.viewMode );
    if( appSettings.sortOrder != sortOrderDefault )
      fprintf( file, "sortOrder=%d\n", appSettings.sortOrder );
    if( appSettings.sortType != sortTypeDefault )
      fprintf( file, "sortType=%d\n", appSettings.sortType );
    if( appSettings.openBookmarkMethod != openBookmarkMethodDefault )
      fprintf( file, "openBookmarkMethod=%d\n", appSettings.openBookmarkMethod );
    if( appSettings.iconTheme )
      fprintf( file, "iconTheme=%s\n", appSettings.iconTheme );
    if( appSettings.terminal )
      fprintf( file, "terminal=%s\n", appSettings.terminal );

    fputs("[Window]\n", file);
    fprintf( file, "width=%d\n", appSettings.width );
    fprintf( file, "height=%d\n", appSettings.height );
    fprintf( file, "splitterPos=%d\n", appSettings.splitterPos );

    fclose( file );
  }

  /* Save bookmarks */
  ptk_bookmarks_save();

  save_mime_action();
}

void free_settings()
{
  if( appSettings.iconTheme )
    g_free(appSettings.iconTheme);
  if( appSettings.terminal )
    g_free(appSettings.terminal);

  ptk_bookmarks_unref();
}
