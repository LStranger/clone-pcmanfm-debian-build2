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

#include "vfs-app-desktop.h"

#include "md5.h"    /* for thumbnails */

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
    if( fi->big_thumbnail )
    {
        gdk_pixbuf_unref(fi->big_thumbnail);
        fi->big_thumbnail = NULL;
    }
    if( fi->small_thumbnail )
    {
        gdk_pixbuf_unref(fi->small_thumbnail);
        fi->small_thumbnail = NULL;
    }

    fi->disp_perm[ 0 ] = '\0';

    if ( fi->mime_type )
    {
        vfs_mime_type_unref( fi->mime_type );
        fi->mime_type = NULL;
    }
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
    vfs_file_info_clear( fi );

    if ( base_name )
        fi->name = g_strdup( base_name );
    else
        fi->name = g_path_get_basename( file_path );

    if ( lstat( file_path, &fi->file_stat ) == 0 )
    {
        if( G_LIKELY( utf8_file_name && g_utf8_validate ( fi->name, -1, NULL) ) )
        {
            fi->disp_name = fi->name;   /* Don't duplicate the name and save memory */
        }
        else
        {
            fi->disp_name = g_filename_display_name( fi->name );
        }
        fi->mime_type = vfs_mime_type_get_from_file( file_path,
                                                     fi->disp_name,
                                                     &fi->file_stat );
        return TRUE;
    }
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
    if( fi->disp_name && fi->disp_name != fi->name )
        g_free( fi->disp_name );
    fi->disp_name = name ? g_strdup(name) : NULL ;
}

void vfs_file_info_set_name( VFSFileInfo* fi, const char* name )
{
    if( fi->name )
        g_free( fi->name );
    fi->name = name ? g_strdup(name) : NULL ;
}

off_t vfs_file_info_get_size( VFSFileInfo* fi )
{
    return fi->file_stat.st_size;
}

const char* vfs_file_info_get_disp_size( VFSFileInfo* fi )
{
    if ( G_UNLIKELY( !fi->disp_size ) )
    {
        char buf[ 64 ];
        file_size_to_string( buf, fi->file_stat.st_size );
        fi->disp_size = g_strdup( buf );
    }
    return fi->disp_size;
}

off_t vfs_file_info_get_blocks( VFSFileInfo* fi )
{
    return fi->file_stat.st_blocks;
}

VFSMimeType* vfs_file_info_get_mime_type( VFSFileInfo* fi )
{
    vfs_mime_type_ref( fi->mime_type );
    return fi->mime_type;
}

void vfs_file_info_reload_mime_type( VFSFileInfo* fi,
                                     const char* full_path )
{
    VFSMimeType* old_mime_type;

    old_mime_type = fi->mime_type;
    fi->mime_type = vfs_mime_type_get_from_file( full_path, 
                                                 fi->name, &fi->file_stat );
    vfs_mime_type_unref( old_mime_type );  /* FIXME: is vfs_mime_type_unref needed ?*/
}

const char* vfs_file_info_get_mime_type_desc( VFSFileInfo* fi )
{
    return vfs_mime_type_get_description( fi->mime_type );
}

GdkPixbuf* vfs_file_info_get_big_icon( VFSFileInfo* fi )
{
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
        puser = getpwuid( fi->file_stat.st_uid );
        if ( puser && puser->pw_name && *puser->pw_name )
            user_name = puser->pw_name;
        else
        {
            sprintf( uid_str_buf, "%d", fi->file_stat.st_uid );
            user_name = uid_str_buf;
        }

        pgroup = getgrgid( fi->file_stat.st_gid );
        if ( pgroup && pgroup->gr_name && *pgroup->gr_name )
            group_name = pgroup->gr_name;
        else
        {
            sprintf( gid_str_buf, "%d", fi->file_stat.st_gid );
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
                  localtime( &fi->file_stat.st_mtime ) );
        fi->disp_mtime = g_strdup( buf );
    }
    return fi->disp_mtime;
}

time_t* vfs_file_info_get_mtime( VFSFileInfo* fi )
{
    return & fi->file_stat.st_mtime;
}

time_t* vfs_file_info_get_atime( VFSFileInfo* fi )
{
    return & fi->file_stat.st_atime;
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
                              fi->file_stat.st_mode );
    return fi->disp_perm;
}

void file_size_to_string( char* buf, guint64 size )
{
    char * unit;
    guint point;

    if ( size > ( ( guint64 ) 1 ) << 30 )
    {
        if ( size > ( ( guint64 ) 1 ) << 40 )
        {
            size /= ( ( ( guint64 ) 1 << 40 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            unit = "TB";
        }
        else
        {
            size /= ( ( 1 << 30 ) / 10 );
            point = ( guint ) ( size % 10 );
            size /= 10;
            unit = "GB";
        }
    }
    else if ( size > ( 1 << 20 ) )
    {
        size /= ( ( 1 << 20 ) / 10 );
        point = ( guint ) ( size % 10 );
        size /= 10;
        unit = "MB";
    }
    else if ( size > ( 1 << 10 ) )
    {
        size /= ( ( 1 << 10 ) / 10 );
        point = size % 10;
        size /= 10;
        unit = "KB";
    }
    else
    {
        unit = size > 1 ? "Bytes" : "Byte";
        sprintf( buf, "%u %s", ( guint ) size, unit );
        return ;
    }
    sprintf( buf, "%llu.%u %s", size, point, unit );
}

gboolean vfs_file_info_is_dir( VFSFileInfo* fi )
{
    if ( S_ISDIR( fi->file_stat.st_mode ) )
        return TRUE;
    if ( S_ISLNK( fi->file_stat.st_mode ) &&
            0 == strcmp( vfs_mime_type_get_type( fi->mime_type ), XDG_MIME_TYPE_DIRECTORY ) )
    {
        return TRUE;
    }
    return FALSE;
}

gboolean vfs_file_info_is_symlink( VFSFileInfo* fi )
{
    return S_ISLNK( fi->file_stat.st_mode ) ? TRUE : FALSE;
}

gboolean vfs_file_info_is_image( VFSFileInfo* fi )
{
    /* FIXME: We had better use functions of xdg_mime to check this */
    if ( ! strncmp( "image/", vfs_mime_type_get_type( fi->mime_type ), 6 ) )
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
        ret = g_spawn_async( NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL,
                             G_SPAWN_SEARCH_PATH, NULL, NULL, err );
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
            ret = vfs_app_desktop_open_files( app, files, err );
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
    return fi->file_stat.st_mode;
}

#define _MAX( x, y )     (x > y ? x : y)

static void unload_thumnails_if_needed( VFSFileInfo* fi )
{
    int w, h;
    if( fi->big_thumbnail )
    {
        w = gdk_pixbuf_get_width( fi->big_thumbnail );
        h = gdk_pixbuf_get_height( fi->big_thumbnail );
        if( _MAX(w, h) != big_thumb_size )
        {
            gdk_pixbuf_unref( fi->big_thumbnail );
            fi->big_thumbnail = NULL;
        }
    }
    if( fi->small_thumbnail )
    {
        w = gdk_pixbuf_get_width( fi->small_thumbnail );
        h = gdk_pixbuf_get_height( fi->small_thumbnail );
        if( MAX(w, h) != small_thumb_size )
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
        return (fi->big_thumbnail != NULL);
    return (fi->small_thumbnail != NULL);
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

    if( big )
    {
        if( fi->big_thumbnail && big )
            return TRUE;
    }
    else
    {
        if( fi->small_thumbnail )
            return TRUE;
    }

    if( !gdk_pixbuf_get_file_info( full_path, &w, &h ) )
        return FALSE;   /* image format cannot be recognized */

    /* If the image itself is very small, we should load it directly */
    if( w <= 128 && h <= 128 )
    {
        size = big ? big_thumb_size : small_thumb_size;
        thumbnail = gdk_pixbuf_new_from_file_at_scale( full_path, size, size,
                                                       TRUE, NULL );
        if ( big )
            fi->big_thumbnail = thumbnail;
        else
            fi->small_thumbnail = thumbnail;
        return (thumbnail != NULL);
    }

    uri = g_filename_to_uri( full_path, NULL, NULL );

    md5_state_t md5_state;
    md5_init( &md5_state );
    md5_append( &md5_state, uri, strlen( uri ) );
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
            atol( thumb_mtime ) != fi->file_stat.st_mtime )
    {
        /* create new thumbnail */
        thumbnail = gdk_pixbuf_new_from_file_at_size( full_path, 128, 128, NULL );
        if ( thumbnail )
        {
            sprintf( mtime_str, "%lu", fi->file_stat.st_mtime );
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
