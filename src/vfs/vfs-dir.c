/*
*  C Implementation: vfs-dir
*
* Description: Object used to present a directory
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-dir.h"
#include "glib-mem.h"

#include <string.h>

#include <fcntl.h>  /* for open() */
#include <unistd.h> /* for read */

static void vfs_dir_class_init( VFSDirClass* klass );
static void vfs_dir_init( VFSDir* dir );
static void vfs_dir_finalize( GObject *obj );
static void vfs_dir_set_property ( GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec );
static void vfs_dir_get_property ( GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec );

/* constructor is private */
static VFSDir* vfs_dir_new( const char* path );

static void vfs_dir_load( VFSDir* dir );
static gpointer vfs_dir_load_thread( VFSAsyncTask* task, VFSDir* dir );

static void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                                      VFSFileMonitorEvent event,
                                      const char* file_name,
                                      gpointer user_data );

static gpointer load_thumbnail_thread( gpointer user_data );

static void on_mime_type_reload( gpointer user_data );


static void update_changed_files( gpointer key, gpointer data,
                                  gpointer user_data );
static gboolean notify_file_change( gpointer user_data );
static gboolean update_file_info( VFSDir* dir, VFSFileInfo* file );

static gboolean is_dir_desktop( const char* path );

enum {
    FILE_CREATED_SIGNAL = 0,
    FILE_DELETED_SIGNAL,
    FILE_CHANGED_SIGNAL,
    THUMBNAIL_LOADED_SIGNAL,
    FILE_LISTED_SIGNAL,
    N_SIGNALS
};

static guint signals[ N_SIGNALS ] = { 0 };
static GObjectClass *parent_class = NULL;

static GHashTable* dir_hash = NULL;
static GList* mime_cb = NULL;
static guint change_notify_timeout = 0;

static char* desktop_dir = NULL;
static gboolean is_desktop_set = FALSE;

GType vfs_dir_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( VFSDirClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) vfs_dir_class_init,
                NULL,
                NULL,
                sizeof ( VFSDir ),
                0,
                ( GInstanceInitFunc ) vfs_dir_init,
                NULL,
            };
        type = g_type_register_static ( G_TYPE_OBJECT, "VFSDir", &info, 0 );
    }
    return type;
}

void vfs_dir_class_init( VFSDirClass* klass )
{
    GObjectClass * object_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = vfs_dir_set_property;
    object_class->get_property = vfs_dir_get_property;
    object_class->finalize = vfs_dir_finalize;

    /* signals */
//    klass->file_created = on_vfs_dir_file_created;
//    klass->file_deleted = on_vfs_dir_file_deleted;
//    klass->file_changed = on_vfs_dir_file_changed;

    /*
    * file-created is emitted when there is a new file created in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_CREATED_SIGNAL ] =
        g_signal_new ( "file-created",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_created ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * file-deleted is emitted when there is a file deleted in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_DELETED_SIGNAL ] =
        g_signal_new ( "file-deleted",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_deleted ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * file-changed is emitted when there is a file changed in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_CHANGED_SIGNAL ] =
        g_signal_new ( "file-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ THUMBNAIL_LOADED_SIGNAL ] =
        g_signal_new ( "thumbnail-loaded",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, thumbnail_loaded ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ FILE_LISTED_SIGNAL ] =
        g_signal_new ( "file-listed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_listed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__BOOLEAN,
                       G_TYPE_NONE, 1, G_TYPE_BOOLEAN );

    /* FIXME: Is there better way to do this? */
    if( G_UNLIKELY( ! is_desktop_set ) )
        vfs_get_desktop_dir();
}

/* constructor */
void vfs_dir_init( VFSDir* dir )
{
    dir->mutex = g_mutex_new();
}

/* destructor */

void vfs_dir_finalize( GObject *obj )
{
    VFSDir * dir = VFS_DIR( obj );
    GList* l;

    do{}
    while( g_source_remove_by_user_data( dir ) );

    if( G_UNLIKELY( dir->task ) )
    {
        /* FIXME: should we generate a "file-list" signal to indicate the dir loading was cancelled? */
        g_object_unref( dir->task );
        dir->task = NULL;
    }
    if ( dir->monitor )
    {
        vfs_file_monitor_remove( dir->monitor,
                                 vfs_dir_monitor_callback,
                                 dir );
    }
    if ( dir->path )
    {
        if( G_LIKELY( dir_hash ) )
        {
            g_hash_table_remove( dir_hash, dir->path );

            /* There is no VFSDir instance */
            if ( 0 == g_hash_table_size( dir_hash ) )
            {
                g_hash_table_destroy( dir_hash );
                dir_hash = NULL;

                vfs_mime_type_remove_reload_cb( mime_cb );
                mime_cb = NULL;

                if( change_notify_timeout )
                {
                    g_source_remove( change_notify_timeout );
                    change_notify_timeout = 0;
                }
            }
        }
        g_free( dir->path );
        g_free( dir->disp_path );
        dir->path = dir->disp_path = NULL;
    }
    /* g_debug( "dir->thumbnail_loader: %p", dir->thumbnail_loader ); */
    if( G_UNLIKELY( dir->thumbnail_loader ) )
    {
        /* g_debug( "FREE THUMBNAIL LOADER IN VFSDIR" ); */
        vfs_thumbnail_loader_free( dir->thumbnail_loader );
        dir->thumbnail_loader = NULL;
    }

    if ( dir->file_list )
    {
        g_list_foreach( dir->file_list, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( dir->file_list );
        dir->file_list = NULL;
        dir->n_files = 0;
    }

    if( dir->changed_files )
    {
        g_slist_foreach( dir->changed_files, (GFunc)vfs_file_info_unref, NULL );
        g_slist_free( dir->changed_files );
        dir->changed_files = NULL;
    }

    g_mutex_free( dir->mutex );
    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void vfs_dir_get_property ( GObject *obj,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec )
{}

void vfs_dir_set_property ( GObject *obj,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec )
{}

static GList* vfs_dir_find_file( VFSDir* dir, const char* file_name, VFSFileInfo* file )
{
    GList * l;
    VFSFileInfo* file2;
    for ( l = dir->file_list; l; l = l->next )
    {
        file2 = ( VFSFileInfo* ) l->data;
        if( G_UNLIKELY( file == file2 ) )
            return l;
        if ( file2->name && 0 == strcmp( file2->name, file_name ) )
        {
            return l;
        }
    }
    return NULL;
}

/* signal handlers */
void vfs_dir_emit_file_created( VFSDir* dir, const char* file_name, VFSFileInfo* file )
{
    GList* l;
    char* full_path = NULL;
    if( G_UNLIKELY( 0 == strcmp(file_name, dir->path) ) )
    {
        /* Special Case: The directory itself was deleted... */
#if 0
        file = NULL;
        g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
        vfs_dir_load( dir, file_name );
#endif
        return;
    }
    g_mutex_lock( dir->mutex );

    l = vfs_dir_find_file( dir, file_name, file );

    if ( G_LIKELY( ! l ) )  /* not in our list */
    {
        full_path = g_build_filename( dir->path, file_name, NULL );
        if( G_UNLIKELY( file ) )
        {
            vfs_file_info_ref( file );
        }
        else
        {
            file = vfs_file_info_new();
            if ( ! vfs_file_info_get( file, full_path, NULL ) )
            {
                vfs_file_info_unref( file );
                file = NULL;
            }
        }
    }
    else
    {
        file = NULL;
    }

    g_mutex_unlock( dir->mutex );

    if ( file )
    {
        /* gboolean is_desktop = is_dir_desktop(dir->path); */
        if( /*G_UNLIKELY(is_desktop) &&*/ G_LIKELY(full_path) )
            vfs_file_info_load_special_info( file, full_path );

        dir->file_list = g_list_prepend( dir->file_list, vfs_file_info_ref(file) );
        ++dir->n_files;

        g_signal_emit( dir, signals[ FILE_CREATED_SIGNAL ], 0, file );
        vfs_file_info_unref( file );
    }
    g_free( full_path );
}

void vfs_dir_emit_file_deleted( VFSDir* dir, const char* file_name, VFSFileInfo* file )
{
    GList* l;

    if( G_UNLIKELY( 0 == strcmp(file_name, dir->path) ) )
    {
        /* Special Case: The directory itself was deleted... */
        file = NULL;
        /* clear the whole list */
        g_mutex_lock( dir->mutex );
        g_list_foreach( dir->file_list, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( dir->file_list );
        dir->file_list = NULL;
        g_mutex_unlock( dir->mutex );

        g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
        return;
    }

    g_mutex_lock( dir->mutex );

    l = vfs_dir_find_file( dir, file_name, file );

    if ( l )
    {
        file = ( VFSFileInfo* ) l->data;
        dir->file_list = g_list_delete_link( dir->file_list, l );
        --dir->n_files;
    }
    else
        file = NULL;
    g_mutex_unlock( dir->mutex );
    if ( G_LIKELY( file ) )
    {
        g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
        vfs_file_info_unref( file );
    }
}

void vfs_dir_emit_file_changed( VFSDir* dir, const char* file_name, VFSFileInfo* file  )
{
    GList* l;

    g_mutex_lock( dir->mutex );

    l = vfs_dir_find_file( dir, file_name, file );
    if ( G_LIKELY( l ) )
    {
        file = vfs_file_info_ref( ( VFSFileInfo* ) l->data );
        /*
            NOTE: We should do some hack here not to fire "change" signal
                  too often, or the GUI will need to be refreshed every
                  second when the file is continuously changed.
                  For example, when downloading a file this happens.

            FIXME: Is there any better way to do this? Here's an idea:
                   Maybe adding a timer with an interval of 5 sec or so.
                   When the timeout handler called, it check if there is
                   anyone request the timer to be alive. If it's true,
                   the timeout stays alive, and will be called in the next
                   5 seconds. The "change" signal only get fired when the file
                   wasn't changed in the past 5 seconds, or we'll wait until
                   the next time the handler called, and fire the signal if the
                   file wasn't changed in the past 5 seconds.
        */
        if( ! g_slist_find( dir->changed_files, file ) )
        {
            /* update file info the first time */
            if( G_LIKELY( update_file_info( dir, file ) ) )
            {
                dir->changed_files = g_slist_prepend( dir->changed_files, vfs_file_info_ref(file) );

                if( 0 == change_notify_timeout )
                {
                    /* check every 5 seconds */
                    change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                                5000,
                                                                notify_file_change,
                                                                NULL, NULL );
                }
            }
        }
    }
    else
        file = NULL;

    g_mutex_unlock( dir->mutex );
    if ( G_LIKELY( file ) )
    {
        g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, file );
        vfs_file_info_unref( file );
    }
}

void vfs_dir_emit_thumbnail_loaded( VFSDir* dir, VFSFileInfo* file )
{
    GList* l;
    g_mutex_lock( dir->mutex );
    l = vfs_dir_find_file( dir, file->name, file );
    if( l )
    {
        g_assert( file == (VFSFileInfo*)l->data );
        file = vfs_file_info_ref( (VFSFileInfo*)l->data );
    }
    else
        file = NULL;
    g_mutex_unlock( dir->mutex );

    if ( G_LIKELY( file ) )
    {
        g_signal_emit( dir, signals[ THUMBNAIL_LOADED_SIGNAL ], 0, file );
        vfs_file_info_unref( file );
    }
}

/* methods */

VFSDir* vfs_dir_new( const char* path )
{
    VFSDir * dir;
    dir = ( VFSDir* ) g_object_new( VFS_TYPE_DIR, NULL );
    dir->path = g_strdup( path );
    dir->disp_path = g_filename_display_name( path );
    return dir;
}

static void on_list_task_finished( VFSAsyncTask* task, gboolean is_cancelled, VFSDir* dir )
{
    g_object_unref( dir->task );
    dir->task = NULL;
    g_signal_emit( dir, signals[FILE_LISTED_SIGNAL], 0, is_cancelled );
}

void vfs_dir_load( VFSDir* dir )
{
    GSList * l;
    struct VFSDirStateCallbackEnt* ent;

    if ( G_LIKELY(dir->path) )
    {
        dir->task = vfs_async_task_new( (VFSAsyncFunc)vfs_dir_load_thread, dir );
        g_signal_connect( dir->task, "finish", G_CALLBACK(on_list_task_finished), dir );
        vfs_async_task_execute( dir->task );
    }
}

gboolean is_dir_desktop( const char* path )
{
    return (desktop_dir && 0 == strcmp(path, desktop_dir));
}

gpointer vfs_dir_load_thread(  VFSAsyncTask* task, VFSDir* dir )
{
    const gchar * file_name;
    char* full_path;
    GDir* dir_content;
    VFSFileInfo* file;
    GList* l;
    /* gboolean is_desktop; */

    dir->file_listed = 0;
    dir->load_complete = 0;

    if ( dir->path )
    {
        /* Install file alteration monitor */
        dir->monitor = vfs_file_monitor_add_dir( dir->path,
                                             vfs_dir_monitor_callback,
                                             dir );

        dir_content = g_dir_open( dir->path, 0, NULL );
        if ( dir_content )
        {
            /* is_desktop = is_dir_desktop( dir->path ); */
            while ( ! vfs_async_task_is_cancelled( dir->task )
                        && ( file_name = g_dir_read_name( dir_content ) ) )
            {
                full_path = g_build_filename( dir->path, file_name, NULL );
                if ( !full_path )
                    continue;
                file = vfs_file_info_new();
                if ( G_LIKELY( vfs_file_info_get( file, full_path, file_name ) ) )
                {
                    g_mutex_lock( dir->mutex );
                    /* Special processing for desktop folder */
                    /* if( G_UNLIKELY(is_desktop) ) */
                    vfs_file_info_load_special_info( file, full_path );
                    dir->file_list = g_list_prepend( dir->file_list, file );
                    g_mutex_unlock( dir->mutex );
                    ++dir->n_files;
                }
                else
                {
                    vfs_file_info_unref( file );
                }
                g_free( full_path );
            }
            g_dir_close( dir_content );
        }
        dir->file_listed = 1;
        dir->load_complete = 1;
    }
    return NULL;
}

gboolean vfs_dir_is_loading( VFSDir* dir )
{
    return dir->task ? TRUE : FALSE;
}

gboolean vfs_dir_is_file_listed( VFSDir* dir )
{
    return dir->file_listed;
}

void vfs_cancel_load( VFSDir* dir )
{
    dir->cancel = TRUE;
    if ( dir->task )
    {
        vfs_async_task_cancel( dir->task );
        /* don't do g_object_unref on task here since this is done in the handler of "finish" signal. */
        dir->task = NULL;
    }
}

gboolean update_file_info( VFSDir* dir, VFSFileInfo* file )
{
    char* full_path;
    char* file_name;
    gboolean ret;
    /* gboolean is_desktop = is_dir_desktop(dir->path); */

    /* FIXME: Dirty hack: steal the string to prevent memory allocation */
    file_name = file->name;
    if( file->name == file->disp_name )
        file->disp_name = NULL;
    file->name = NULL;

    full_path = g_build_filename( dir->path, file_name, NULL );

    if ( G_LIKELY( full_path ) )
    {
        if( G_LIKELY( vfs_file_info_get( file, full_path, file_name ) ) )
        {
            ret = TRUE;
            /* if( G_UNLIKELY(is_desktop) ) */
            vfs_file_info_load_special_info( file, full_path );
        }
        else /* The file doesn't exist */
        {
            GList* l;
            l = g_list_find( dir->file_list, file );
            if( G_UNLIKELY(l) )
            {
                dir->file_list = g_list_delete_link( dir->file_list, l );
                --dir->n_files;
                if ( file )
                {
                    g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
                    vfs_file_info_unref( file );
                }
            }
            ret = FALSE;
        }
        g_free( full_path );
    }
    g_free( file_name );
    return ret;
}

void update_changed_files( gpointer key, gpointer data,
                                  gpointer user_data )
{
    VFSDir* dir = (VFSDir*)data;
    GSList* l;

    if( dir->changed_files )
    {
        for( l = dir->changed_files; l; l = l->next )
        {
            VFSFileInfo* file = l->data;
            g_mutex_lock( dir->mutex );
            update_file_info( dir, file );
            g_mutex_unlock( dir->mutex );
            vfs_file_info_unref( file );
        }
        g_slist_free( dir->changed_files );
        dir->changed_files = NULL;
    }
}

gboolean notify_file_change( gpointer user_data )
{
    GDK_THREADS_ENTER();
    g_hash_table_foreach( dir_hash, update_changed_files, NULL );
    /* remove the timeout */
    change_notify_timeout = 0;
    GDK_THREADS_LEAVE();
    return FALSE;
}

/* Callback function which will be called when monitored events happen */
void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                               VFSFileMonitorEvent event,
                               const char* file_name,
                               gpointer user_data )
{
    VFSDir* dir = ( VFSDir* ) user_data;
    GDK_THREADS_ENTER();

    switch ( event )
    {
    case VFS_FILE_MONITOR_CREATE:
        vfs_dir_emit_file_created( dir, file_name, NULL );
        break;
    case VFS_FILE_MONITOR_DELETE:
        vfs_dir_emit_file_deleted( dir, file_name, NULL );
        break;
    case VFS_FILE_MONITOR_CHANGE:
        vfs_dir_emit_file_changed( dir, file_name, NULL );
        break;
    default:
        g_warning("Error: unrecognized file monitor signal!");
        return;
    }
    GDK_THREADS_LEAVE();
}

VFSDir* vfs_dir_get_by_path( const char* path )
{
    VFSDir * dir = NULL;

    g_return_val_if_fail( G_UNLIKELY(path), NULL );

    if ( G_UNLIKELY( ! dir_hash ) )
        dir_hash = g_hash_table_new_full( g_str_hash, g_str_equal, NULL, NULL );
    else
        dir = g_hash_table_lookup( dir_hash, path );

    if( G_UNLIKELY( !mime_cb ) )
        mime_cb = vfs_mime_type_add_reload_cb( on_mime_type_reload, NULL );

    if ( dir )
        g_object_ref( dir );
    else
    {
        dir = vfs_dir_new( path );
        vfs_dir_load( dir );  /* asynchronous operation */
        g_hash_table_insert( dir_hash, (gpointer)dir->path, (gpointer)dir );
    }
    return dir;
}

static void reload_mime_type( char* key, VFSDir* dir, gpointer user_data )
{
    GList* l;
    VFSFileInfo* file;
    char* full_path;

    if( G_UNLIKELY( ! dir || ! dir->file_list ) )
        return;
    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        full_path = g_build_filename( dir->path,
                                      vfs_file_info_get_name( file ), NULL );
        vfs_file_info_reload_mime_type( file, full_path );
        /* g_debug( "reload %s", full_path ); */
        g_free( full_path );
    }

    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        g_signal_emit( dir, signals[FILE_CHANGED_SIGNAL], 0, file );
    }
    g_mutex_unlock( dir->mutex );
}

static void on_mime_type_reload( gpointer user_data )
{
    if( ! dir_hash )
        return;
    /* g_debug( "reload mime-type" ); */
    g_hash_table_foreach( dir_hash, (GHFunc)reload_mime_type, NULL );
}

/* Thanks to the freedesktop.org, things are much more complicated now... */
const char* vfs_get_desktop_dir()
{
    char* def;

    if( G_LIKELY(is_desktop_set) )
        return desktop_dir;

/* glib provides API for this since ver. 2.14, but I think my implementation is better. */
#if GLIB_CHECK_VERSION( 2, 14, 0 ) && 0  /* Delete && 0 to use the one provided by glib */
    desktop_dir = g_get_user_special_dir( G_USER_DIRECTORY_DESKTOP );
#else
    def = g_build_filename( g_get_user_config_dir(), "user-dirs.dirs", NULL );
    if( def )
    {
        int fd = open( def, O_RDONLY );
        g_free( def );
        if( G_LIKELY( fd != -1 ) )
        {
            struct stat s;
            if( G_LIKELY( fstat( fd, &s ) != -1 ) )
            {
                char* buf = g_malloc( s.st_size + 1 );
                if( (s.st_size = read( fd, buf, s.st_size )) != -1 )
                {
                    char* line;
                    buf[ s.st_size ] = 0;
                    line = strstr( buf, "XDG_DESKTOP_DIR=" );
                    if( G_LIKELY( line ) )
                    {
                        char* eol;
                        line += 16;
                        if( G_LIKELY( eol = strchr( line, '\n' ) ) )
                            *eol = '\0';
                        line = g_shell_unquote( line, NULL );
                        if( g_str_has_prefix(line, "$HOME") )
                        {
                            desktop_dir = g_build_filename( g_get_home_dir(), line + 5, NULL );
                            g_free( line );
                        }
                        else
                            desktop_dir = line;
                    }
                }
                g_free( buf );
            }
            close( fd );
        }
    }

    if( ! desktop_dir )
        desktop_dir = g_build_filename( g_get_home_dir(), "Desktop", NULL );
#endif

#if 0
    /* FIXME: what should we do if the user has no desktop dir? */
    if( ! g_file_test( desktop_dir, G_FILE_TEST_IS_DIR ) )
    {
        g_free( desktop_dir );
        desktop_dir = NULL;
    }
#endif
    is_desktop_set = TRUE;
    return desktop_dir;
}

void vfs_dir_foreach( GHFunc func, gpointer user_data )
{
    if( ! dir_hash )
        return;
    /* g_debug( "reload mime-type" ); */
    g_hash_table_foreach( dir_hash, (GHFunc)func, user_data );
}

void vfs_dir_unload_thumbnails( VFSDir* dir, gboolean is_big )
{
    GList* l;
    VFSFileInfo* file;
    g_mutex_lock( dir->mutex );
    if( is_big )
    {
        for( l = dir->file_list; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            if( file->big_thumbnail )
            {
                g_object_unref( file->big_thumbnail );
                file->big_thumbnail = NULL;
            }
        }
    }
    else
    {
        for( l = dir->file_list; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            if( file->small_thumbnail )
            {
                g_object_unref( file->small_thumbnail );
                file->small_thumbnail = NULL;
            }
        }
    }
    g_mutex_unlock( dir->mutex );
}
