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

/*
FIXME: The original multi-threading synchronization in this file is poor and
       needs re-check.  Briefly speaking, it's totally in a mess.
       If this works without any problems, you are really lucky.
       If not, take it easy.  That's quite normal.
 
       Finally, I found Thunar's thunar/thunar-thumbnail-generator.c, whose author
       is Benedikt Meurer, and use it as a reference.  Now, the multi-threading
       handling is learned from Thunar and re-written.
*/

#include "vfs-dir.h"
#include "vfs-file-info.h"
#include "glib-mem.h"

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

static void vfs_dir_load( VFSDir* dir, const char* path );
static gpointer vfs_dir_load_thread( VFSDir* dir );

static void vfs_dir_file_created( VFSDir* dir, const VFSFileInfo* file );
static void vfs_dir_file_deleted( VFSDir* dir, const VFSFileInfo* file );
static void vfs_dir_file_changed( VFSDir* dir, const VFSFileInfo* file );

static void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                                      VFSFileMonitorEvent event,
                                      const char* file_name,
                                      gpointer user_data );

static gboolean vfs_dir_call_state_callback( VFSDir* dir );

static gpointer load_thumbnail_thread( gpointer user_data );
static gboolean on_thumbnail_idle( VFSDir* dir );

static void on_mime_type_reload( gpointer user_data );

enum {
    FILE_CREATED_SIGNAL = 0,
    FILE_DELETED_SIGNAL,
    FILE_CHANGED_SIGNAL,
    N_SIGNALS
};

static guint signals[ N_SIGNALS ] = { 0 };
static GObjectClass *parent_class = NULL;

static GHashTable* dir_hash = NULL;
static int xdg_mime_cb_id = 0;

struct VFSDirStateCallbackEnt
{
    VFSDirStateCallback func;
    gpointer user_data;
    int stage;
};

enum
{
    LOAD_BIG_THUMBNAIL,
    LOAD_SMALL_THUMBNAIL,
    N_LOAD_TYPES
};

typedef struct _ThumbnailRequest
{
    int n_requests[ N_LOAD_TYPES ];
    VFSFileInfo* file;
}
ThumbnailRequest;


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
    klass->file_created = vfs_dir_file_created;
    klass->file_deleted = vfs_dir_file_deleted;
    klass->file_changed = vfs_dir_file_changed;

    /*
    * file-created is emitted when there is a new file created in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_CREATED_SIGNAL ] =
        g_signal_new ( "file-created",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
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
                       G_SIGNAL_RUN_LAST,
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
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
}

/* constructor */
void vfs_dir_init( VFSDir* dir )
{
    dir->mutex = g_mutex_new();
}

/* destructor */

static void vfs_dir_clear( VFSDir* dir )
{
    ThumbnailRequest * req;
    GList* l;

    if ( dir->monitor )
    {
        vfs_file_monitor_remove( dir->monitor,
                                 vfs_dir_monitor_callback,
                                 dir );
    }
    if ( dir->path )
    {
        g_hash_table_remove( dir_hash, dir->path );
        if ( 0 == g_hash_table_size( dir_hash ) )
        {
            g_hash_table_destroy( dir_hash );
            dir_hash = NULL;
            xdg_mime_remove_callback( xdg_mime_cb_id );
            xdg_mime_cb_id = 0;
        }
        g_free( dir->path );
        g_free( dir->disp_path );
    }

    if ( dir->thumbnail_mutex )
    {
        g_mutex_lock( dir->thumbnail_mutex );
        if ( dir->loaded_thumbnails )
        {
            for ( l = dir->loaded_thumbnails; l ;l = l->next )
            {
                req = l->data;
                vfs_file_info_unref( req->file );
                g_slice_free( ThumbnailRequest, req );
            }
            g_list_free( dir->loaded_thumbnails );
            dir->loaded_thumbnails = NULL;
        }
        if ( dir->thumbnail_requests )
        {
            while ( req = ( ThumbnailRequest* ) g_queue_pop_head( dir->thumbnail_requests ) )
            {
                vfs_file_info_unref( req->file );
                g_slice_free( ThumbnailRequest, req );
            }
            g_mutex_unlock( dir->thumbnail_mutex );
            if ( dir->thumbnail_thread )
            {
                g_cond_wait( dir->thumbnail_cond, dir->thumbnail_mutex );
            }
            g_cond_free( dir->thumbnail_cond );
            dir->thumbnail_cond = NULL;
        }
        else
        {
            g_mutex_unlock( dir->thumbnail_mutex );
        }

        if ( dir->thumbnail_idle )
        {
            g_source_remove( dir->thumbnail_idle );
            dir->thumbnail_idle = 0;
        }

        g_mutex_free( dir->thumbnail_mutex );
        dir->thumbnail_mutex = NULL;
    }

    if ( dir->file_list )
    {
        g_list_foreach( dir->file_list, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( dir->file_list );
        dir->file_list = NULL;
        dir->n_files = 0;
    }
}

void vfs_dir_finalize( GObject *obj )
{
    VFSDir * dir = VFS_DIR( obj );

    if ( dir->load_notify )
        g_source_remove( dir->load_notify );
    vfs_dir_clear( dir );

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

/* signal handlers */
void vfs_dir_file_created( VFSDir* dir, const VFSFileInfo* file )
{}
void vfs_dir_file_deleted( VFSDir* dir, const VFSFileInfo* file )
{}
void vfs_dir_file_changed( VFSDir* dir, const VFSFileInfo* file )
{}

/* methods */

VFSDir* vfs_dir_new()
{
    VFSDir * dir;
    dir = ( VFSDir* ) g_object_new( VFS_TYPE_DIR, NULL );
    return dir;
}

void vfs_dir_load( VFSDir* dir, const char* path )
{
    GSList * l;
    struct VFSDirStateCallbackEnt* ent;

    if ( path )
    {
        vfs_dir_clear( dir );
        dir->path = g_strdup( path );
        dir->disp_path = g_filename_display_name( path );
        for ( l = dir->state_callback_list; l; l = l->next )
        {
            ent = ( struct VFSDirStateCallbackEnt* ) l->data;
            ent->stage = 0;
        }
        dir->thread = g_thread_create( ( GThreadFunc ) vfs_dir_load_thread,
                                       dir, FALSE, NULL );
    }
}

gpointer vfs_dir_load_thread( VFSDir* dir )
{
    const gchar * file_name;
    char* full_path;
    GDir* dir_content;
    VFSFileInfo* file;
    GList* l;

    dir->file_listed = 0;
    dir->load_complete = 0;

    if ( dir->path )
    {
        /* Install file alteration monitor */
        dir->monitor = vfs_file_monitor_add( dir->path,
                                             vfs_dir_monitor_callback,
                                             dir );

        dir_content = g_dir_open( dir->path, 0, NULL );
        if ( dir_content )
        {
            while ( !dir->cancel && ( file_name = g_dir_read_name( dir_content ) ) )
            {
                full_path = g_build_filename( dir->path, file_name, NULL );
                if ( !full_path )
                    continue;
                file = vfs_file_info_new();
                if ( vfs_file_info_get( file, full_path, file_name ) )
                {
                    g_mutex_lock( dir->mutex );
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
        //        dir->load_notify = g_idle_add(
        //                               ( GSourceFunc ) vfs_dir_call_state_callback, dir );
        /* FIXME: load thumbnails and do some time-consuming tasks here */
        dir->load_complete = 1;
        dir->load_notify = g_idle_add(
                               ( GSourceFunc ) vfs_dir_call_state_callback, dir );
    }
    return NULL;
}

gboolean vfs_dir_is_loading( VFSDir* dir )
{
    return dir->thread ? TRUE : FALSE;
}

void vfs_cancel_load( VFSDir* dir )
{
    dir->cancel = TRUE;
    if ( dir->thread )
    {
        g_thread_join( dir->thread );
        dir->thread = NULL;
    }
}

GList* vfs_dir_find_file( VFSDir* dir, const char* file_name )
{
    GList * l;
    VFSFileInfo* file;
    for ( l = dir->file_list; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        if ( file->name && 0 == strcmp( file->name, file_name ) )
        {
            return l;
        }
    }
    return NULL;
}

/* Callback function which will be called when monitored events happen */
void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                               VFSFileMonitorEvent event,
                               const char* file_name,
                               gpointer user_data )
{
    char * full_path;
    VFSDir* dir = ( VFSDir* ) user_data;
    VFSFileInfo* file = NULL;
    GList* l;
    guint signal;

    g_mutex_lock( dir->mutex );
    switch ( event )
    {
    case VFS_FILE_MONITOR_CREATE:
        if ( ! vfs_dir_find_file( dir, file_name ) )
        {
            signal = FILE_CREATED_SIGNAL;
            full_path = g_build_filename( dir->path, file_name, NULL );
            if ( full_path )
            {
                file = vfs_file_info_new();
                if ( vfs_file_info_get( file, full_path, NULL ) )
                {
                    dir->file_list = g_list_prepend( dir->file_list, file );
                    vfs_file_info_ref( file );
                    ++dir->n_files;
                }
                else
                {
                    vfs_file_info_unref( file );
                    file = NULL;
                }
                g_free( full_path );
            }
        }
        break;
    case VFS_FILE_MONITOR_DELETE:
        l = vfs_dir_find_file( dir, file_name );
        if ( l )
        {
            signal = FILE_DELETED_SIGNAL;
            file = ( VFSFileInfo* ) l->data;
            dir->file_list = g_list_delete_link( dir->file_list, l );
            --dir->n_files;
        }
        break;
    case VFS_FILE_MONITOR_CHANGE:
        l = vfs_dir_find_file( dir, file_name );
        if ( l )
        {
            signal = FILE_CHANGED_SIGNAL;
            full_path = g_build_filename( dir->path, file_name, NULL );
            if ( full_path )
            {
                file = ( VFSFileInfo* ) l->data;
                vfs_file_info_ref( file );
                vfs_file_info_get( file, full_path, file_name );
                g_free( full_path );
            }
            else
                file = NULL;
        }
        break;
    }
    g_mutex_unlock( dir->mutex );

    if ( file )
    {
        g_signal_emit( dir, signals[ signal ], 0, file );
        vfs_file_info_unref( file );
    }
}

gboolean vfs_dir_call_state_callback( VFSDir* dir )
{
    GSList * l;
    struct VFSDirStateCallbackEnt* ent;

    dir->load_notify = 0;

    if ( dir->load_complete )
    {
        /* FIXME: g_thread_join( dir->thread ); Is this necessary? */
        dir->thread = NULL;
    }

    if ( dir->file_listed )
    {
        for ( l = dir->state_callback_list; l; l = l->next )
        {
            ent = ( struct VFSDirStateCallbackEnt* ) l->data;
            if ( ent->stage < 1 )
            {
                ent->func( dir, 1, ent->user_data );
                ent->stage = 1;
            }
        }
    }

    if ( dir->load_complete )
    {
        for ( l = dir->state_callback_list; l; l = l->next )
        {
            ent = ( struct VFSDirStateCallbackEnt* ) l->data;
            if ( ent->stage < 2 )
            {
                ent->func( dir, 2, ent->user_data );
                ent->stage = 2;
            }
        }
    }
    return FALSE;
}

void vfs_dir_add_state_callback( VFSDir* dir,
                                 VFSDirStateCallback func,
                                 gpointer user_data )
{
    struct VFSDirStateCallbackEnt * ent;
    ent = g_slice_new( struct VFSDirStateCallbackEnt );
    ent->func = func;
    ent->user_data = user_data;
    ent->stage = 0;
    dir->state_callback_list = g_slist_prepend( dir->state_callback_list, ent );
    if ( dir->file_listed )
    {
        /* The dir has been loaded */
        /*if ( ! dir->load_notify )*/   /* FIXME: this seems to have problems? */
        {
            dir->load_notify = g_idle_add(
                                   ( GSourceFunc ) vfs_dir_call_state_callback, dir );
        }
    }
}

void vfs_dir_remove_state_callback( VFSDir* dir,
                                    VFSDirStateCallback func,
                                    gpointer user_data )
{
    struct VFSDirStateCallbackEnt * ent;
    GSList* prev;
    GSList* l;

    prev = NULL;
    for ( l = dir->state_callback_list; l; l = l->next )
    {
        ent = ( struct VFSDirStateCallbackEnt* ) l->data;
        if ( ent->func == func && ent->user_data == user_data )
        {
            if ( G_LIKELY( prev ) )
                prev->next = l->next;
            else    /* first item */
                dir->state_callback_list = l->next;
            g_slist_free_1( l );
            g_slice_free( struct VFSDirStateCallbackEnt, ent );
            break;
        }
        prev = l;
    }
}


VFSDir* vfs_get_dir( const char* path )
{
    VFSDir * dir = NULL;
    if ( !dir_hash )
    {
        dir_hash = g_hash_table_new( g_str_hash, g_str_equal );
        xdg_mime_cb_id = xdg_mime_register_reload_callback(
                             on_mime_type_reload, NULL, NULL );
    }
    else
    {
        dir = g_hash_table_lookup( dir_hash, path );
    }
    if ( dir )
    {
        vfs_dir_ref( dir );
    }
    else
    {
        dir = vfs_dir_new();
        g_hash_table_insert( dir_hash, path, dir );
        vfs_dir_load( dir, path );  /* asynchronous operation */
    }
    return dir;
}

gboolean on_thumbnail_idle( VFSDir* dir )
{
    GList * reqs, *l;
    ThumbnailRequest* req;

    g_mutex_lock( dir->thumbnail_mutex );

    reqs = dir->loaded_thumbnails;
    dir->loaded_thumbnails = NULL;
    dir->thumbnail_idle = 0;

    g_mutex_unlock( dir->thumbnail_mutex );

    gdk_threads_enter();
    for ( l = reqs; l ;l = l->next )
    {
        req = l->data;
        g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, req->file );
        vfs_file_info_unref( req->file );
        g_slice_free( ThumbnailRequest, req );
    }
    g_list_free( dir->loaded_thumbnails );
    gdk_threads_leave();

    return FALSE;
}

gpointer load_thumbnail_thread( gpointer user_data )
{
    VFSDir * dir = VFS_DIR( user_data );
    ThumbnailRequest* req;
    VFSFileInfo* file;
    char* full_path;
    int i;
    gboolean load_big;
    gboolean need_update;

    while ( TRUE )
    {
        g_mutex_lock( dir->thumbnail_mutex );
        req = ( ThumbnailRequest* ) g_queue_pop_head( dir->thumbnail_requests );
        if ( G_UNLIKELY( ! req ) )
        {
            g_queue_free( dir->thumbnail_requests );
            dir->thumbnail_requests = NULL;
            g_mutex_unlock( dir->thumbnail_mutex );
            dir->thumbnail_thread = NULL;
            g_cond_signal( dir->thumbnail_cond );
            break;
        }
        g_mutex_unlock( dir->thumbnail_mutex );
        file = req->file;
        need_update = FALSE;
        for ( i = 0; i < 2; ++i )
        {
            if ( ! req->n_requests[ i ] )
                continue;
            load_big = ( i == LOAD_BIG_THUMBNAIL );
            if ( ! vfs_file_info_is_thumbnail_loaded( file, load_big ) )
            {
                full_path = g_build_filename( dir->path,
                                              vfs_file_info_get_name( file ),
                                              NULL );
                vfs_file_info_load_thumbnail( file, full_path, load_big );
                g_free( full_path );
            }
            need_update = TRUE;
        }

        if ( need_update )
        {
            g_mutex_lock( dir->thumbnail_mutex );
            dir->loaded_thumbnails = g_list_append( dir->loaded_thumbnails, req );
            if ( ! dir->thumbnail_idle )
            {
                dir->thumbnail_idle = g_idle_add( ( GSourceFunc ) on_thumbnail_idle, dir );
            }
            g_mutex_unlock( dir->thumbnail_mutex );
        }

        g_thread_yield();
    }
    return NULL;
}

void vfs_dir_request_thumbnail( VFSDir* dir, VFSFileInfo* file, gboolean big )
{
    ThumbnailRequest * req;
    GList* l;
    if ( ! dir->thumbnail_mutex )
    {
        dir->thumbnail_mutex = g_mutex_new();
        dir->thumbnail_cond = g_cond_new();
    }

    g_mutex_lock( dir->thumbnail_mutex );

    if ( ! dir->thumbnail_requests )
    {
        dir->thumbnail_requests = g_queue_new();
    }

    for ( l = dir->thumbnail_requests->head; l; l = l->next )
    {
        req = ( ThumbnailRequest* ) l->data;
        if ( req->file == file )
        {
            if ( big )
                ++req->n_requests[ LOAD_BIG_THUMBNAIL ];
            else
                ++req->n_requests[ LOAD_SMALL_THUMBNAIL ];
            g_mutex_unlock( dir->thumbnail_mutex );
            return ;
        }
    }

    req = g_slice_new0( ThumbnailRequest );
    vfs_file_info_ref( file );
    req->file = file;

    if ( big )
        ++req->n_requests[ LOAD_BIG_THUMBNAIL ];
    else
        ++req->n_requests[ LOAD_SMALL_THUMBNAIL ];

    g_queue_push_tail( dir->thumbnail_requests, req );

    if ( ! dir->thumbnail_thread )
    {
        dir->thumbnail_thread = g_thread_create_full(
                                    load_thumbnail_thread, dir,
                                    0, FALSE, FALSE, G_THREAD_PRIORITY_LOW, NULL );
    }
    g_mutex_unlock( dir->thumbnail_mutex );
}

void vfs_dir_cancel_thumbnail_request( VFSDir* dir, VFSFileInfo* file,
                                       gboolean big )
{
    /* FIXME: This cannot work. :( */
#if 0
    ThumbnailRequest * req;
    GList* l;

    if ( ! dir->thumbnail_mutex || ! dir->thumbnail_requests )
        return ;
    g_mutex_lock( dir->thumbnail_mutex );
    for ( l = dir->thumbnail_requests->head; l; l = l->next )
    {
        req = ( ThumbnailRequest* ) l->data;
        if ( req->file == file )
        {
            if ( big )
                --req->n_requests[ LOAD_BIG_THUMBNAIL ];
            else
                --req->n_requests[ LOAD_SMALL_THUMBNAIL ];
            /* FIXME: remove the request if it doesn't load any thumbnail? */
        }
    }
    g_mutex_unlock( dir->thumbnail_mutex );
#endif
}

static void reload_mime_type( char* key, VFSDir* dir, gpointer user_data )
{
    GList* l;
    VFSFileInfo* file;
    char* full_path;

    if( G_UNLIKELY( ! dir || ! dir->file_list ) )
        return;
    gdk_threads_enter();
    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        full_path = g_build_filename( dir->path,
                                      vfs_file_info_get_name( file ), NULL );
        vfs_file_info_reload_mime_type( file, full_path );
        g_free( full_path );
    }

    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        g_signal_emit( dir, signals[FILE_CHANGED_SIGNAL], 0, file );
    }
    g_mutex_unlock( dir->mutex );
    gdk_threads_leave();
}

static void on_mime_type_reload( gpointer user_data )
{
    if( ! dir_hash )
        return;
    g_hash_table_foreach( dir_hash, (GHFunc)reload_mime_type, NULL );
}

