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
#include <glib/gi18n.h>

#ifdef HAVE_HAL
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libhal.h> 
/* #include <libhal-storage.h> */

struct _VFSVolume
{
    char* udi;
    char* storage_udi;
    char* disp_name;
    char* icon;
    char* mount_point;
    gboolean is_mounted;
    gboolean is_hotpluggable;
    gboolean is_removable;
    gboolean requires_eject;
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

static void vfs_volume_set_from_udi( VFSVolume* volume, const char* udi )
{
    g_free( volume->udi );
    volume->udi = g_strdup( udi );
    g_free( volume->mount_point );
    volume->mount_point = libhal_device_get_property_string ( hal_context,
                                                              udi,
                                                              "volume.mount_point",
                                                              NULL );
    if ( volume->mount_point && !*volume->mount_point )
    {
        libhal_free_string( volume->mount_point );
        volume->mount_point = NULL;
    }

    g_free( volume->storage_udi );
    volume->storage_udi = libhal_device_get_property_string ( hal_context,
                                                              udi,
                                                              "block.storage_device",
                                                              NULL );

    if ( G_UNLIKELY( ! volume->mount_point ) )
        volume->is_mounted = FALSE;
    else
        volume->is_mounted = libhal_device_get_property_bool ( hal_context,
                                                               udi,
                                                               "volume.is_mounted",
                                                               NULL );

    volume->is_hotpluggable = libhal_device_get_property_bool ( hal_context,
                                                                volume->storage_udi,
                                                                "storage.hotpluggable",
                                                                NULL );

    volume->requires_eject = libhal_device_get_property_bool ( hal_context,
                                                               volume->storage_udi,
                                                               "storage.requires_eject",
                                                               NULL );

    volume->is_removable = libhal_device_get_property_bool ( hal_context,
                                                             volume->storage_udi,
                                                             "storage.removable",
                                                             NULL );
}

static VFSVolume* vfs_volume_new( const char* udi )
{
    VFSVolume * volume;
    const char* storage_udi;

    volume = g_slice_new0( VFSVolume );
    vfs_volume_set_from_udi( volume, udi );

    return volume;
}

void vfs_volume_free( VFSVolume* volume )
{
    g_free( volume->udi );
    if ( volume->mount_point )
        libhal_free_string( volume->mount_point );
    if ( volume->storage_udi )
        libhal_free_string( volume->storage_udi );
    g_free( volume->disp_name );
    g_slice_free( VFSVolume, volume );
}

static void vfs_volume_update_disp_name( VFSVolume* vol )
{
    char * disp_name = NULL;
    char* label;

    g_free( vol->disp_name );

    if( G_UNLIKELY(vol->mount_point && 
        0 == strcmp("/", vol->mount_point)) )
    {
        vol->disp_name = g_strdup(_("File System"));
        return;
    }
    label = libhal_device_get_property_string ( hal_context, vol->udi, "volume.label", NULL );
    if ( G_LIKELY( label ) )
    {
        if ( *label )
            disp_name = g_strdup( label );
        libhal_free_string( label );
    }
    /* FIXME: Display better names for cdroms and other removable media */
    if ( ! disp_name )
    {
        char * device;
        device = libhal_device_get_property_string ( hal_context, vol->udi, "block.device", NULL );
        if ( G_LIKELY( device ) )
        {
            if ( *device )
                disp_name = g_path_get_basename( device );
            libhal_free_string( device );
        }
    }
    vol->disp_name = disp_name;
}

static void on_hal_device_added( LibHalContext *ctx, const char *udi )
{
    VFSVolume * volume;
    char* fs;

    /* g_debug("device added: %s", udi); */

    if ( libhal_device_get_property_bool ( ctx, udi, "volume.ignore", NULL ) )
        return ;

    if ( ! libhal_device_property_exists ( ctx, udi, "volume.fsusage", NULL ) )
        return ;

    fs = libhal_device_get_property_string ( ctx, udi, "volume.fsusage", NULL );
    if ( !fs || ( ( strcmp ( fs, "filesystem" ) != 0 ) && ( strcmp ( fs, "crypto" ) != 0 ) ) )
    {
        libhal_free_string ( fs );
        return ;
    }

    volume = vfs_volume_new( udi );
    /* FIXME: sort items */
    volumes = g_array_append_val( volumes, volume );
    call_callbacks( volume, VFS_VOLUME_ADDED );
}

static void on_hal_device_removed( LibHalContext *ctx, const char *udi )
{
    int i;
    VFSVolume **v, *volume;

    if ( !volumes )
        return ;
    /* g_debug("device removed: %s", udi); */

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
        if ( 0 == strcmp( v[ i ] ->udi , udi ) )
        {
            vfs_volume_set_from_udi( v[ i ], udi );
            vfs_volume_update_disp_name( v[ i ] );
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
        if ( 0 == strcmp( v[ i ] ->udi , udi ) )
        {
            vfs_volume_set_from_udi( v[ i ], udi );
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
            /*
                libhal_ctx_set_device_new_capability (ctx, on_hal_device_new_capability);
                libhal_ctx_set_device_lost_capability (ctx, on_hal_device_lost_capability);
            */
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
                else
                    volumes = g_array_sized_new( FALSE, FALSE, sizeof( VFSVolume* ), 4 );

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
    char* mount;
    mount = g_find_program_in_path( "pmount-hal" );
    if ( mount )
    {
        g_snprintf( cmd, 1024, "%s %s", mount, vol->udi );
        g_free( mount );
    }
    else
    {
        char* device;
        device = libhal_device_get_property_string ( hal_context,
                                                     vol->udi,
                                                     "block.device",
                                                     NULL );

        mount = g_find_program_in_path( "pmount" );
        if ( mount )
        {
            g_snprintf( cmd, 1024, "%s %s", mount, device );
            g_free( mount );
        }
        else
            g_snprintf( cmd, 1024, "mount %s", device );
        libhal_free_string( device );
    }
    /* g_debug( "cmd: %s", cmd ); */
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
        return FALSE;
    /* dirty hack :-( */
    if ( ! vol->mount_point )
    {
        vol->mount_point = libhal_device_get_property_string ( hal_context,
                                                               vol->udi,
                                                               "volume.mount_point",
                                                               NULL );
        if ( vol->mount_point && !*vol->mount_point )
        {
            libhal_free_string( vol->mount_point );
            vol->mount_point = NULL;
        }
    }
    return !ret;
}

gboolean vfs_volume_umount( VFSVolume *vol )
{
    char cmd[ 1024 ];
    int ret;
    if ( !vol->mount_point )
        return FALSE;
    g_snprintf( cmd, 1024, "pumount \"%s\"", vol->mount_point );
    /* g_debug( "cmd: %s", cmd ); */
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
        return FALSE;
    return !ret;
}

gboolean vfs_volume_eject( VFSVolume *vol )
{
    char cmd[ 1024 ];
    int ret;
    char* device;

    device = libhal_device_get_property_string ( hal_context,
                                                 vol->udi,
                                                 "block.device",
                                                 NULL );
    if ( !device )
        return FALSE;

    g_snprintf( cmd, 1024, "eject \"%s\"", device );
    libhal_free_string( device );
    /* g_debug( "cmd: %s", cmd ); */
    if ( !g_spawn_command_line_sync( cmd, NULL, NULL, &ret, NULL ) )
    {
        return FALSE;
    }
    return !ret;
}

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    char * disp_name;

    if ( vol->disp_name )
        return vol->disp_name;
    vfs_volume_update_disp_name( vol );
    /*
    policy = libhal_storage_policy_new();
    disp_name = libhal_volume_policy_compute_display_name(
                    vol->drive, vol->vol, policy );
    if ( vol->disp_name )
        g_free( vol->disp_name );
    vol->disp_name = disp_name;
    libhal_storage_policy_free( policy );
    */ 
    return vol->disp_name;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    return vol->mount_point;
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    return NULL; /* FIXME: vfs_volume_get_device is not implemented */
}

const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    const char * fs = NULL;
    return fs;  /* FIXME: vfs_volume_get_fstype is not implemented */
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    char * icon;
    char* type;

    if ( vol->icon )
        return vol->icon;

    icon = NULL;
    type = libhal_device_get_property_string ( hal_context,
                                               vol->storage_udi,
                                               "storage.drive_type",
                                               NULL );
    if ( type )
    {
        /* FIXME: Show different icons for different types of discs */
        if ( 0 == strcmp( type, "cdrom" ) )
        {
            gboolean is_dvd = libhal_device_get_property_bool ( hal_context,
                                                                vol->storage_udi,
                                                                "storage.cdrom.dvd",
                                                                NULL );
            icon = is_dvd ? "gnome-dev-dvd" : "gnome-dev-cdrom";
        }
        else if ( 0 == strcmp( type, "floppy" ) )
            icon = "gnome-dev-floppy";
        libhal_free_string( type );
    }

    if ( !icon )
    {
        if ( vol->is_removable )
        {
            char * bus;
            bus = libhal_device_get_property_string ( hal_context,
                                                      vol->storage_udi,
                                                      "storage.bus",
                                                      NULL );
            if ( bus )
            {
                if ( 0 == strcmp( bus, "usb" ) )
                    icon = "gnome-dev-removable-usb";
                libhal_free_string( bus );
            }
            if ( ! icon )
                icon = "gnome-dev-removable";
        }

        if ( ! icon )
            icon = "gnome-dev-harddisk";
    }

    /*
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
    */

    vol->icon = icon;
    return icon;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    return vol->is_removable;
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    return vol->is_mounted;
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    return vol->requires_eject;
}

VFSVolume** vfs_volume_get_all_volumes( int* num )
{
    if( G_UNLIKELY( ! volumes ) )
    {
        if( num )
            *num = 0;
        return NULL;
    }
    if ( num )
        * num = volumes->len;
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
    if ( num )
        * num = 0;
    return NULL;
}

void vfs_volume_add_callback( VFSVolumeCallback cb, gpointer user_data )
{}

void vfs_volume_remove_callback( VFSVolumeCallback cb, gpointer user_data )
{}

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


