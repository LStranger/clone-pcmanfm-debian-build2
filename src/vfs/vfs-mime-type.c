/*
*  C Implementation: vfs-mime_type-type
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-mime-type.h"
#include "vfs-file-monitor.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "glib-mem.h"

static GHashTable *mime_hash = NULL;
static guint reload_callback_id = 0;
static int big_icon_size = 32, small_icon_size = 16;

const char mime_actions_group[] = "MIME Cache";
static GKeyFile *mime_actions = NULL;
static GKeyFile *user_actions = NULL;
VFSFileMonitor* mime_actions_monitor = NULL;

static guint theme_change_notify = 0;

static void on_icon_theme_changed( GtkIconTheme *icon_theme,
                                   gpointer user_data );

#if 0
/* FIXME: Don't cache mime actions since they change often */

static void free_old_actions( gpointer key,
                              gpointer value,
                              gpointer user_data )
{
    VFSMimeType * mime_type = ( VFSMimeType* ) value;
    g_strfreev( mime_type->actions );
    mime_type->actions = NULL;
}

#endif

static void reload_mime_actions( VFSFileMonitor* fm,
                                 VFSFileMonitorEvent event,
                                 const char* file_name,
                                 gpointer user_data )
{
#if 0
    /* unload all old cached data */
    /* FIXME: Why sometimes this callback gets called twice? */
    g_hash_table_foreach( mime_hash, free_old_actions, NULL );
#endif

    if ( mime_actions )
    {
        g_key_file_free( mime_actions );
        /* reload data */
        mime_actions = g_key_file_new();
        g_key_file_load_from_file( mime_actions,
                                   fm->path,
                                   G_KEY_FILE_NONE, NULL );
    }
}


static void init_mime_actions()
{
    /* FIXME: Paths of directories shouldn't be hardcoded */
    gchar * full_path = g_build_filename( "/usr/share",
                                          "applications/mimeinfo.cache",
                                          NULL );
    mime_actions = g_key_file_new();
    g_key_file_load_from_file( mime_actions,
                               full_path,
                               G_KEY_FILE_NONE, NULL );
    /* FIXME: using file monitors for this purpose is a waste and should be changed */
    mime_actions_monitor = vfs_file_monitor_add( full_path,
                                                 reload_mime_actions,
                                                 mime_hash );
    g_free( full_path );

    full_path = g_build_filename( g_get_home_dir(),
                                  ".pcmanfm/mime_info",
                                  NULL );

    user_actions = g_key_file_new();
    g_key_file_load_from_file( user_actions,
                               full_path,
                               G_KEY_FILE_NONE, NULL );
    g_free( full_path );
}

/* Final clean up */
void finalize_mime_actions()
{
    if ( mime_actions )
        g_key_file_free( mime_actions );
    if ( user_actions )
        g_key_file_free( user_actions );
    vfs_file_monitor_remove( mime_actions_monitor,
                             reload_mime_actions, mime_hash );
}

/* Save user profile */
static void save_mime_actions()
{
    gchar * full_path;
    gchar* data;
    gsize len;
    int file;

    if ( ! mime_actions && ! user_actions )
        init_mime_actions();

    if ( user_actions )
    {
        if ( data = g_key_file_to_data ( user_actions, &len, NULL ) )
        {
            full_path = g_build_filename( g_get_home_dir(),
                                          ".pcmanfm/mime_info",
                                          NULL );

            file = creat( full_path, S_IWUSR | S_IRUSR | S_IRGRP );
            g_free( full_path );

            if ( file != -1 )
            {
                write( file, data, len );
                close( file );
            }
            else
            {
                g_warning( "Cannot save mime actions\n" );
            }
            g_free( data );
        }
    }
}

static void vfs_mime_type_reload( void* user_data )
{
    /* FIXME: process mime database reloading properly. */
    /* Remove all items in the hash table */
    g_hash_table_foreach_remove ( mime_hash, ( GHRFunc ) gtk_true, NULL );
}

void vfs_mime_type_init()
{
    GtkIconTheme * theme;
    xdg_mime_init();
    reload_callback_id = xdg_mime_register_reload_callback( vfs_mime_type_reload,
                                                            NULL, NULL );

    mime_hash = g_hash_table_new_full( g_str_hash, g_str_equal,
                                       NULL, vfs_mime_type_unref );
    theme = gtk_icon_theme_get_default();
    theme_change_notify = g_signal_connect( theme, "changed",
                                            G_CALLBACK( on_icon_theme_changed ),
                                            NULL );
}

void vfs_mime_type_clean()
{
    GtkIconTheme * theme;
    theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect( theme, theme_change_notify );

    xdg_mime_remove_callback( reload_callback_id );
    xdg_mime_shutdown();
    g_hash_table_destroy( mime_hash );

    if ( mime_actions || user_actions )
        finalize_mime_actions();
}

VFSMimeType* vfs_mime_type_get_from_file_name( const char* ufile_name )
{
    const char * type;
    type = xdg_mime_get_mime_type_from_file_name( ufile_name );
    return vfs_mime_type_get_from_type( type );
}

VFSMimeType* vfs_mime_type_get_from_file( const char* file_path,
                                          const char* base_name,
                                          struct stat* pstat )
{
    struct stat file_stat;
    const char* type;

    /* We have to get the mime-type of target path
       when the file itself is a symlink */
    if ( !pstat || S_ISLNK( pstat->st_mode ) )
    {
        /* FIXME: If the file doesn't exist, should we return NULL? */
        if ( stat( file_path, &file_stat ) == 0 )
            pstat = &file_stat;
    }
    if ( S_ISDIR( pstat->st_mode ) )
    {
        type = XDG_MIME_TYPE_DIRECTORY;
    }
    else
    {
        if ( !base_name )
        {
            base_name = strrchr( file_path, '/' );
            if ( base_name )
            {
                ++base_name;
            }
            else
            {
                base_name = file_path;
            }
        }
        type = xdg_mime_get_mime_type_for_file( file_path, base_name, pstat );
    }
    return vfs_mime_type_get_from_type( type );
}

VFSMimeType* vfs_mime_type_get_from_type( const char* type )
{
    VFSMimeType * mime_type;

    mime_type = g_hash_table_lookup( mime_hash, type );
    if ( !mime_type )
    {
        mime_type = vfs_mime_type_new( type );
        g_hash_table_insert( mime_hash, mime_type->type, mime_type );
    }
    vfs_mime_type_ref( mime_type );
    return mime_type;
}

VFSMimeType* vfs_mime_type_new( const char* type_name )
{
    VFSMimeType * mime_type = g_slice_new0( VFSMimeType );
    mime_type->type = g_strdup( type_name );
    mime_type->n_ref = 1;
    return mime_type;
}

void vfs_mime_type_ref( VFSMimeType* mime_type )
{
    ++mime_type->n_ref;
}

void vfs_mime_type_unref( VFSMimeType* mime_type )
{
    --mime_type->n_ref;
    if ( mime_type->n_ref <= 0 )
    {
        if ( mime_type->type )
            g_free( mime_type->type );
        if ( mime_type->big_icon )
            gdk_pixbuf_unref( mime_type->big_icon );
        if ( mime_type->small_icon )
            gdk_pixbuf_unref( mime_type->small_icon );
        /* g_strfreev( mime_type->actions ); */

        g_slice_free( VFSMimeType, mime_type );
    }
}

GdkPixbuf* vfs_mime_type_get_icon( VFSMimeType* mime_type, gboolean big )
{
    GdkPixbuf * icon = NULL;
    const char* sep;
    char icon_name[ 100 ];
    GtkIconTheme *icon_theme;
    int size;

    if ( big )
    {
        if ( G_LIKELY( mime_type->big_icon ) )     /* big icon */
            return gdk_pixbuf_ref( mime_type->big_icon );
        size = big_icon_size;
    }
    else    /* small icon */
    {
        if ( G_LIKELY( mime_type->small_icon ) )
            return gdk_pixbuf_ref( mime_type->small_icon );
        size = small_icon_size;
    }

    icon_theme = gtk_icon_theme_get_default ();

    if ( G_UNLIKELY( 0 == strcmp( mime_type->type, XDG_MIME_TYPE_DIRECTORY ) ) )
    {
        icon = gtk_icon_theme_load_icon ( icon_theme, "gnome-fs-directory",
                                          size, 0, NULL );
        if ( big )
            mime_type->big_icon = icon;
        else
            mime_type->small_icon = icon;
        return icon ? gdk_pixbuf_ref( icon ) : NULL;
    }

    sep = strchr( mime_type->type, '/' );
    if ( !sep )
        return NULL;
    strcpy( icon_name, "gnome-mime-" );
    strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
    strcat( icon_name, "-" );
    strcat( icon_name, sep + 1 );
    icon = gtk_icon_theme_load_icon ( icon_theme, icon_name,
                                      size, 0, NULL );

    if ( ! icon )
    {
        icon_name[ 11 ] = 0;
        strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
        icon = gtk_icon_theme_load_icon ( icon_theme, icon_name,
                                          size, 0, NULL );
    }
    if( G_UNLIKELY( !icon ) )
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if( G_LIKELY( strcmp(mime_type->type, XDG_MIME_TYPE_UNKNOWN) ) )
        {
            /* FIXME: fallback to icon of parent mime-type */
            VFSMimeType* unknown;
            unknown = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
            icon = vfs_mime_type_get_icon( unknown, big );
            vfs_mime_type_unref( unknown );
        }
    }

    if ( big )
        mime_type->big_icon = icon;
    else
        mime_type->small_icon = icon;
    return icon ? gdk_pixbuf_ref( icon ) : NULL;
}

static void free_cached_icons ( gpointer key,
                                gpointer value,
                                gpointer user_data )
{
    VFSMimeType * mime_type = ( VFSMimeType* ) value;
    gboolean big = ( gboolean ) user_data;
    if ( big )
    {
        if ( mime_type->big_icon )
        {
            gdk_pixbuf_unref( mime_type->big_icon );
            mime_type->big_icon = NULL;
        }
    }
    else
    {
        if ( mime_type->small_icon )
        {
            gdk_pixbuf_unref( mime_type->small_icon );
            mime_type->small_icon = NULL;
        }
    }
}

void vfs_mime_type_set_icon_size( int big, int small )
{
    if ( big != big_icon_size )
    {
        big_icon_size = big;
        /* Unload old cached icons */
        g_hash_table_foreach( mime_hash,
                              free_cached_icons,
                              ( gpointer ) TRUE );
    }
    if ( small != small_icon_size )
    {
        small_icon_size = small;
        /* Unload old cached icons */
        g_hash_table_foreach( mime_hash,
                              free_cached_icons,
                              ( gpointer ) FALSE );
    }
}

void vfs_mime_type_get_icon_size( int* big, int* small )
{
    if ( big )
        * big = big_icon_size;
    if ( small )
        * small = small_icon_size;
}

const char* vfs_mime_type_get_type( VFSMimeType* mime_type )
{
    return mime_type->type;
}

/* Get human-readable description of mime type */
const char* vfs_mime_type_get_description( VFSMimeType* mime_type )
{
    int fd;
    struct stat file_stat;
    const char** lang;
    char full_path[ 256 ];
    char* buf = NULL;
    char* desc = NULL;
    char* eng_desc = NULL;
    char* tmp;
    int len;
    if ( mime_type->description )
        return mime_type->description;

    /* FIXME: This path shouldn't be hard-coded. */
    sprintf( full_path, "/usr/share/mime/%s.xml", mime_type->type );

    fd = open( full_path, O_RDONLY );
    if ( fd != -1 )
    {
        fstat( fd, &file_stat );
        if ( file_stat.st_size > 0 )
        {
            buf = g_malloc( file_stat.st_size + 1 );
            read( fd, buf, file_stat.st_size );
            buf[ file_stat.st_size ] = '\0';
            eng_desc = strstr( buf, "<comment>" );
            if ( eng_desc )
            {
                eng_desc += 9;
                for ( desc = eng_desc; *desc != '\n' && *desc; ++desc )
                    ;
                while ( desc = strstr( desc, "<comment xml:lang=" ) )
                {
                    if ( !desc )
                        break;

                    desc += 19;
                    lang = g_get_language_names();
                    tmp = strchr( lang[ 0 ], '.' );
                    len = tmp ? ( ( int ) tmp - ( int ) lang[ 0 ] ) : strlen( lang[ 0 ] );

                    if ( lang && 0 == strncmp( desc, lang[ 0 ], len ) )
                    {
                        desc += ( len + 2 );
                        break;
                    }
                    while ( *desc != '\n' && *desc )
                        ++desc;
                }
            }
        }
        close( fd );
    }

    if ( !desc )
    {
        desc = eng_desc;
        if ( !desc )
            return mime_type->type;
    }

    if ( desc )
    {
        if ( tmp = strstr( desc, "</" ) )
            * tmp = '\0';
        desc = g_strdup( desc );
    }
    if ( buf )
    {
        g_free( buf );
    }
    mime_type->description = desc;
    return desc;
}

/*
* Join two string vector containing app lists to generate a new one.
* Duplicated app will be removed.
*/
char** vfs_mime_type_join_actions( char** list1, gsize len1,
                                   char** list2, gsize len2 )
{
    gchar **ret = NULL;
    int i, j, k;

    if ( len1 > 0 || len2 > 0 )
        ret = g_new0( char*, len1 + len2 + 1 );
    for ( i = 0; i < len1; ++i )
    {
        ret[ i ] = g_strdup( list1[ i ] );
    }
    for ( j = 0, k = 0; j < len2; ++j )
    {
        for ( i = 0; i < len1; ++i )
        {
            if ( 0 == strcmp( ret[ i ], list2[ j ] ) )
                break;
        }
        if ( i >= len1 )
        {
            ret[ len1 + k ] = g_strdup( list2[ j ] );
            ++k;
        }
    }
    return ret;
}

char** vfs_mime_type_get_actions( VFSMimeType* mime_type )
{
    gchar **vals;
    gsize len;
    gchar **text_vals;
    gsize text_len;
    gchar **tmp_vals;
    gsize tmp_len;
    gboolean is_text = FALSE;
    int i, j, k;
    GKeyFile* profiles[ 2 ];
    char** apps[ 2 ];
    char** parents;
    char** actions;

    if ( ! mime_actions && ! user_actions )
        init_mime_actions();

    /*  FIXME: Cancel this useless cache since it causes problems.
    if ( mime_type->actions )
        return mime_type->actions;
        mime_type->actions = NULL;
    */
    actions = NULL;

    profiles[ 0 ] = user_actions;
    profiles[ 1 ] = mime_actions;

    is_text = xdg_mime_mime_type_subclass(mime_type, XDG_MIME_TYPE_PLAIN_TEXT);

    for ( i = 0; i < 2; ++i )
    {
        len = 0;
        vals = g_key_file_get_string_list ( profiles[ i ], mime_actions_group,
                                            mime_type->type, &len, NULL );

        /* Special process for text files */
        if ( is_text )
        {
            text_len = 0;
            text_vals = g_key_file_get_string_list ( profiles[ i ], mime_actions_group,
                                                     XDG_MIME_TYPE_PLAIN_TEXT,
                                                     &text_len, NULL );
            tmp_vals = vfs_mime_type_join_actions( vals, len, text_vals, text_len );
            if ( vals )
                g_strfreev( vals );
            if ( text_vals )
                g_strfreev( text_vals );
            vals = tmp_vals;
        }
        apps[ i ] = vals;
    }
    len = apps[ 0 ] ? g_strv_length( apps[ 0 ] ) : 0;
    tmp_len = apps[ 1 ] ? g_strv_length( apps[ 1 ] ) : 0;
    actions = vfs_mime_type_join_actions( apps[ 0 ], len,
                                                     apps[ 1 ], tmp_len );
    if ( apps[ 0 ] )
        g_strfreev( apps[ 0 ] );
    if ( apps[ 1 ] )
        g_strfreev( apps[ 1 ] );

    /* FIXME: This part is extremely buggy!!!
              Must be re-written before release. */
    parents = xdg_mime_list_mime_parents( mime_type->type );
    if ( parents )
    {
        char** parent_actions;
        char** parent;
        VFSMimeType* parent_mime;
        for ( parent = parents; *parent; ++parent )
        {
            parent_mime = vfs_mime_type_get_from_type( *parent );
            parent_actions = vfs_mime_type_get_actions( parent_mime );
            vfs_mime_type_unref( parent_mime );
            if ( parent_actions )
            {
                len = actions ? g_strv_length( actions ) : 0;
                tmp_len = g_strv_length( parent_actions );
                tmp_vals = vfs_mime_type_join_actions(
                               actions, len,
                               parent_actions, tmp_len );
                g_strfreev( actions );
                g_strfreev( parent_actions );
                actions = tmp_vals;
            }
        }
        g_free( parents );
    }
    return actions;
}

char* vfs_mime_type_get_default_action( VFSMimeType* mime_type )
{
    char** actions = vfs_mime_type_get_actions( mime_type );
    char* action = actions ? g_strdup( actions[ 0 ] ) : NULL;
    g_strfreev( actions );
    return action;
}

/*
* Add app.desktop to specified mime-type
* app can be the name of the desktop file or a command line.
* If default_action = TRUE, the action will be inserted as 
* the first element; otherwise, it will be added to the list
* at non-specific place.
*/
static void _vfs_mime_type_add_action( VFSMimeType* mime_type,
                                       const char* action,
                                       gboolean default_action )
{
    gchar **vals;
    gchar **new_vals;
    gchar **new_val;
    int i;
    gsize len;
    gboolean already_exists;

    if ( ! mime_actions && ! user_actions )
        init_mime_actions();

    if( default_action )
    {
        vals = g_key_file_get_string_list ( user_actions,
                                            mime_actions_group,
                                            mime_type->type, &len,
                                            NULL );
        if( !vals )
        {
            g_key_file_set_string( user_actions, mime_actions_group,
                                mime_type->type, action );
            save_mime_actions();
            return ;
        }
    }
    else
    {
        vals = vfs_mime_type_get_actions( mime_type );
        len = vals ? g_strv_length( vals ) : 0;
    }

    new_vals = g_new0( gchar*, len + 2 );
    if ( default_action )
        new_vals[ 0 ] = g_strdup( action );
    else
        already_exists = FALSE;

    new_val = default_action ? ( new_vals + 1 ) : new_vals;
    for ( i = 0; i < len; ++i )
    {
        if ( strcmp( vals[ i ], action ) )
        {
            *new_val = vals[ i ];
            ++new_val;
        }
        else    /* the action already exists */
        {
            if ( default_action )
                g_free( vals[ i ] );
            else
            {
                already_exists = TRUE;
                *new_val = vals[ i ];
                ++new_val;
            }
        }
        vals[ i ] = NULL;
    }
    if ( vals )
        g_free( vals );
    if ( ! default_action && ! already_exists )
        new_vals[ i ] = g_strdup( action );

    g_key_file_set_string_list( user_actions,
                                mime_actions_group,
                                mime_type->type,
                                new_vals,
                                g_strv_length( new_vals ) );
    save_mime_actions();
    g_strfreev( new_vals );
}

/*
* Set default app.desktop for specified file.
* app can be the name of the desktop file or a command line.
*/
void vfs_mime_type_set_default_action( VFSMimeType* mime_type,
                                       const char* action )
{
    _vfs_mime_type_add_action( mime_type, action, TRUE );
}

void vfs_mime_type_add_action( VFSMimeType* mime_type,
                               const char* action )
{
    _vfs_mime_type_add_action( mime_type, action, FALSE );
}


/*
* char** vfs_mime_type_get_all_known_apps():
*
* Get all app.desktop files for all mime types.
* The returned string array contains a list of *.desktop file names,
* and should be freed when no longer needed.
*/

static void hash_to_strv ( gpointer key, gpointer value, gpointer user_data )
{
    char***all_apps = ( char*** ) user_data;
    **all_apps = ( char* ) key;
    ++*all_apps;
}

static char** get_all_known_apps_from_profile( GKeyFile* profile,
                                               gsize* list_len )
{
    char** all_apps = NULL;
    GHashTable* hash;
    gchar **keys;
    gchar **key;
    gchar **apps;
    gchar **app;
    int len = 0;

    hash = g_hash_table_new( g_str_hash, g_str_equal );

    keys = g_key_file_get_keys( profile,
                                mime_actions_group,
                                NULL, NULL );
    /* "MIME Cache" cannot be found. */
    if ( NULL == keys )
        goto just_free_keys;

    for ( key = keys; *key; ++key )
    {
        apps = g_key_file_get_string_list ( profile,
                                            mime_actions_group,
                                            *key, NULL, NULL );
        if ( !apps )
            continue;
        for ( app = apps; *app; ++app )
        {
            /* duplicated */
            if ( g_hash_table_lookup( hash, *app ) )
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
    *list_len = len;
    return all_apps;
}

char** vfs_mime_type_get_all_known_apps()
{
    char** user_apps;
    char** mime_apps;
    gsize len1, len2;
    char** all_apps;

    user_apps = get_all_known_apps_from_profile( user_actions, &len1 );
    mime_apps = get_all_known_apps_from_profile( mime_actions, &len2 );
    all_apps = vfs_mime_type_join_actions( user_apps, len1, mime_apps, len2 );
    g_strfreev( user_apps );
    g_strfreev( mime_apps );

    return all_apps;
}

void on_icon_theme_changed( GtkIconTheme *icon_theme,
                            gpointer user_data )
{
    /* reload_mime_icons */
    g_hash_table_foreach( mime_hash,
                          free_cached_icons,
                          ( gpointer ) TRUE );
    g_hash_table_foreach( mime_hash,
                          free_cached_icons,
                          ( gpointer ) FALSE );
}
