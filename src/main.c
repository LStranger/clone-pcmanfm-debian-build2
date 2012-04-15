/*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* turn on to debug GDK_THREADS_ENTER/GDK_THREADS_LEAVE related deadlocks */
#undef _DEBUG_THREAD

#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <stdlib.h>
#include <string.h>

/* socket is used to keep single instance */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "main-window.h"

#include "vfs-file-info.h"
#include "vfs-mime-type.h"
#include "vfs-app-desktop.h"

#include "vfs-file-monitor.h"
#include "vfs-volume.h"

#include "ptk-utils.h"
#include "app-chooser-dialog.h"

#include "settings.h"

static int sock;
GIOChannel* io_channel = NULL;

static char* default_files[2] = {NULL, NULL};
static char** files = NULL;
static gboolean no_desktop = FALSE;
static gboolean old_show_desktop = FALSE;

static char* mount = NULL;
static char* umount = NULL;
static char* eject = NULL;

static GOptionEntry opt_entries[] =
{
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("Don't show desktop icons."), NULL },
#ifdef HAVE_HAL
/*
    { "mount", 'm', 0, G_OPTION_ARG_STRING, &mount, N_("Mount specified device"), "device [mount options]" },
    { "umount", 'u', 0, G_OPTION_ARG_STRING, &umount, N_("Unmount specified device"), "device" },
    { "eject", 'e', 0, G_OPTION_ARG_STRING, &eject, N_("Eject specified device"), "device" },
*/
#endif
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, N_("[FILE1, FILE2,...]")},
    { NULL }
};

static FMMainWindow* create_main_window();

static void open_file( const char* path );

static gboolean on_socket_event( GIOChannel* ioc,
                                 GIOCondition cond,
                                 gpointer data )
{
    int client, r;
    socklen_t addr_len = 0;
    GString* paths;
    struct sockaddr_un client_addr =
        {
            0
        };
    FMMainWindow* main_window;
    static char buf[ 256 ];

    if ( cond & G_IO_IN )
    {
        client = accept( g_io_channel_unix_get_fd( ioc ), (struct sockaddr *)&client_addr, &addr_len );
        if ( client != -1 )
        {
            char *path, *end;
            paths = g_string_new_len( NULL, 512 );
            while( (r = read( client, buf, sizeof(buf) )) > 0 )
                g_string_append_len( paths, buf, r);
            shutdown( client, 2 );
            close( client );

            for( end = path = paths->str; path && *path;  )
            {
                end = strchr(path, '\n');
                if( end )
                    *end = '\0';

                GDK_THREADS_ENTER();

                if ( g_file_test( path, G_FILE_TEST_IS_DIR ) )
                {
                    main_window = create_main_window();
                    fm_main_window_add_new_tab( main_window, path,
                                                appSettings.showSidePane,
                                                appSettings.sidePaneMode );
                    gtk_window_present( GTK_WINDOW(main_window) );
                }
                else
                {
                    open_file( path );
                }
                GDK_THREADS_LEAVE();

                path = end ? (end + 1) : NULL;
            }
            g_string_free( paths, TRUE );
        }
    }
    return TRUE;
}

static void get_socket_name( char* buf, int len )
{
    char* dpy = gdk_get_display();
    g_snprintf( buf, len, "/tmp/.pcmanfm-socket%s-%s", dpy, g_get_user_name() );
    g_free( dpy );
}

static void single_instance_init()
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;
    int reuse;

    if ( ( sock = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
    {
        ret = 1;
        goto _exit;
    }

    addr.sun_family = AF_UNIX;
    get_socket_name( addr.sun_path, sizeof( addr.sun_path ) );
#ifdef SUN_LEN
    addr_len = SUN_LEN( &addr );
#else

    addr_len = strlen( addr.sun_path ) + sizeof( addr.sun_family );
#endif

    if ( connect( sock, ( struct sockaddr* ) & addr, addr_len ) == 0 )
    {
        char** file;
        /* connected successfully */
        if( ! files )
        {
            files = default_files;
            files[0] = g_get_home_dir();
        }
        for( file = files; *file; ++file )
        {
            char* path = *file;
            if( g_str_has_prefix( path, "file:" ) ) /* It's a URI */
            {
                path = g_filename_from_uri( path, NULL, NULL );
                g_free( *file );
                *file = path;
            }
            write( sock, path, strlen( path ) );
            write( sock, "\n", 1 );
        }
        shutdown( sock, 2 );
        close( sock );
        gdk_notify_startup_complete();

        ret = 0;
        goto _exit;
    }

    /* There is no existing server. So, we are in the first instance. */
    unlink( addr.sun_path ); /* delete old socket file if it exists. */
    reuse = 1;
    ret = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    if ( bind( sock, ( struct sockaddr* ) & addr, addr_len ) == -1 )
    {
        ret = 1;
        goto _exit;
    }

    io_channel = g_io_channel_unix_new( sock );
    g_io_channel_set_encoding( io_channel, NULL, NULL );
    g_io_channel_set_buffered( io_channel, FALSE );

    g_io_add_watch( io_channel, G_IO_IN,
                    ( GIOFunc ) on_socket_event, NULL );

    if ( listen( sock, 5 ) == -1 )
    {
        ret = 1;
        goto _exit;
    }
    return ;

_exit:
    exit( ret );
}

static void single_instance_finalize()
{
    char lock_file[ 256 ];

    shutdown( sock, 2 );
    g_io_channel_unref( io_channel );
    close( sock );

    get_socket_name( lock_file, sizeof( lock_file ) );
    unlink( lock_file );
}

FMMainWindow* create_main_window()
{
    FMMainWindow * main_window = FM_MAIN_WINDOW(fm_main_window_new ());
    gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                 appSettings.width, appSettings.height );
    if ( appSettings.maximized )
    {
        gtk_window_maximize( GTK_WINDOW( main_window ) );
    }
    gtk_widget_show ( GTK_WIDGET( main_window ) );
    return main_window;
}

static void check_icon_theme()
{
    GtkSettings * settings;
    char* theme;
    const char* title = N_( "GTK+ icon theme is not properly set" );
    const char* error_msg =
        N_( "<big><b>%s</b></big>\n\n"
            "This usually means you don't have an XSETTINGS manager running.  "
            "Desktop environment like GNOME or XFCE automatically execute their "
            "XSETTING managers like gnome-settings-daemon or xfce-mcs-manager.\n\n"
            "<b>If you don't use these desktop environments, "
            "you have two choices:\n"
            "1. run an XSETTINGS manager, or\n"
            "2. simply specify an icon theme in ~/.gtkrc-2.0.</b>\n"
            "For example to use the Tango icon theme add a line:\n"
            "<i><b>gtk-icon-theme-name=\"Tango\"</b></i> in your ~/.gtkrc-2.0. (create it if no such file)\n\n"
            "<b>NOTICE: The icon theme you choose should be compatible with GNOME, "
            "or the file icons cannot be displayed correctly.</b>  "
            "Due to the differences in icon naming of GNOME and KDE, KDE themes cannot be used.  "
            "Currently there is no standard for this, but it will be solved by freedesktop.org in the future." );
    settings = gtk_settings_get_default();
    g_object_get( settings, "gtk-icon-theme-name", &theme, NULL );

    /* No icon theme available */
    if ( !theme || !*theme || 0 == strcmp( theme, "hicolor" ) )
    {
        GtkWidget * dlg;
        dlg = gtk_message_dialog_new_with_markup( NULL,
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_OK,
                                                  _( error_msg ), _( title ) );
        gtk_window_set_title( GTK_WINDOW( dlg ), _( title ) );
        gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
    }
    g_free( theme );
}

#ifdef _DEBUG_THREAD

G_LOCK_DEFINE(gdk_lock);
void debug_gdk_threads_enter (const char* message)
{
    g_debug( "Thread %p tries to get GDK lock: %s", g_thread_self (), message );
    G_LOCK(gdk_lock);
    g_debug( "Thread %p got GDK lock: %s", g_thread_self (), message );
}

static void _debug_gdk_threads_enter ()
{
    debug_gdk_threads_enter( "called from GTK+ internal" );
}

void debug_gdk_threads_leave( const char* message )
{
    g_debug( "Thread %p tries to release GDK lock: %s", g_thread_self (), message );
    G_LOCK(gdk_lock);
    g_debug( "Thread %p released GDK lock: %s", g_thread_self (), message );
}

static void _debug_gdk_threads_leave()
{
    debug_gdk_threads_leave( "called from GTK+ internal" );
}
#endif

static void init_folder()
{
    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size( appSettings.bigIconSize,
                                 appSettings.smallIconSize );
    vfs_file_info_set_thumbnail_size( appSettings.bigIconSize,
                                      appSettings.smallIconSize );

    check_icon_theme();
}

#ifdef HAVE_HAL
static int handle_mount( char** argv )
{
    char* self = argv[0];
    vfs_volume_init();
    if( mount )
    {

    }
    else if( umount )
    {

    }
    else /* if( eject ) */
    {

    }
    vfs_volume_finalize();
}
#endif

int
main ( int argc, char *argv[] )
{
    FMMainWindow * main_window = NULL;
    GtkSettings *settings;
    GOptionContext *context;
    char** file;
    GError* err = NULL;

#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    /* Initialize multithreading
         No matter we use threads or not, it's safer to initialize this earlier. */
#ifdef _DEBUG_THREAD
    gdk_threads_set_lock_functions(_debug_gdk_threads_enter, _debug_gdk_threads_leave);
#endif
    g_thread_init( NULL );
    gdk_threads_init ();

    /* parse command line arguments */
    context = g_option_context_new ("");
    g_option_context_add_main_entries (context, opt_entries, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    /* gtk_init( &argc, &argv ); is not needed if g_option_context_parse is called */
    if( G_UNLIKELY( ! g_option_context_parse (context, &argc, &argv, &err) ) )
    {
        g_print( _("Error: %s\n"), err->message );
        g_error_free( err );
        return 1;
    }
    g_option_context_free( context );

#if HAVE_HAL
    /* If the user wants to mount/umount/eject a device */
    if( G_UNLIKELY( mount || umount || eject ) )
        return handle_mount( argv );
#endif

    load_settings();    /* load config file */

    /* if ( appSettings.singleInstance ) */
    single_instance_init(); /* ensure there is only one instance of pcmanfm. */

    /* temporarily turn off desktop if needed */
    if( G_LIKELY( no_desktop ) )
    {
        /* No matter what the value of showDesktop is, we don't showdesktop icons
         * if --no-desktop argument is passed by the users. */
        old_show_desktop = appSettings.showDesktop;
        /* This config value will be restored before saving config files, if needed. */
        appSettings.showDesktop = FALSE;
    }

    /* If no command line arguments are specified, open home dir by defualt. */
    if( G_LIKELY( ! files ) )
    {
        files = default_files;
        /* only show home dir when desktop icons is not used. */
        if( ! appSettings.showDesktop )
            files[0] = g_get_home_dir();
    }

    vfs_file_info_set_utf8_filename( g_get_filename_charsets( NULL ) );

    if( G_UNLIKELY( ! vfs_file_monitor_init() ) )
    {
        ptk_show_error( NULL, _("Error"), _("Error: Unable to establish connection with FAM.\n\nDo you have \"FAM\" or \"Gamin\" installed and running?") );
        vfs_file_monitor_clean();
        free_settings();
        return 1;
    }

    vfs_mime_type_init();   /* Initialize our mime-type system */

    /* open files passed in command line arguments */
    for( file = files; *file; ++file )
    {
        char* file_path, *real_path;
        if( g_str_has_prefix( *file, "file:" ) ) /* It's a URI */
        {
            file_path = g_filename_from_uri( *file, NULL, NULL );
            g_free( *file );
            *file = file_path;
        }
        else
            file_path = *file;

        real_path = vfs_file_resolve_path( NULL, file_path );
        if( g_file_test( real_path, G_FILE_TEST_IS_DIR ) )
        {
            if( G_UNLIKELY( ! main_window ) )   /* create main window if needed */
            {
                /* initialize things required by folder view... */
                init_folder();
                main_window = create_main_window();
            }
            fm_main_window_add_new_tab( main_window, real_path,
                                        appSettings.showSidePane,
                                        appSettings.sidePaneMode );
        }
        else
        {
            open_file( real_path );
        }
        g_free( real_path );
    }

    if( files != default_files )
        g_strfreev( files );

    /* If we need to show desktop icons */
    if( appSettings.showDesktop )
    {
        if( G_LIKELY( ! main_window ) )
            init_folder();
        fm_desktop_init();
    }

    if( G_LIKELY( main_window || appSettings.showDesktop ) )
    {
        gtk_main(); /* if folder windows or desktop icons are showed. */
    }
    else
    {
        vfs_mime_type_clean();
        vfs_file_monitor_clean();
        /* if ( appSettings.singleInstance ) */
        single_instance_finalize();
        free_settings();
        return 0;
    }

    /* if ( appSettings.singleInstance ) */
    single_instance_finalize();

    if( no_desktop )    /* desktop icons is temporarily supressed */
    {
        if( old_show_desktop )  /* restore original settings */
        {
            old_show_desktop = appSettings.showDesktop;
            appSettings.showDesktop = TRUE;
        }
    }
    save_settings();    /* write config file */
    free_settings();

    vfs_volume_finalize();
    vfs_mime_type_clean();
    vfs_file_monitor_clean();

    if( old_show_desktop )   /* If desktop icons are showed, finalize them. */
        fm_desktop_cleanup();

    return 0;
}

void open_file( const char* path )
{
    GError * err;
    char *msg, *error_msg;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    gboolean opened;
    char* app_name;

    if ( ! g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        ptk_show_error( NULL, _("Error"), _( "File doesn't exist" ) );
        return ;
    }

    file = vfs_file_info_new();
    vfs_file_info_get( file, path, NULL );
    mime_type = vfs_file_info_get_mime_type( file );
    opened = FALSE;
    err = NULL;

    app_name = vfs_mime_type_get_default_action( mime_type );
    if ( app_name )
    {
        opened = vfs_file_info_open_file( file, path, &err );
        g_free( app_name );
    }
    else
    {
        VFSAppDesktop* app;
        GList* files;

        app_name = ptk_choose_app_for_mime_type( NULL, mime_type );
        if ( app_name )
        {
            app = vfs_app_desktop_new( app_name );
            if ( ! vfs_app_desktop_get_exec( app ) )
                app->exec = g_strdup( app_name ); /* This is a command line */
            files = g_list_prepend( NULL, path );
            opened = vfs_app_desktop_open_files( gdk_screen_get_default(),
                                                 NULL, app, files, &err );
            g_free( files->data );
            g_list_free( files );
            vfs_app_desktop_unref( app );
            g_free( app_name );
        }
        else
            opened = TRUE;
    }

    if ( !opened )
    {
        char * disp_path;
        if ( err && err->message )
        {
            error_msg = err->message;
        }
        else
            error_msg = _( "Don't know how to open the file" );
        disp_path = g_filename_display_name( path );
        msg = g_strdup_printf( _( "Unable to open file:\n\"%s\"\n%s" ), disp_path, error_msg );
        g_free( disp_path );
        ptk_show_error( NULL, _("Error"), msg );
        g_free( msg );
        if ( err )
            g_error_free( err );
    }
    vfs_mime_type_unref( mime_type );
    vfs_file_info_unref( file );
}
