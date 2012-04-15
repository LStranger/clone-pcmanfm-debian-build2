/*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "glib-utils.h" /* for g_mkdir_with_parents() */

#include <gtk/gtk.h>
#include "ptk-file-browser.h"

AppSettings appSettings = {0};
/* const gboolean singleInstanceDefault = TRUE; */
const gboolean showHiddenFilesDefault = FALSE;
const gboolean showSidePaneDefault = TRUE;
const int sidePaneModeDefault = FB_SIDE_PANE_BOOKMARKS;
const gboolean showThumbnailDefault = TRUE;
const int maxThumbSizeDefault = 1 << 20;
const int bigIconSizeDefault = 48;
const int smallIconSizeDefault = 20;
const int openBookmarkMethodDefault = 1;
const int viewModeDefault = FBVM_ICON_VIEW;
const int sortOrderDefault = FB_SORT_BY_NAME;
const int sortTypeDefault = GTK_SORT_ASCENDING;

const gboolean showDesktopDefault = FALSE;
const gboolean showWallpaperDefault = FALSE;
const GdkColor desktopBg1Default={0};
const GdkColor desktopBg2Default={0};
const GdkColor desktopTextDefault={0, 65535, 65535, 65535};

typedef void ( *SettingsParseFunc ) ( char* line );

static void color_from_str( GdkColor* ret, const char* value );
static void save_color( FILE* file, const char* name,
                 GdkColor* color );

static void parse_general_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "encoding" ) )
        strcpy( appSettings.encoding, value );
    else if ( 0 == strcmp( name, "showHiddenFiles" ) )
        appSettings.showHiddenFiles = atoi( value );
    else if ( 0 == strcmp( name, "showSidePane" ) )
        appSettings.showSidePane = atoi( value );
    else if ( 0 == strcmp( name, "sidePaneMode" ) )
        appSettings.sidePaneMode = atoi( value );
    else if ( 0 == strcmp( name, "showThumbnail" ) )
        appSettings.showThumbnail = atoi( value );
    else if ( 0 == strcmp( name, "maxThumbSize" ) )
        appSettings.maxThumbSize = atoi( value ) << 10;
    else if ( 0 == strcmp( name, "bigIconSize" ) )
    {
        appSettings.bigIconSize = atoi( value );
        if( appSettings.bigIconSize <= 0 || appSettings.bigIconSize > 128 )
            appSettings.bigIconSize = bigIconSizeDefault;
    }
    else if ( 0 == strcmp( name, "smallIconSize" ) )
    {
        appSettings.smallIconSize = atoi( value );
        if( appSettings.smallIconSize <= 0 || appSettings.smallIconSize > 128 )
            appSettings.smallIconSize = smallIconSizeDefault;
    }
    else if ( 0 == strcmp( name, "viewMode" ) )
        appSettings.viewMode = atoi( value );
    else if ( 0 == strcmp( name, "sortOrder" ) )
        appSettings.sortOrder = atoi( value );
    else if ( 0 == strcmp( name, "sortType" ) )
        appSettings.sortType = atoi( value );
    else if ( 0 == strcmp( name, "openBookmarkMethod" ) )
        appSettings.openBookmarkMethod = atoi( value );
/*
    else if ( 0 == strcmp( name, "iconTheme" ) )
    {
        if ( value && *value )
            appSettings.iconTheme = strdup( value );
    }
*/
    else if ( 0 == strcmp( name, "terminal" ) )
    {
        if ( value && *value )
            appSettings.terminal = strdup( value );
    }
    /*
    else if ( 0 == strcmp( name, "singleInstance" ) )
        appSettings.singleInstance = atoi( value );
    */
}

static void color_from_str( GdkColor* ret, const char* value )
{
    sscanf( value, "%d,%d,%d",
            &ret->red, &ret->green, &ret->blue );
}

static void save_color( FILE* file, const char* name, GdkColor* color )
{
    fprintf( file, "%s=%d,%d,%d\n", name,
             color->red, color->green, color->blue );
}

static void parse_window_state( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    int v;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "splitterPos" ) )
    {
        v = atoi( value );
        appSettings.splitterPos = ( v > 0 ? v : 160 );
    }
    if ( 0 == strcmp( name, "width" ) )
    {
        v = atoi( value );
        appSettings.width = ( v > 0 ? v : 640 );
    }
    if ( 0 == strcmp( name, "height" ) )
    {
        v = atoi( value );
        appSettings.height = ( v > 0 ? v : 480 );
    }
    if ( 0 == strcmp( name, "maximized" ) )
    {
        appSettings.maximized = atoi( value );
    }
}

static void parse_desktop_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';

    if ( 0 == strcmp( name, "showDesktop" ) )
        appSettings.showDesktop = atoi( value );
    else if ( 0 == strcmp( name, "showWallpaper" ) )
        appSettings.showWallpaper = atoi( value );
    else if ( 0 == strcmp( name, "wallpaper" ) )
        appSettings.wallpaper = g_strdup( value );
    else if ( 0 == strcmp( name, "Bg1" ) )
        color_from_str( &appSettings.desktopBg1, value );
    else if ( 0 == strcmp( name, "Bg2" ) )
        color_from_str( &appSettings.desktopBg2, value );
    else if ( 0 == strcmp( name, "Text" ) )
        color_from_str( &appSettings.desktopText, value );
}

void load_settings()
{
    FILE * file;
    gchar* path;
    char line[ 1024 ];
    char* section_name;
    SettingsParseFunc func = NULL;

    /* set default value */
    /* General */
    /* appSettings.singleInstance = singleInstanceDefault; */
    appSettings.showDesktop = showDesktopDefault;
    appSettings.showWallpaper = showWallpaperDefault;
    appSettings.wallpaper = NULL;
    appSettings.desktopBg1 = desktopBg1Default;
    appSettings.desktopBg2 = desktopBg2Default;
    appSettings.desktopText = desktopTextDefault;

    appSettings.encoding[ 0 ] = '\0';
    appSettings.showHiddenFiles = showHiddenFilesDefault;
    appSettings.showSidePane = showSidePaneDefault;
    appSettings.sidePaneMode = sidePaneModeDefault;
    appSettings.showThumbnail = showThumbnailDefault;
    appSettings.maxThumbSize = maxThumbSizeDefault;
    appSettings.bigIconSize = bigIconSizeDefault;
    appSettings.smallIconSize = smallIconSizeDefault;
    appSettings.viewMode = viewModeDefault;
    appSettings.openBookmarkMethod = openBookmarkMethodDefault;
    /* appSettings.iconTheme = NULL; */
    appSettings.terminal = NULL;

    /* Window State */
    appSettings.splitterPos = 160;
    appSettings.width = 640;
    appSettings.height = 480;

    /* load settings */
    path = g_build_filename( g_get_user_config_dir(), "pcmanfm/main", NULL );
    file = fopen( path, "r" );
    g_free( path );
    if ( file )
    {
        while ( fgets( line, sizeof( line ), file ) )
        {
            strtok( line, "\r\n" );
            if ( ! line[ 0 ] )
                continue;
            if ( line[ 0 ] == '[' )
            {
                section_name = strtok( line, "]" );
                if ( 0 == strcmp( line + 1, "General" ) )
                    func = &parse_general_settings;
                else if ( 0 == strcmp( line + 1, "Window" ) )
                    func = &parse_window_state;
                else if ( 0 == strcmp( line + 1, "Desktop" ) )
                    func = &parse_desktop_settings;
                else
                    func = NULL;
                continue;
            }
            if ( func )
                ( *func ) ( line );
        }
        fclose( file );
    }

    if ( appSettings.encoding[ 0 ] )
    {
        setenv( "G_FILENAME_ENCODING", appSettings.encoding, 1 );
    }

    /* Load bookmarks */
    appSettings.bookmarks = ptk_bookmarks_get();
}


void save_settings()
{
    FILE * file;
    gchar* path;
    /* load settings */
    path = g_build_filename( g_get_user_config_dir(), "pcmanfm", NULL );

    if ( ! g_file_test( path, G_FILE_TEST_EXISTS ) )
        g_mkdir_with_parents( path, 0766 );

    chdir( path );
    g_free( path );
    file = fopen( "main", "w" );
    if ( file )
    {
        /* General */
        fputs( "[General]\n", file );
        /*
        if ( appSettings.singleInstance != singleInstanceDefault )
            fprintf( file, "singleInstance=%d\n", !!appSettings.singleInstance );
        */
        if ( appSettings.encoding[ 0 ] )
            fprintf( file, "encoding=%s\n", appSettings.encoding );
        if ( appSettings.showHiddenFiles != showHiddenFilesDefault )
            fprintf( file, "showHiddenFiles=%d\n", !!appSettings.showHiddenFiles );
        if ( appSettings.showSidePane != showSidePaneDefault )
            fprintf( file, "showSidePane=%d\n", appSettings.showSidePane );
        if ( appSettings.sidePaneMode != sidePaneModeDefault )
            fprintf( file, "sidePaneMode=%d\n", appSettings.sidePaneMode );
        if ( appSettings.showThumbnail != showThumbnailDefault )
            fprintf( file, "showThumbnail=%d\n", !!appSettings.showThumbnail );
        if ( appSettings.maxThumbSize != maxThumbSizeDefault )
            fprintf( file, "maxThumbSize=%d\n", appSettings.maxThumbSize >> 10 );
        if ( appSettings.bigIconSize != bigIconSizeDefault )
            fprintf( file, "bigIconSize=%d\n", appSettings.bigIconSize );
        if ( appSettings.smallIconSize != smallIconSizeDefault )
            fprintf( file, "smallIconSize=%d\n", appSettings.smallIconSize );
        if ( appSettings.viewMode != viewModeDefault )
            fprintf( file, "viewMode=%d\n", appSettings.viewMode );
        if ( appSettings.sortOrder != sortOrderDefault )
            fprintf( file, "sortOrder=%d\n", appSettings.sortOrder );
        if ( appSettings.sortType != sortTypeDefault )
            fprintf( file, "sortType=%d\n", appSettings.sortType );
        if ( appSettings.openBookmarkMethod != openBookmarkMethodDefault )
            fprintf( file, "openBookmarkMethod=%d\n", appSettings.openBookmarkMethod );
        /*
        if ( appSettings.iconTheme )
            fprintf( file, "iconTheme=%s\n", appSettings.iconTheme );
        */
        if ( appSettings.terminal )
            fprintf( file, "terminal=%s\n", appSettings.terminal );

        fputs( "\n[Window]\n", file );
        fprintf( file, "width=%d\n", appSettings.width );
        fprintf( file, "height=%d\n", appSettings.height );
        fprintf( file, "splitterPos=%d\n", appSettings.splitterPos );
        fprintf( file, "maximized=%d\n", appSettings.maximized );

        /* Desktop */
        fputs( "\n[Desktop]\n", file );
        if ( appSettings.showDesktop != showDesktopDefault )
            fprintf( file, "showDesktop=%d\n", !!appSettings.showDesktop );
        if ( appSettings.showWallpaper != showWallpaperDefault )
            fprintf( file, "showWallpaper=%d\n", !!appSettings.showWallpaper );
        if ( appSettings.wallpaper && appSettings.wallpaper[ 0 ] )
            fprintf( file, "wallpaper=%s\n", appSettings.wallpaper );
        if ( ! gdk_color_equal( &appSettings.desktopBg1,
               &desktopBg1Default ) )
            save_color( file, "Bg1",
                        &appSettings.desktopBg1 );
        if ( ! gdk_color_equal( &appSettings.desktopBg2,
               &desktopBg2Default ) )
            save_color( file, "Bg2",
                        &appSettings.desktopBg2 );
        if ( ! gdk_color_equal( &appSettings.desktopText,
               &desktopTextDefault ) )
            save_color( file, "Text",
                        &appSettings.desktopText );
        fclose( file );
    }

    /* Save bookmarks */
    ptk_bookmarks_save();

    /* FIXME: save_mime_action(); */
}

void free_settings()
{
/*
    if ( appSettings.iconTheme )
        g_free( appSettings.iconTheme );
*/
    g_free( appSettings.terminal );
    g_free( appSettings.wallpaper );

    ptk_bookmarks_unref();
}
