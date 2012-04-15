/*
*  C Implementation: vfs-monitor
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vfs-file-monitor.h"
#include <sys/types.h>  /* for stat */
#include <sys/stat.h>

#include <stdlib.h>
#include <string.h>

#include "glib-mem.h"

typedef struct
{
    VFSFileMonitorCallback callback;
    gpointer user_data;
}
VFSFileMonitorCallbackEntry;

static GHashTable* monitor_hash = NULL;
static FAMConnection fam;
static GIOChannel* fam_io_channel = NULL;
static guint fam_io_watch = 0;

/* event handler of all FAM events */
static gboolean on_fam_event( GIOChannel *channel,
                              GIOCondition cond,
                              gpointer user_data );


static gboolean connect_to_fam()
{
    if ( FAMOpen( &fam ) )
    {
        fam_io_channel = NULL;
        fam.fd = -1;
        g_warning( "There is no FAM/gamin server\n" );
        return FALSE;
    }
#if HAVE_FAMNOEXISTS
    /*
    * Disable the initital directory content loading.
    * This can greatly speed up directory loading, but
    * unfortunately, it's not compatible with original FAM.
    */
    FAMNoExists( &fam );  /* This is an extension of gamin */
#endif

    fam_io_channel = g_io_channel_unix_new( fam.fd );
    g_io_channel_set_encoding( fam_io_channel, NULL, NULL );
    g_io_channel_set_buffered( fam_io_channel, FALSE );

    fam_io_watch = g_io_add_watch( fam_io_channel,
                                   G_IO_IN | G_IO_HUP,
                                   on_fam_event,
                                   NULL );
    return TRUE;
}

static void disconnect_from_fam()
{
    if ( fam_io_channel )
    {
        g_io_channel_unref( fam_io_channel );
        fam_io_channel = NULL;
        g_source_remove( fam_io_watch );

        FAMClose( &fam );
    }
}

/* final cleanup */
void vfs_file_monitor_clean()
{
    disconnect_from_fam();
    if ( monitor_hash )
    {
        g_hash_table_destroy( monitor_hash );
        monitor_hash = NULL;
    }
}

/*
* Init monitor:
* Establish connection with gamin/fam.
*/
gboolean vfs_file_monitor_init()
{
    monitor_hash = g_hash_table_new( g_str_hash, g_str_equal );
    if ( ! connect_to_fam() )
        return FALSE;
    return TRUE;
}

VFSFileMonitor* vfs_file_monitor_add( const char* path,
                                      VFSFileMonitorCallback cb,
                                      gpointer user_data )
{
    VFSFileMonitor * monitor;
    VFSFileMonitorCallbackEntry cb_ent;
    gboolean add_new = FALSE;
    struct stat file_stat;

    if ( ! monitor_hash )
    {
        if ( !vfs_file_monitor_init() )
            return NULL;
    }
    monitor = ( VFSFileMonitor* ) g_hash_table_lookup ( monitor_hash, path );
    if ( ! monitor )
    {
        monitor = g_slice_new0( VFSFileMonitor );
        monitor->path = g_strdup( path );
        monitor->callbacks = g_array_new ( FALSE, FALSE, sizeof( VFSFileMonitorCallbackEntry ) );
        g_hash_table_insert ( monitor_hash,
                              path,
                              monitor );
        if ( lstat( path, &file_stat ) != -1 )
        {
            if ( S_ISDIR( file_stat.st_mode ) )
            {
                FAMMonitorDirectory( &fam,
                                     path,
                                     &monitor->request,
                                     monitor );
            }
            else
            {
                FAMMonitorFile( &fam,
                                path,
                                &monitor->request,
                                monitor );
            }
        }
    }
    if ( cb )
    { /* Install a callback */
        cb_ent.callback = cb;
        cb_ent.user_data = user_data;
        monitor->callbacks = g_array_append_val( monitor->callbacks, cb_ent );
    }
    ++monitor->n_ref;
    return monitor;
}

void vfs_file_monitor_remove( VFSFileMonitor* fm,
                              VFSFileMonitorCallback cb,
                              gpointer user_data )
{
    int i;
    VFSFileMonitorCallbackEntry* callbacks;
    if ( cb && fm->callbacks )
    {
        callbacks = ( VFSFileMonitorCallbackEntry* ) fm->callbacks->data;
        for ( i = 0; i < fm->callbacks->len; ++i )
        {
            if ( callbacks[ i ].callback == cb && callbacks[ i ].user_data == user_data )
            {
                fm->callbacks = g_array_remove_index_fast ( fm->callbacks, i );
                break;
            }
        }
    }
    --fm->n_ref;
    if ( 0 >= fm->n_ref )
    {
        FAMCancelMonitor( &fam, &fm->request );
        g_hash_table_remove( monitor_hash, fm->path );
        g_free( fm->path );
        g_array_free( fm->callbacks, TRUE );
        g_slice_free( VFSFileMonitor, fm );
    }
}

static void reconnect_fam( gpointer key,
                           gpointer value,
                           gpointer user_data )
{
    struct stat file_stat;
    VFSFileMonitor* monitor = ( VFSFileMonitor* ) value;
    const char* path = ( const char* ) key;
    if ( lstat( path, &file_stat ) != -1 )
    {
        if ( S_ISDIR( file_stat.st_mode ) )
        {
            FAMMonitorDirectory( &fam,
                                 path,
                                 &monitor->request,
                                 monitor );
        }
        else
        {
            FAMMonitorFile( &fam,
                            path,
                            &monitor->request,
                            monitor );
        }
    }
}

/* event handler of all FAM events */
static gboolean on_fam_event( GIOChannel *channel,
                              GIOCondition cond,
                              gpointer user_data )
{
    FAMEvent evt;
    VFSFileMonitor* monitor = NULL;
    VFSFileMonitorCallbackEntry* cb;
    VFSFileMonitorCallback func;
    int i;

    if ( cond & G_IO_HUP )
    {
        disconnect_from_fam();
        if ( g_hash_table_size ( monitor_hash ) > 0 )
        {
            /*
              Disconnected from FAM server, but there are still monitors.
              This may be caused by crash of FAM server.
              So we have to reconnect to FAM server.
            */
            connect_to_fam();
            g_hash_table_foreach( monitor_hash, ( GHFunc ) reconnect_fam, NULL );
        }
        return TRUE; /* don't need to remove the event source since
                            it has been removed by disconnect_from_fam(). */
    }

    while ( FAMPending( &fam ) )
    {
        if ( FAMNextEvent( &fam, &evt ) > 0 )
        {
            monitor = ( VFSFileMonitor* ) evt.userdata;
            switch ( evt.code )
            {
            case FAMCreated:
            case FAMDeleted:
            case FAMChanged:
                /* Call the callback functions */
                if ( monitor->callbacks && monitor->callbacks->len )
                {
                    cb = ( VFSFileMonitorCallbackEntry* ) monitor->callbacks->data;
                    for ( i = 0; i < monitor->callbacks->len; ++i )
                    {
                        func = cb[ i ].callback;
                        func( monitor, evt.code, evt.filename, cb[ i ].user_data );
                    }
                }
                break;
            default:
                return TRUE;  /* Other events are not supported */
            }
        }
    }
    return TRUE;
}

