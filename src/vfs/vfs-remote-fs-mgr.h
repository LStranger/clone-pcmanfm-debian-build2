/*
 *      vfs-remote-fs-mgr.h
 *      
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef __VFS_REMOTE_FS_MGR_H__
#define __VFS_REMOTE_FS_MGR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define VFS_REMOTE_FS_MGR_TYPE				(vfs_remote_fs_mgr_get_type())
#define VFS_REMOTE_FS_MGR(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
		VFS_REMOTE_FS_MGR_TYPE, VFSRemoteFSMgr))
#define VFS_REMOTE_FS_MGR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
		VFS_REMOTE_FS_MGR_TYPE, VFSRemoteFSMgrClass))
#define IS_VFS_REMOTE_FS_MGR(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
		VFS_REMOTE_FS_MGR_TYPE))
#define IS_VFS_REMOTE_FS_MGR_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
		VFS_REMOTE_FS_MGR_TYPE))

typedef struct _VFSRemoteFSMgr				VFSRemoteFSMgr;
typedef struct _VFSRemoteFSMgrClass			VFSRemoteFSMgrClass;

typedef struct _VFSRemoteVolume             VFSRemoteVolume;

struct _VFSRemoteVolume
{
    char* name;
    char* host;
    char* proxy_host;
    gushort port;
    gushort proxy_port;
    /* gboolean is_mounted : 1; */
    char* user;
    char* passwd;
    char* type;
    char* encoding;
    char* init_dir;
    char* mount_point;

    char* proxy_type;
    char* proxy_user;
    char* proxy_passwd;

    /* <private> */
    int n_ref;
};

struct _VFSRemoteFSMgr
{
	GObject parent;
	/* <private> */
	GList* vols;
};

struct _VFSRemoteFSMgrClass
{
	GObjectClass parent_class;

	void (*add) ( VFSRemoteFSMgr* dir, VFSRemoteVolume* vol );
	void (*remove) ( VFSRemoteFSMgr* dir, VFSRemoteVolume* vol );
	void (*change) ( VFSRemoteFSMgr* dir, VFSRemoteVolume* vol );
	void (*mount) ( VFSRemoteFSMgr* dir, VFSRemoteVolume* vol );
	void (*unmount) ( VFSRemoteFSMgr* dir, VFSRemoteVolume* vol );
};

VFSRemoteVolume* vfs_remote_volume_new();
VFSRemoteVolume* vfs_remote_volume_new_from_url(const char* url);
VFSRemoteVolume* vfs_remote_volume_ref( VFSRemoteVolume* vol );
void vfs_remote_volume_unref( VFSRemoteVolume* vol );

GType		vfs_remote_fs_mgr_get_type	(void);

/* Get the global remote file system manager instance.
 * Manual creation of this object is prohibited
 * Changes of remote volumes can be monitored via connecting to
 * related signals of the manager instance.
 */
VFSRemoteFSMgr* vfs_remote_fs_mgr_get();

/* Get a list of all known remote volumes
 * The list is owned by VFSRemoteFSMgr, and shouldn't be freed
 * Don't store the pointer for future use because it will be changed. */
GList* vfs_remote_fs_mgr_get_volumes( VFSRemoteFSMgr* mgr );

/* Add a new remote volume */
void vfs_remote_fs_mgr_add_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol );

/* Remove an existing remote volume */
void vfs_remote_fs_mgr_remove_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol );

/* Notify that you've changed the content of the remote volume object */
void vfs_remote_fs_mgr_change_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol );

/* Mount the remote volume */
gboolean vfs_remote_volume_mount( VFSRemoteVolume* vol, GError** err );

/* Unmount the remote volume */
gboolean vfs_remote_volume_unmount( VFSRemoteVolume* vol, GError** err );

/* Check if the remote volume is mounted */
gboolean vfs_remote_volume_is_mounted( VFSRemoteVolume* vol );

/* Get the desired mount point of the volume (the path may not exist) */
const char* vfs_remote_volume_get_mount_point( VFSRemoteVolume* vol );

G_END_DECLS

#endif /* __VFS_REMOTE_FS_MGR_H__ */
