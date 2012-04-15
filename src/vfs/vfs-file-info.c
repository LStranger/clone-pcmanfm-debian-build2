/*
*  C Implementation: vfs-file-info
*
* Description: File information
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-file-info.h"
#include "xdgmime.h"
#include <glib.h>
#include "glib-mem.h"
#include <glib/gi18n.h>
#include <grp.h> /* Query group name */
#include <pwd.h> /* Query user name */
#include <string.h>

#include "vfs-app-desktop.h"
#include "md5.h"    /* for thumbnails */

#define _MAX( x, y )     (x > y ? x : y)

static int big_thumb_size = 48, small_thumb_size = 20;
static gboolean utf8_file_name = FALSE;

void vfs_file_info_set_utf8_filename( gboolean is_utf8 )
{
    utf8_file_name = is_utf8;
}

VFSFileInfo* vfs_file_info_new ()
{
    VFSFileInfo * fi = g_slice_new0( VFSFileInfo );
    fi->n_ref = 1;
    return fi;
}

static void vfs_file_info_clear( VFSFileInfo* fi )
{
    if ( fi->disp_name && fi->disp_name != fi->name )
    {
        g_free( fi->disp_name );
        fi->disp_name = NULL;
    }
    if ( fi->name )
    {
        g_free( fi->name );
        fi->name = NULL;
    }
    if ( fi->disp_size )
    {
        g_free( fi->disp_size );
        fi->disp_size = NULL;
    }
    if ( fi->disp_owner )
    {
        g_free( fi->disp_owner );
        fi->disp_owner = NULL;
    }
    if ( fi->disp_mtime )
    {
        g_free( fi->disp_mtime );
        fi->disp_mtime = NULL;
    }
    if ( fi->big_thumbnail )
    {
        gdk_pixbuf_unref( fi->big_thumbnail );
        fi->big_thumbnail = NULL;
    }
    if ( fi->small_thumbnail )
    {
        gdk_pixbuf_unref( fi->small_thumbnail );
        fi->small_thumbnail = NULL;
    }

    fi->disp_perm[ 0 ] = '\0';

    if ( fi->mime_type )
    {
        vfs_mime_type_unref( fi->mime_type );
        fi->mime_type = NULL;
    }
    fi->flags = VFS_FILE_INFO_NONE;
}

void vfs_file_info_ref( VFSFileInfo* fi )
{
    ++fi->n_ref;
}

void vfs_file_info_unref( VFSFileInfo* fi )
{
    --fi->n_ref;
    if ( fi->n_ref <= 0 )
    {
        vfs_file_info_clear( fi );
        g_slice_free( VFSFileInfo, fi );
    }
}

gboolean vfs_file_info_get( VFSFileInfo* fi,
                            const char* file_path,
                            const char* base_name )
{
    struct stat file_stat;
    vfs_file_info_clear( fi );

    if ( base_name )
        fi->name = g_strdup( base_name );
    else
        fi->name = g_path_get_basename( file_path );

    if ( lstat( file_path, &file_stat ) == 0 )
    {
        /* This is time-consuming but can save much memory */
        fi->mode = file_stat.st_mode;
        fi->dev = file_stat.st_dev;
        fi->uid = file_stat.st_uid;
        fi->gid = file_stat.st_gid;
        fi->size = file_stat.st_size;
        fi->mtime = file_stat.st_mtime;
        fi->atime = file_stat.st_atime;
        fi->blksize = file_stat.st_blksize;
        fi->blocks = file_stat.st_blocks;

        if ( G_LIKELY( utf8_file_name && g_utf8_validate ( fi->name, -1, NULL ) ) )
        {
            fi->disp_name = fi->name;   /* Don't duplicate the name and save memory */
        }
        else
        {
            fi->disp_name = g_filename_display_name( fi->name );
        }
        fi->mime_type = vfs_mime_type_get_from_file( file_path,
                                                     fi->disp_name,
                                                     &file_stat );
        return TRUE;
    }
    else
        fi->mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
    return FALSE;
}

const char* vfs_file_info_get_name( VFSFileInfo* fi )
{
    return fi->name;
}

/* Get displayed name encoded in UTF-8 */
const char* vfs_file_info_get_disp_name( VFSFileInfo* fi )
{
    return fi->disp_name;
}

void vfs_file_info_set_disp_name( VFSFileInfo* fi, const char* name )
{
    if ( fi->disp_name && fi->disp_name != fi->name )
        g_free( fi->disp_name );
    fi->disp_name = g_strdup( name );
}

void vfs_file_info_set_name( VFSFileInfo* fi, const char* name )
{
    g_free( fi->name );
    fi->name = g_strdup( name );
}

off_t vfs_file_info_get_size( VFSFileInfo* fi )
{
    return fi->size;
}

const char* vfs_file_info_get_disp_size( VFSFileInfo* fi )
{
    if ( G_UNLIKELY( !fi->disp_size ) )
    {
        char buf[ 64 ];
        file_size_to_string( buf, fi->size );
        fi->disp_size = g_strdup( buf );
    }
    return fi->disp_size;
}

off_t vfs_file_info_get_blocks( VFSFileInfo* fi )
{
    return fi->blocks;
}

VFSMimeType* vfs_file_info_get_mime_type( VFSFileInfo* fi )
{
    vfs_mime_type_ref( fi->mime_type );
    return fi->mime_type;
}

void vfs_file_info_reload_mime_type( VFSFileInfo* fi,
                                     const char* full_path )
{
    VFSMimeType * old_mime_type;
    struct stat file_stat;

    /* convert VFSFileInfo to struct stat */
    /* In current implementation, only st_mode is used in
       mime-type detection, so let's save some CPU cycles
       and don't copy unused fields.
    */
    file_stat.st_mode = fi->mode;
    /*
    file_stat.st_dev = fi->dev;
    file_stat.st_uid = fi->uid;
    file_stat.st_gid = fi->gid;
    file_stat.st_size = fi->size;
    file_stat.st_mtime = fi->mtime;
    file_stat.st_atime = fi->atime;
    file_stat.st_blksize = fi->blksize;
    file_stat.st_blocks = fi->blocks;
    */
    old_mime_type = fi->mime_type;
    fi->mime_type = vfs_mime_type_get_from_file( full_path,
                                                 fi->name, &file_stat );
    vfs_mime_type_unref( old_mime_type );  /* FIXME: is vfs_mime_type_unref needed ?*/
}

const char* vfs_file_info_get_mime_type_desc( VFSFileInfo* fi )
{
    return vfs_mime_type_get_description( fi->mime_type );
}

GdkPixbuf* vfs_file_info_get_big_icon( VFSFileInfo* fi )
{
    /* get special icons for special files, especially for
       some desktop icons */

    if ( G_UNLIKELY( fi->flags != VFS_FILE_INFO_NONE ) )
    {
        int w, h;
        int icon_size;
        vfs_mime_type_get_icon_size( &icon_size, NULL );
        if ( fi->big_thumbnail )
        {
            w = gdk_pixbuf_get_width( fi->big_thumbnail );
            h = gdk_pixbuf_get_height( fi->big_thumbnail );
        }
        else
            w = h = 0;

        if ( _MAX( w, h ) != icon_size )
        {
            char * icon_name = NULL;
            if ( fi->big_thumbnail )
            {
                icon_name = ( char* ) g_object_steal_data(
                                G_OBJECT(fi->big_thumbnail), "name" );
                gdk_pixbuf_unref( fi->big_thumbnail );
                fi->big_thumbnail = NULL;
            }
            if ( G_LIKELY( icon_name ) )
            {
                if ( G_UNLIKELY( icon_name[ 0 ] == '/' ) )
                    fi->big_thumbnail = gdk_pixbuf_new_from_file( icon_name, NULL );
                else
                    fi->big_thumbnail = gtk_icon_theme_load_icon(
                                            gtk_icon_theme_get_default(),
                                            icon_name,
                                            icon_size, 0, NULL );
            }
            if ( fi->big_thumbnail )
                g_object_set_data_full( G_OBJECT(fi->big_thumbnail), "name", icon_name, g_free );
            else
                g_free( icon_name );
        }
        return fi->big_thumbnail ? gdk_pixbuf_ref( fi->big_thumbnail ) : NULL;
    }
    if( G_UNLIKELY(!fi->mime_type) )
        return NULL;
    return vfs_mime_type_get_icon( fi->mime_type, TRUE );
}

GdkPixbuf* vfs_file_info_get_small_icon( VFSFileInfo* fi )
{
    return vfs_mime_type_get_icon( fi->mime_type, FALSE );
}

GdkPixbuf* vfs_file_info_get_big_thumbnail( VFSFileInfo* fi )
{
    return fi->big_thumbnail ? gdk_pixbuf_ref( fi->big_thumbnail ) : NULL;
}

GdkPixbuf* vfs_file_info_get_small_thumbnail( VFSFileInfo* fi )
{
    return fi->small_thumbnail ? gdk_pixbuf_ref( fi->small_thumbnail ) : NULL;
}

const char* vfs_file_info_get_disp_owner( VFSFileInfo* fi )
{
    struct passwd * puser;
    struct group* pgroup;
    char uid_str_buf[ 32 ];
    char* user_name;
    char gid_str_buf[ 32 ];
    char* group_name;

    /* FIXME: user names should be cached */
    if ( ! fi->disp_owner )
    {
        puser = getpwuid( fi->uid );
        if ( puser && puser->pw_name && *puser->pw_name )
            user_name = puser->pw_name;
        else
        {
            sprintf( uid_str_buf, "%d", fi->uid );
            user_name = uid_str_buf;
        }

        pgroup = getgrgid( fi->gid );
        if ( pgroup && pgroup->gr_name && *pgroup->gr_name )
            group_name = pgroup->gr_name;
        else
        {
            sprintf( gid_str_buf, "%d", fi->gid );
            group_name = gid_str_buf;
        }
        fi->disp_owner = g_strdup_printf ( "%s:%s", user_name, group_name );
    }
    return fi->disp_owner;
}

const char* vfs_file_info_get_disp_mtime( VFSFileInfo* fi )
{
    if ( ! fi->disp_mtime )
    {
        char buf[ 64 ];
        strftime( buf, sizeof( buf ),
                  "%Y-%m-%d %H:%M",
                  localtime( &fi->mtime ) );
        fi->disp_mtime = g_strdup( buf );
    }
    return fi->disp_mtime;
}

time_t* vfs_file_info_get_mtime( VFSFileInfo* fi )
{
    return & fi->mtime;
}

time_t* vfs_file_info_get_atime( VFSFileInfo* fi )
{
    return & fi->atime;
}

static void get_file_perm_string( char* perm, mode_t mode )
{
    perm[ 0 ] = S_ISDIR( mode ) ? 'd' : ( S_ISLNK( mode ) ? 'l' : '-' );
    perm[ 1 ] = ( mode & S_IRUSR ) ? 'r' : '-';
    perm[ 2 ] = ( mode & S_IWUSR ) ? 'w' : '-';
    perm[ 3 ] = ( mode & S_IXUSR ) ? 'x' : '-';
    perm[ 4 ] = ( mode & S_IRGRP ) ? 'r' : '-';
    perm[ 5 ] = ( mode & S_IWGRP ) ? 'w' : '-';
    perm[ 6 ] = ( mode & S_IXGRP ) ? 'x' : '-';
    perm[ 7 ] = ( mode & S_IROTH ) ? 'r' : '-';
    perm[ 8 ] = ( mode & S_IWOTH ) ? 'w' : '-';
    perm[ 9 ] = ( mode & S_IXOTH ) ? 'x' : '-';
    perm[ 10 ] = '\0';
}

const char* vfs_file_info_get_disp_perm( VFSFileInfo* fi )
{
    if ( ! fi->disp_perm[ 0 ] )
        get_file_perm_string( fi->disp_perm,
                              fi->mode );
    return fi->disp_perm;
}

void file_size_to_string( char* buf, guint64 size )
{
    char * unit;
    /* guint point; */
    gfloat val;

    /*
       FIXME: Is floating point calculation slower than integer division?
              Some profiling is needed here.
    */
    if ( size > ( ( guint64 ) 1 ) << 30 )
    {
        if ( size > ( ( guint64 ) 1 ) << 40 )
        {
            /*
            size /= ( ( ( guint64 ) 1 << 40 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
            val = ((gfloat)size) / ( ( guint64 ) 1 << 40 );
            unit = "TB";
        }
        else
        {
            /*
            size /= ( ( 1 << 30 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            */
            val = ((gfloat)size) / ( ( guint64 ) 1 << 30 );
            unit = "GB";
        }
    }
    else if ( size > ( 1 << 20 ) )
    {
        /*
        size /= ( ( 1 << 20 ) / 10 );
        point = ( guint ) ( size % 10 );
        size /= 10;
        */
        val = ((gfloat)size) / ( ( guint64 ) 1 << 20 );
        unit = "MB";
    }
    else if ( size > ( 1 << 10 ) )
    {
        /*
        size /= ( ( 1 << 10 ) / 10 );
        point = size % 10;
        size /= 10;
        */
        val = ((gfloat)size) / ( ( guint64 ) 1 << 10 );
        unit = "KB";
    }
    else
    {
        unit = size > 1 ? "Bytes" : "Byte";
        sprintf( buf, "%u %s", ( guint ) size, unit );
        return ;
    }
    /* sprintf( buf, "%llu.%u %s", size, point, unit ); */
    sprintf( buf, "%.1f %s", val, unit );
}

gboolean vfs_file_info_is_dir( VFSFileInfo* fi )
{
    if ( S_ISDIR( fi->mode ) )
        return TRUE;
    if ( S_ISLNK( fi->mode ) &&
            0 == strcmp( vfs_mime_type_get_type( fi->mime_type ), XDG_MIME_TYPE_DIRECTORY ) )
    {
        return TRUE;
    }
    return FALSE;
}

gboolean vfs_file_info_is_symlink( VFSFileInfo* fi )
{
    return S_ISLNK( fi->mode ) ? TRUE : FALSE;
}

gboolean vfs_file_info_is_image( VFSFileInfo* fi )
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if ( ! strncmp( "image/", vfs_mime_type_get_type( fi->mime_type ), 6 ) )
        return TRUE;
    return FALSE;
}

gboolean vfs_file_info_is_unknown_type( VFSFileInfo* fi )
{
    if ( ! strcmp( XDG_MIME_TYPE_UNKNOWN,
           vfs_mime_type_get_type( fi->mime_type ) ) )
        return TRUE;
    return FALSE;
}

/* full path of the file is required by this function */
gboolean vfs_file_info_is_executable( VFSFileInfo* fi, const char* file_path )
{
    return xdg_mime_is_executable_file( file_path, fi->mime_type->type );
}

/* full path of the file is required by this function */
gboolean vfs_file_info_is_text( VFSFileInfo* fi, const char* file_path )
{
    return xdg_mime_is_text_file( file_path, fi->mime_type->type );
}

/*
* Run default action of specified file.
* Full path of the file is required by this function.
*/
gboolean vfs_file_info_open_file( VFSFileInfo* fi,
                                  const char* file_path,
                                  GError** err )
{
    VFSMimeType * mime_type;
    char* app_name;
    VFSAppDesktop* app;
    GList* files = NULL;
    gboolean ret = FALSE;
    char* argv[ 2 ];

    if ( vfs_file_info_is_executable( fi, file_path ) )
    {
        argv[ 0 ] = file_path;
        argv[ 1 ] = '\0';
        ret = g_spawn_async( NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL|
                             G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, err );
    }
    else
    {
        mime_type = vfs_file_info_get_mime_type( fi );
        app_name = vfs_mime_type_get_default_action( mime_type );
        if ( app_name )
        {
            app = vfs_app_desktop_new( app_name );
            if ( ! vfs_app_desktop_get_exec( app ) )
                app->exec = g_strdup( app_name );   /* FIXME: app->exec */
            files = g_list_prepend( files, file_path );
            /* FIXME: working dir is needed */
            ret = vfs_app_desktop_open_files( gdk_screen_get_default(),
                                              NULL, app, files, err );
            g_list_free( files );
            vfs_app_desktop_unref( app );
            g_free( app_name );
        }
        vfs_mime_type_unref( mime_type );
    }
    return ret;
}

mode_t vfs_file_info_get_mode( VFSFileInfo* fi )
{
    return fi->mode;
}

static void unload_thumnails_if_needed( VFSFileInfo* fi )
{
    int w, h;
    if ( fi->big_thumbnail && fi->flags == VFS_FILE_INFO_NONE )
    {
        w = gdk_pixbuf_get_width( fi->big_thumbnail );
        h = gdk_pixbuf_get_height( fi->big_thumbnail );
        if ( _MAX( w, h ) != big_thumb_size )
        {
            gdk_pixbuf_unref( fi->big_thumbnail );
            fi->big_thumbnail = NULL;
        }
    }
    if ( fi->small_thumbnail && fi->flags == VFS_FILE_INFO_NONE )
    {
        w = gdk_pixbuf_get_width( fi->small_thumbnail );
        h = gdk_pixbuf_get_height( fi->small_thumbnail );
        if ( MAX( w, h ) != small_thumb_size )
        {
            gdk_pixbuf_unref( fi->small_thumbnail );
            fi->small_thumbnail = NULL;
        }
    }
}

gboolean vfs_file_info_is_thumbnail_loaded( VFSFileInfo* fi, gboolean big )
{
    /* FIXME: I cannot find a better place to unload thumbnails */
    unload_thumnails_if_needed( fi );
    if ( big )
        return ( fi->big_thumbnail != NULL );
    return ( fi->small_thumbnail != NULL );
}

gboolean vfs_file_info_load_thumbnail( VFSFileInfo* fi,
                                       const char* full_path,
                                       gboolean big )
{
    char * uri;
    md5_byte_t md5[ 16 ];
    char* thumbnail_file;
    char file_name[ 36 ];
    char mtime_str[ 32 ];
    const char* thumb_mtime;
    int i, w, h, size;
    struct stat statbuf;
    GdkPixbuf* thumbnail, *result = NULL;

    /* FIXME: I cannot find a better place to unload thumbnails */
    unload_thumnails_if_needed( fi );

    if ( big )
    {
        if ( fi->big_thumbnail && big )
            return TRUE;
    }
    else
    {
        if ( fi->small_thumbnail )
            return TRUE;
    }

    if ( !gdk_pixbuf_get_file_info( full_path, &w, &h ) )
        return FALSE;   /* image format cannot be recognized */

    /* If the image itself is very small, we should load it directly */
    if ( w <= 128 && h <= 128 )
    {
        size = big ? big_thumb_size : small_thumb_size;
        thumbnail = gdk_pixbuf_new_from_file_at_scale( full_path, size, size,
                                                       TRUE, NULL );
        if ( big )
            fi->big_thumbnail = thumbnail;
        else
            fi->small_thumbnail = thumbnail;
        return ( thumbnail != NULL );
    }

    uri = g_filename_to_uri( full_path, NULL, NULL );

    md5_state_t md5_state;
    md5_init( &md5_state );
    md5_append( &md5_state, ( md5_byte_t * ) uri, strlen( uri ) );
    md5_finish( &md5_state, md5 );

    for ( i = 0; i < 16; ++i )
    {
        sprintf( ( file_name + i * 2 ), "%02x", md5[ i ] );
    }
    strcpy( ( file_name + i * 2 ), ".png" );

    thumbnail_file = g_build_filename( g_get_home_dir(),
                                       ".thumbnails/normal",
                                       file_name, NULL );

    /* load existing thumbnail */
    thumbnail = gdk_pixbuf_new_from_file( thumbnail_file, NULL );
    if ( !thumbnail ||
            !( thumb_mtime = gdk_pixbuf_get_option( thumbnail, "tEXt::Thumb::MTime" ) ) ||
            atol( thumb_mtime ) != fi->mtime )
    {
        /* create new thumbnail */
        thumbnail = gdk_pixbuf_new_from_file_at_size( full_path, 128, 128, NULL );
        if ( thumbnail )
        {
            sprintf( mtime_str, "%lu", fi->mtime );
            gdk_pixbuf_save( thumbnail, thumbnail_file, "png", NULL,
                             "tEXt::Thumb::URI", uri, "tEXt::Thumb::MTime", mtime_str, NULL );
        }
    }

    if ( thumbnail )
    {
        w = gdk_pixbuf_get_width( thumbnail );
        h = gdk_pixbuf_get_height( thumbnail );
        size = big ? big_thumb_size : small_thumb_size;
        if ( w > h )
        {
            h = h * size / w;
            w = size;
        }
        else if ( h > w )
        {
            w = w * size / h;
            h = size;
        }
        else
        {
            w = h = size;
        }
        result = gdk_pixbuf_scale_simple(
                     thumbnail,
                     w, h, GDK_INTERP_BILINEAR );
        gdk_pixbuf_unref( thumbnail );
        if ( big )
            fi->big_thumbnail = result;
        else
            fi->small_thumbnail = result;
    }
    g_free( uri );
    g_free( thumbnail_file );
    return ( result != NULL );
}

void vfs_file_info_set_thumbnail_size( int big, int small )
{
    big_thumb_size = big;
    small_thumb_size = small;
}

void vfs_file_info_load_special_info( VFSFileInfo* fi,
                                      const char* file_path )
{
    if ( g_str_has_suffix( fi->name, ".desktop" ) )
    {
        VFSAppDesktop * desktop;
        const char* icon_name;
        fi->flags |= VFS_FILE_INFO_DESKTOP_ENTRY;
        desktop = vfs_app_desktop_new( file_path );
        if ( vfs_app_desktop_get_disp_name( desktop ) )
        {
            vfs_file_info_set_disp_name(
                fi, vfs_app_desktop_get_disp_name( desktop ) );
        }
        if( fi->big_thumbnail )
        {
            gdk_pixbuf_unref( fi->big_thumbnail );
            fi->big_thumbnail = NULL;
        }
        if ( (icon_name = vfs_app_desktop_get_icon_name( desktop )) )
        {
            char * _icon_name = strdup( icon_name );
            GdkPixbuf* icon;
            int size;
            vfs_mime_type_get_icon_size( &size, NULL );
            /* icon name is a full path */
            if ( icon_name[ 0 ] == '/' )
            {
                icon = gdk_pixbuf_new_from_file_at_size(
                           icon_name, size, size, NULL );
            }
            else
            {
                char* dot = strchr( _icon_name, '.' );
                if ( dot )
                    * dot = '\0';
                icon = gtk_icon_theme_load_icon(
                           gtk_icon_theme_get_default(),
                           _icon_name, size, 0, NULL );
            }
            /* save app icon in thumbnail */
            fi->big_thumbnail = icon;
            if ( G_LIKELY( icon ) )
                g_object_set_data_full( G_OBJECT(icon), "name", _icon_name, g_free );
            else
                g_free( _icon_name );
        }
        vfs_app_desktop_unref( desktop );
    }
}

void vfs_file_info_list_free( GList* list )
{
    g_list_foreach( list, (GFunc)vfs_file_info_unref, NULL );
    g_list_free( list );
}


char* vfs_file_resolve_path( const char* cwd, const char* relative_path )
{
    GString* ret = g_string_sized_new( 4096 );
    const char* sep;
    if( G_UNLIKELY(*relative_path != '/') ) /* relative path */
    {
        if( G_UNLIKELY(relative_path[0] == '~') ) /* home dir */
        {
            g_string_append( ret, g_get_home_dir());
            ++relative_path;
        }
        else
        {
            if( ! cwd )
            {
                cwd = g_get_current_dir();
                g_string_append( ret, cwd );
                g_free( cwd );
            }
            else
                g_string_append( ret, cwd );
        }
    }

    if( relative_path[0] != '/' )
        g_string_append_c( ret, '/' );

    while( G_LIKELY( *relative_path ) )
    {
        if( G_UNLIKELY(*relative_path == '.') )
        {
            if( relative_path[1] == '/' || relative_path[1] == '\0' ) /* current dir */
            {
                relative_path += relative_path[1] ? 2 : 1;
                continue;
            }
            if( relative_path[1] == '.' &&
                ( relative_path[2] == '/' || relative_path[2] == '\0') ) /* parent dir */
            {
                gsize len = ret->len - 2;
                while( ret->str[ len ] != '/' )
                    --len;
                g_string_truncate( ret, len + 1 );
                relative_path += relative_path[2] ? 3 : 2;
                continue;
            }
        }

        do
        {
            g_string_append_c( ret, *relative_path );
        }while( G_LIKELY( *(relative_path++) != '/' && *relative_path ) );
    }

    /* remove tailing '/' */
    if( G_LIKELY( ret->len > 1 ) && G_UNLIKELY( ret->str[ ret->len - 1 ] == '/' ) )
        g_string_truncate( ret, ret->len - 1 );
    return g_string_free( ret, FALSE );
}

