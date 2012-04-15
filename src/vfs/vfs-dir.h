/*
*  C Interface: vfs-dir
*
* Description: Object used to present a directory
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _VFS_DIR_H_
#define _VFS_DIR_H_

#include <glib.h>
#include <glib-object.h>

#include "vfs-file-monitor.h"
#include "vfs-file-info.h"

G_BEGIN_DECLS

#define VFS_TYPE_DIR             (vfs_dir_get_type())
#define VFS_DIR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  VFS_TYPE_DIR, VFSDir))
#define VFS_DIR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  VFS_TYPE_DIR, VFSDirClass))
#define VFS_IS_DIR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VFS_TYPE_DIR))
#define VFS_IS_DIR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  VFS_TYPE_DIR))
#define VFS_DIR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  VFS_TYPE_DIR, VFSDirClass))

#define vfs_dir_ref( dir )      g_object_ref(dir)
#define vfs_dir_unref( dir )    g_object_unref(dir)

typedef struct _VFSDir VFSDir;
typedef struct _VFSDirClass VFSDirClass;

struct _VFSDir
{
    GObject parent;

    char* path;
    char* disp_path;
    GList* file_list;
    int n_files;

    /*<private>*/
    VFSFileMonitor* monitor;
    GMutex* mutex;  /* Used to guard file_list */
    GThread* thread;
    GSList* state_callback_list;
    gboolean file_listed : 1;
    gboolean load_complete : 1;
    gboolean cancel: 1;
    gboolean show_hidden : 1;
    /* gboolean dir_only : 1; FIXME: can this be implemented? */
    guint load_notify;

    GQueue* thumbnail_requests;
    GMutex* thumbnail_mutex;  /* Used to guard thumbnail_requests */
    GCond* thumbnail_cond;
    GThread* thumbnail_thread;
    guint thumbnail_idle;
    GList* loaded_thumbnails;
};

struct _VFSDirClass
{
    GObjectClass parent;
    /* Default signal handlers */
    void ( *file_created ) ( VFSDir* dir, const char* file_name );
    void ( *file_deleted ) ( VFSDir* dir, const char* file_name );
    void ( *file_changed ) ( VFSDir* dir, const char* file_name );
    void ( *file_listed ) ( VFSDir* dir );
    void ( *load_complete ) ( VFSDir* dir );
    /*  void (*need_reload) ( VFSDir* dir ); */
    /*  void (*update_mime) ( VFSDir* dir ); */
};

typedef void ( *VFSDirStateCallback ) ( VFSDir* dir, int state, gpointer user_data );

GType vfs_dir_get_type ( void );
VFSDir* vfs_dir_new();

VFSDir* vfs_get_dir( const char* path );

void vfs_dir_add_state_callback( VFSDir* dir,
                                 VFSDirStateCallback func,
                                 gpointer user_data );

void vfs_dir_remove_state_callback( VFSDir* dir,
                                    VFSDirStateCallback func,
                                    gpointer user_data );

gboolean vfs_dir_is_loading( VFSDir* dir );
void vfs_dir_cancel_load( VFSDir* dir );

void vfs_dir_request_thumbnail( VFSDir* dir, VFSFileInfo* file, gboolean big );
void vfs_dir_cancel_thumbnail_request( VFSDir* dir, VFSFileInfo* file,
                                       gboolean big );

G_END_DECLS

#endif
