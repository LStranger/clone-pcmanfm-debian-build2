/*
*  C Implementation: vfs-volume
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
#include <config.h>
#endif

#include "vfs-volume.h"
#include "glib-mem.h"

#ifdef HAVE_HAL
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libhal.h>
#include <libhal-storage.h>

struct _VFSVolume
{
    LibHalDrive* drive;
    LibHalVolume* vol;
    char* udi;
    char* disp_name;
    char* icon;
};

typedef struct _VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    gpointer user_data;
}
VFSVolumeCallbackData;

static LibHalContext* hal_context = NULL;
static DBusConnection * dbus_connection = NULL;

/* FIXME: Will it be better if we use GSList? */
static GArray* volumes = NULL;

static GArray* callbacks = NULL;

static call_callbacks( VFSVolume* vol, VFSVolumeState state )
{
    int i;
    VFSVolumeCallbackData* e;

    if ( !callbacks )
        return ;

    e = ( VFSVolumeCallbackData* ) callbacks->data;
    for ( i = 0; i < callbacks->len; ++i )
    {
        ( *e[ i ].cb ) ( vol, state, e[ i ].user_data );
    }
}

static VFSVolume* vfs_volume_new( LibHalContext *ctx, LibHalVolume * vol )
{
    VFSVolume * volume;
    const char* storage_udi;
    LibHalDrive * drive;

    storage_udi = libhal_volume_get_storage_device_udi( vol );
    drive = libhal_drive_from_udi( ctx, storage_udi );

    volume = g_slice_new0( VFSVolume );
    volume->vol = vol;
    volume->drive = drive;
    volume->udi = g_strdup( libhal_volume_get_udi( vol ) );

    return volume;
}

void vfs_volume_free( VFSVolume* volume )
{
    if ( volume->drive )
        libhal_drive_free( volume->drive );
    if ( volume->vol )
        libhal_volume_free( volume->vol );
    if ( volume->disp_name )
        g_free( volume->disp_name );
    if ( volume->udi )
        g_free( volume->udi );
    g_slice_free( VFSVolume, volume );
}

static void on_hal_device_added( LibHalContext *ctx, const char *udi )
{
    LibHalVolume * vol;

    VFSVolume* volume;

    vol = libhal_volume_from_udi( ctx, udi );
    if ( !vol )
        return ;

    if ( LIBHAL_VOLUME_USAGE_MOUNTABLE_FILESYSTEM == libhal_volume_get_fsusage( vol ) )
    {

        volume = vfs_volume_new( ctx, vol );
        volumes = g_array_append_val( volumes, volume );
        call_callbacks( volume, VFS_VOLUME_ADDED );
    }
    else
    {
        libhal_volume_free( vol );
    }
}

static void on_hal_device_removed( LibHalContext *ctx, const char *udi )
{
    int i;
    VFSVolume **v, *volume;

    if ( !volumes )
        return ;

    v = ( VFSVolume** ) volumes->data;
    for ( i = 0; i < volumes->len; ++i )
    {
        if ( 0 == strcmp( v[ i ] ->udi, udi ) )
        {
            volume = v[ i ];
            volumes = g_array_remove_index( volumes, i );
            call_callbacks( volume, VFS_VOLUME_REMOVED );
            vfs_volume_free( volume );
            break;
        }
    }
}

static void on_hal_property_modified( LibHalContext *ctx,
                                      const char *udi,
                                      const char *key,
                                      dbus_bool_t is_removed,
                                      dbus_bool_t is_added )
{
    /* FIXME: on_hal_property_modified: this func is not efficient, nor correct */

    int i;
    VFSVolume** v;

    if ( !volumes )
        return ;

    v = ( VFSVolume** ) volumes->data;
    for ( i = 0; i < volumes->len; ++i )
    {
        if ( 0 == strcmp( libhal_volume_get_udi( v[ i ] ->vol ), udi ) )
        {
            libhal_volume_free( v[ i ] ->vol );
            v[ i ] ->vol = libhal_volume_from_udi( ctx, udi );
            call_callbacks( v[ i ], VFS_VOLUME_CHANGED );
            break;
        }
    }
}

static void on_hal_condition ( LibHalContext *ctx,
                               const char *udi,
                               const char *condition_name,
                               const char *condition_detail )
{
    /* FIXME: on_hal_condition: this func is not efficient, nor correct */
    /* g_debug( "condition changed: %s, %s, %s\n", condition_name, condition_detail, udi ); */
    int i;
    VFSVolume** v;

    if ( !volumes )
        return ;

    v = ( VFSVolume** ) volumes->data;
    for ( i = 0; i < volumes->len; ++i )
    {
        if ( 0 == strcmp( libhal_volume_get_udi( v[ i ] ->vol ), udi ) )
        {
            libhal_volume_free( v[ i ] ->vol );
            v[ i ] ->vol = libhal_volume_from_udi( ctx, udi );
            call_callbacks( v[ i ], VFS_VOLUME_CHANGED );
            break;
        }
    }
}

/* This function is taken from gnome volumen manager */
static dbus_bool_t
hal_mainloop_integration ( LibHalContext *ctx, DBusError *error )
{
    dbus_connection = dbus_bus_get ( DBUS_BUS_SYSTEM, error );

    if ( dbus_error_is_set ( error ) )
        return FALSE;

    dbus_connection_setup_with_g_main ( dbus_connection, NULL );
    libhal_ctx_set_dbus_connection ( ctx, dbus_connection );
    return TRUE;
}

gboolean vfs_volume_init()
{
    DBusError error;

    char** devices;
    char** device;
    int n;

    if ( hal_context )
        return TRUE;

    hal_context = libhal_ctx_new ();

    if ( hal_context )
    {
        dbus_error_init ( &error );
        if ( hal_mainloop_integration ( hal_context, &error ) )
        {
            libhal_ctx_set_device_added ( hal_context, on_hal_device_added );
            libhal_ctx_set_device_removed ( hal_context, on_hal_device_removed );
            libhal_ctx_set_device_property_modified ( hal_context, on_hal_property_modified );
            // libhal_ctx_set_device_new_capability (ctx, on_hal_device_new_capability);
            // libhal_ctx_set_device_lost_capability (ctx, on_hal_device_lost_capability);
            libhal_ctx_set_device_condition( hal_context, on_hal_condition );

            if ( libhal_ctx_init ( hal_context, &error ) )
            {
                devices = libhal_find_device_by_capability ( hal_context, "volume", &n, &error );
                if ( devices )
                {
                    volumes = g_array_sized_new( FALSE, FALSE, sizeof( VFSVolume* ), n + 4 );
                    for ( device = devices; *device; ++device )
                    {
                        on_hal_device_added( hal_context, *device );
                    }
                    libhal_free_string_array ( devices );
                }

                if ( libhal_device_property_watch_all ( hal_context, &error ) )
                {
                    dbus_error_free ( &error );
                    return TRUE;
                }
                else
                {
                    g_warning ( "%s\n", error.message );
                }
            }
            else
            {
                g_warning ( "%s\n", error.message );
            }
        }
        libhal_ctx_free ( hal_context );
        hal_context = NULL;
        dbus_error_free ( &error );
    }
    else
    {
        g_warning ( "%s\n", error.message );
    }
    return FALSE;
}

gboolean vfs_volume_clean()
{
    int i;
    VFSVolume** vols;

    if ( callbacks )
        g_array_free( callbacks, TRUE );

    if ( volumes )
    {
        vols = ( VFSVolume** ) volumes->data;
        for ( i = 0; i < volumes->len; ++i )
            vfs_volume_free( vols[ i ] );
        g_array_free( volumes, TRUE );
    }

    if ( hal_context )
    {
        libhal_ctx_shutdown( hal_context, NULL );
        libhal_ctx_free ( hal_context );
    }
    if ( dbus_connection )
        dbus_connection_unref( dbus_connection );
}

void vfs_volume_add_callback( VFSVolumeCallback cb, gpointer user_data )
{
    VFSVolumeCallbackData e;
    if ( !cb )
        return ;

    if ( !callbacks )
        callbacks = g_array_sized_new( FALSE, FALSE, sizeof( VFSVolumeCallbackData ), 8 );
    e.cb = cb;
    e.user_data = user_data;
    callbacks = g_array_append_val( callbacks, e );
}

void vfs_volume_remove_callback( VFSVolumeCallback cb, gpointer user_data )
{
    int i;
    VFSVolumeCallbackData* e;

    if ( !callbacks )
        return ;

    e = ( VFSVolumeCallbackData* ) callbacks->data;
    for ( i = 0; i < callbacks->len; ++i )
    {
        if ( e[ i ].cb == cb && e[ i ].user_data == user_data )
        {
            callbacks = g_array_remove_index_fast( callbacks, i );
            if ( callbacks->len > 8 )
                g_array_set_size( callbacks, 8 );
            break;
        }
    }
}

gboolean vfs_volume_mount( VFSVolume* vol )
{
    char cmd[ 1024 ];
    int ret;
    g_snprintf( cmd, 1024, "pmount-hal %s", libhal_volume_get_udi( vol->vol ) );
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
        return FALSE;
    return !ret;
}

gboolean vfs_volume_umount( VFSVolume *vol )
{
    char cmd[ 1024 ];
    int ret;
    g_snprintf( cmd, 1024, "pumount %s", libhal_volume_get_mount_point( vol->vol ) );
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
        return FALSE;
    return !ret;
}

gboolean vfs_volume_eject( VFSVolume *vol )
{
    char cmd[ 1024 ];
    int ret;
    g_snprintf( cmd, 1024, "eject %s", libhal_volume_get_device_file( vol->vol ) );
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
        return FALSE;
    return !ret;
}

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    char * disp_name;
    LibHalStoragePolicy* policy;

    if ( vol->disp_name )
        return vol->disp_name;

    policy = libhal_storage_policy_new();
    disp_name = libhal_volume_policy_compute_display_name(
                    vol->drive, vol->vol, policy );
    if ( vol->disp_name )
        g_free( vol->disp_name );
    vol->disp_name = disp_name;
    libhal_storage_policy_free( policy );

    return disp_name;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    return libhal_volume_get_mount_point( vol->vol );
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    return libhal_volume_get_device_file( vol->vol );
}

const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    const char * fs;
    LibHalStoragePolicy* policy;
    policy = libhal_storage_policy_new();
    fs = libhal_volume_policy_get_mount_fs( vol->drive, vol->vol, policy );
    libhal_storage_policy_free( policy );
    return fs;
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    char * icon;
    LibHalStoragePolicy* policy;

    if ( vol->icon )
        return vol->icon;

    policy = libhal_storage_policy_new();
    icon = libhal_volume_policy_compute_icon_name(
               vol->drive, vol->vol, policy );
    if ( !icon )
    {
        switch ( libhal_drive_get_type( vol->drive ) )
        {
        case LIBHAL_DRIVE_TYPE_CDROM:
            icon = "gnome-dev-cdrom";
            break;
        case LIBHAL_DRIVE_TYPE_REMOVABLE_DISK:
            if ( LIBHAL_DRIVE_BUS_USB == libhal_drive_get_bus( vol->drive ) )
                icon = "gnome-dev-removable-usb";
            else
                icon = "gnome-dev-removable";
            break;
        default:
            if ( LIBHAL_DRIVE_BUS_USB == libhal_drive_get_bus( vol->drive ) )
                icon = "gnome-dev-harddisk-usb";
            else
                icon = "gnome-dev-harddisk";
        }
    }
    vol->icon = icon;
    libhal_storage_policy_free( policy );

    return icon;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    return ( gboolean ) libhal_drive_uses_removable_media( vol->drive );
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    return ( gboolean ) libhal_volume_is_mounted( vol->vol ) &&
           libhal_volume_get_mount_point( vol );
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    return ( unsigned short ) libhal_drive_requires_eject( vol->drive );
}

VFSVolume** vfs_volume_get_all_volumes( int* num )
{
    if ( num )
        * num = volumes ? volumes->len : 0;
    return ( VFSVolume** ) volumes->data;
}

#else   /* Build without HAL */

struct _VFSVolume
{
    char* disp_name;
};

gboolean vfs_volume_init()
{
    return TRUE;
}

gboolean vfs_volume_clean()
{
    return TRUE;
}

VFSVolume** vfs_volume_get_all_volumes( int* num )
{
    if( num )
        *num = 0;
    return NULL;
}

void vfs_volume_add_callback( VFSVolumeCallback cb, gpointer user_data )
{

}

void vfs_volume_remove_callback( VFSVolumeCallback cb, gpointer user_data )
{

}

gboolean vfs_volume_mount( VFSVolume* vol )
{
    return TRUE;
}

gboolean vfs_volume_umount( VFSVolume *vol )
{
    return TRUE;
}

gboolean vfs_volume_eject( VFSVolume *vol )
{
    return TRUE;
}

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    return NULL;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    return FALSE;
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    return FALSE;
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    return FALSE;
}

#endif  /*  #ifdef HAVE_HAL  */


