/*
*  C Interface: vfs-file-info
*
* Description: File information
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _VFS_FILE_INFO_H_
#define _VFS_FILE_INFO_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vfs-mime-type.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _VFSFileInfo VFSFileInfo;

struct _VFSFileInfo
{
    struct stat file_stat;
    char* name; /* real name on file system */
    char* disp_name;  /* displayed name (in UTF-8) */
    char* disp_size;  /* displayed human-readable file size */
    char* disp_owner; /* displayed owner:group pair */
    char* disp_mtime; /* displayed last modification time */
    char disp_perm[ 12 ];  /* displayed permission in string form */
    VFSMimeType* mime_type; /* mime type related information */
    GdkPixbuf* big_thumbnail; /* thumbnail of the file */
    GdkPixbuf* small_thumbnail; /* thumbnail of the file */

    /*<private>*/
    int n_ref;
};

void vfs_file_info_set_utf8_filename( gboolean is_utf8 );

VFSFileInfo* vfs_file_info_new ();
void vfs_file_info_ref( VFSFileInfo* fi );
void vfs_file_info_unref( VFSFileInfo* fi );

gboolean vfs_file_info_get( VFSFileInfo* fi,
                            const char* file_path,
                            const char* base_name );

const char* vfs_file_info_get_name( VFSFileInfo* fi );
const char* vfs_file_info_get_disp_name( VFSFileInfo* fi );

void vfs_file_info_set_name( VFSFileInfo* fi, const char* name );
void vfs_file_info_set_disp_name( VFSFileInfo* fi, const char* name );

off_t vfs_file_info_get_size( VFSFileInfo* fi );
const char* vfs_file_info_get_disp_size( VFSFileInfo* fi );

off_t vfs_file_info_get_blocks( VFSFileInfo* fi );

mode_t vfs_file_info_get_mode( VFSFileInfo* fi );

VFSMimeType* vfs_file_info_get_mime_type( VFSFileInfo* fi );
void vfs_file_info_reload_mime_type( VFSFileInfo* fi,
                                     const char* full_path );

const char* vfs_file_info_get_mime_type_desc( VFSFileInfo* fi );

const char* vfs_file_info_get_disp_owner( VFSFileInfo* fi );
const char* vfs_file_info_get_disp_mtime( VFSFileInfo* fi );
const char* vfs_file_info_get_disp_perm( VFSFileInfo* fi );

time_t* vfs_file_info_get_mtime( VFSFileInfo* fi );
time_t* vfs_file_info_get_atime( VFSFileInfo* fi );

void vfs_file_info_set_thumbnail_size( int big, int small );
gboolean vfs_file_info_load_thumbnail( VFSFileInfo* fi,
                                       const char* full_path,
                                       gboolean big );

GdkPixbuf* vfs_file_info_get_big_icon( VFSFileInfo* fi );
GdkPixbuf* vfs_file_info_get_small_icon( VFSFileInfo* fi );

GdkPixbuf* vfs_file_info_get_big_thumbnail( VFSFileInfo* fi );
GdkPixbuf* vfs_file_info_get_small_thumbnail( VFSFileInfo* fi );

void file_size_to_string( char* buf, guint64 size );

gboolean vfs_file_info_is_dir( VFSFileInfo* fi );

gboolean vfs_file_info_is_symlink( VFSFileInfo* fi );

gboolean vfs_file_info_is_image( VFSFileInfo* fi );

/* Full path of the file is required by this function */
gboolean vfs_file_info_is_executable( VFSFileInfo* fi, const char* file_path );

/* Full path of the file is required by this function */
gboolean vfs_file_info_is_text( VFSFileInfo* fi, const char* file_path );

/*
* Run default action of specified file.
* Full path of the file is required by this function.
*/
gboolean vfs_file_info_open_file( VFSFileInfo* fi,
                                  const char* file_path,
                                  GError** err );

G_END_DECLS

#endif
