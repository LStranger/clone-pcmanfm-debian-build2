/*
*  C Interface: fileoperation
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/
#ifndef  _FILE_OPERATION_H
#define  _FILE_OPERATION_H

#include <gtk/gtk.h>
#include <glib.h>
#include <sys/types.h>

typedef enum
{
  FO_MOVE = 0,
  FO_COPY,
  FO_DELETE,
  FO_LINK, /* will be supported in the future */
  FO_CHMOD_CHOWN, /* These two kinds of operation have lots in common,
                   * so put them together to reduce duplicated disk I/O */
  FO_LAST
}FileOperationType;

typedef enum {
  OWNER_R = 0,
  OWNER_W,
  OWNER_X,
  GROUP_R,
  GROUP_W,
  GROUP_X,
  OTHER_R,
  OTHER_W,
  OTHER_X,
  SET_UID,
  SET_GID,
  STICKY,
  N_CHMOD_ACTIONS
}ChmodActionType;

extern const mode_t chmod_flags[];

struct _FileOperation;

typedef void (*FileOperationCallback)( struct _FileOperation* );


typedef enum
{
  FOS_RUNNING,
  FOS_QUERYCANCEL,
  FOS_QUERYOVERWRITE,
  FOS_CANCELLED,
}FileOperationState;

typedef struct _FileOperation{
  FileOperationType action;
  GList* source_paths; /* All source files. This list will be freed
                          after file operation is completed. */
  char* src_path; /* First source file */
  char* dest_path; /* Destinaton directory */

  gboolean overwrite_all; /* Overwrite all existing files without prompt */
  gboolean recursive; /* Apply operation to all files under folders
                      * recursively. This is default to copy/delete,
                      * and should be set manually for chown/chmod. */
  gboolean skip_all; /* Don't try to overwrite any files */

  /* For chown */
  uid_t uid;
  gid_t gid;

  /* For chmod */
  guchar *chmod_actions;  /* If chmod is not needed, this should be NULL */

  char* current_file; /* Current processed file */
  off_t total_size; /* Total size of the files to be processed, in bytes */
  off_t progress; /* Total size of current processed files, in btytes */
  int percent; /* progress (percentage) */

  char* error_message;

  GThread* thread;
  GCond* wait_gui;
  GMutex* mutex;

  GtkWindow* parent_window;
  GtkWidget* progress_dlg;
  GtkLabel* current_label;
  GtkLabel* from_label;
  GtkProgressBar* progress_bar;

/* private data */
  gint timer;
  FileOperationState state;

  FileOperationCallback callback;
}FileOperation;

/*
* source_files sould be a newly allocated list, and it will be
* freed after file operation has been completed
* If callback is not NULL, you have to call file_operation_free
* to release the resources in your own callback.
*/
G_GNUC_MALLOC
FileOperation* file_operation_new ( GList* source_files,
                                    const char* dest_path,
                                    FileOperationType action,
                                    FileOperationCallback callback,
                                    GtkWindow* parent_window );

/* Set some actions for chmod, this array will be copied
* and stored in FileOperation */
void file_operation_set_chmod( FileOperation* file_operation,
                               guchar* chmod_actions );

void file_operation_set_chown( FileOperation* file_operation,
                               uid_t uid, gid_t gid );

void file_operation_run ( FileOperation* file_operation );

#define file_operation_copy_files( source_files, dest_path, callback, parent_win )  \
          file_operation_new ( source_files, dest_path, FO_COPY, callback, parent_win  )

#define file_operation_copy_file( source_file, dest_path, callback, parent_win  )  \
          file_operation_new ( g_list_append( NULL, g_strdup(source_file) ), \
                              dest_path, FO_COPY, callback, parent_win  )


#define file_operation_move_files( source_files, dest_path, callback, parent_win  )  \
          file_operation_new ( source_files, dest_path, FO_MOVE, callback, parent_win  )

#define file_operation_move_file( source_file, dest_path, callback, parent_win  )  \
          file_operation_new ( g_list_append( NULL, g_strdup(source_file) ), \
                              dest_path, FO_MOVE, callback, parent_win  )

#define file_operation_delete_files( source_files, callback, parent_win  )  \
          file_operation_new ( source_files, NULL, FO_DELETE, callback, parent_win  )

#define file_operation_delete_file( source_file, callback, parent_win  )  \
          file_operation_new ( g_list_append( NULL, g_strdup(source_file) ), \
                              NULL, FO_DELETE, callback, parent_win  )


void file_operation_cancel ( FileOperation* file_operation );

void file_operation_free ( FileOperation* file_operation );

void
on_cancel_button_clicked               (GtkButton       *button,
                                        gpointer         user_data);

#endif
