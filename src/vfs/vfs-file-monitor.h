/*
*  C Interface: vfs-monitor
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

/*
  FIXME: VFSFileMonitor can support at most 1024 monitored files.
         This is caused by the limit of FAM/gamin itself.
         Maybe using inotify directly can solve this?
*/

#ifndef _VFS_FILE_MONITOR_H_
#define _VFS_FILE_MONITOR_H_

#include <glib.h>
#include <fam.h>

G_BEGIN_DECLS

typedef enum{
  VFS_FILE_MONITOR_CREATE = FAMCreated,
  VFS_FILE_MONITOR_DELETE = FAMDeleted,
  VFS_FILE_MONITOR_CHANGE = FAMChanged
}VFSFileMonitorEvent;

typedef struct _VFSFileMonitor VFSFileMonitor;

struct _VFSFileMonitor{
  gchar* path;
  /*<private>*/
  int n_ref;
  FAMRequest request;
  GArray* callbacks;
};

/* Callback function which will be called when monitored events happen */
typedef void (*VFSFileMonitorCallback)( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        gpointer user_data );

VFSFileMonitor* vfs_file_monitor_add( const char* path,
                                      VFSFileMonitorCallback cb,
                                      gpointer user_data );

void vfs_file_monitor_remove( VFSFileMonitor* fm,
                              VFSFileMonitorCallback cb,
                              gpointer user_data );

G_END_DECLS

#endif
