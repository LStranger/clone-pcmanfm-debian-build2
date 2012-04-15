/*
*  C Implementation: fileoperation
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "file-operation.h"
#include "file-operation-ui.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <glib.h>

#include "glade-support.h"

#include <stdio.h>
#include <errno.h>

const mode_t chmod_flags[]={
  S_IRUSR, S_IWUSR, S_IXUSR,
  S_IRGRP, S_IWGRP, S_IXGRP,
  S_IROTH, S_IWOTH, S_IXOTH,
  S_ISUID, S_ISGID, S_ISVTX
};

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
static void get_total_size_of_dir( const char* path,
                                   off_t* size,
                                   gboolean* cancel );

static gboolean destroy_file_operation( FileOperation* file_operation );

static void report_error( FileOperation* file_operation );

static gboolean open_up_progress_dlg( FileOperation* file_operation );

static void
file_operation_delete_do ( const char* src_file,
                           FileOperation* file_operation );

static gboolean update_dialog_progress( FileOperation* file_operation )
{
  g_mutex_lock( file_operation->mutex );
  char text[10];
  sprintf( text, "%d %%", file_operation->percent );
  gtk_progress_bar_set_fraction( file_operation->progress_bar,
                                 ((gdouble)file_operation->percent)/100 );
  gtk_progress_bar_set_text(file_operation->progress_bar, text);

  g_cond_signal( file_operation->wait_gui );
  g_mutex_unlock( file_operation->mutex );
  return FALSE;
}

static void update_percentage( FileOperation* file_operation )
{
  gdouble percent = ((gdouble)file_operation->progress) / file_operation->total_size;
  int ipercent = (int)(percent * 100);

  if( ipercent != file_operation->percent ){
    file_operation->percent = ipercent;
    if( file_operation->progress_bar )  {
      g_mutex_lock( file_operation->mutex );
      g_idle_add( (GSourceFunc)update_dialog_progress, file_operation );
      g_cond_wait( file_operation->wait_gui, file_operation->mutex );
      g_mutex_unlock( file_operation->mutex );
    }
  }

}

static gboolean update_dialog_cur_file( FileOperation* file_operation )
{
  g_mutex_lock( file_operation->mutex );
  if( file_operation->current_file
      && file_operation->current_label ) {
    gtk_label_set_text( file_operation->current_label,
                        file_operation->current_file);
  }
  g_cond_signal( file_operation->wait_gui );
  g_mutex_unlock( file_operation->mutex );
  return FALSE;
}

static gboolean update_dialog_src_file( FileOperation* file_operation )
{
  g_mutex_lock( file_operation->mutex );
  if( file_operation->src_path ) {
    gtk_label_set_text( file_operation->from_label,
                        file_operation->src_path);
  }
  g_cond_signal( file_operation->wait_gui );
  g_mutex_unlock( file_operation->mutex );
  return FALSE;
}

static gboolean query_cancel( FileOperation* file_operation )
{
  GtkWidget* dlg;
  g_mutex_lock( file_operation->mutex );
  dlg = gtk_message_dialog_new( file_operation->parent_window,
                                GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_YES_NO,
                                _("Cancel the operation?") );
  if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_YES )
    file_operation->state = FOS_CANCELLED;
  else
    file_operation->state = FOS_RUNNING;
  gtk_widget_destroy( dlg );

  g_cond_signal( file_operation->wait_gui );
  g_mutex_unlock( file_operation->mutex );
  return FALSE;
}

void check_cancel( FileOperation* file_operation )
{
  if( file_operation->state == FOS_QUERYCANCEL )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)query_cancel, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }
}

/* private use */
typedef struct {
  struct stat src_stat;
  struct stat dest_stat;
  const char* dest_file;
  char* new_dest_file;
  FileOperation* file_operation;
  gint ret_val;
  gboolean is_src_dir;
  gboolean is_dest_dir;
  gboolean different_files;
}_FileOperationQueryOverwriteParm;

enum{
  RESPONSE_OVERWRITE = 1<<0,
  RESPONSE_OVERWRITEALL = 1<<1,
  RESPONSE_RENAME = 1<<2,
  RESPONSE_SKIP = 1<<3,
  RESPONSE_SKIPALL = 1<<4
};

static void on_file_name_entry_changed( GtkEntry* entry, GtkDialog* dlg )
{
  const char* old_name;
  gboolean can_rename;
  const char* new_name = gtk_entry_get_text(entry);

  old_name = (const char*)g_object_get_data(G_OBJECT(entry), "old_name");
  can_rename = new_name && (0 != strcmp( new_name, old_name) );

  gtk_dialog_set_response_sensitive (dlg, RESPONSE_RENAME, can_rename);
  gtk_dialog_set_response_sensitive (dlg, RESPONSE_OVERWRITE, !can_rename);
  gtk_dialog_set_response_sensitive (dlg, RESPONSE_OVERWRITEALL, !can_rename);
}

static gboolean query_overwrite( _FileOperationQueryOverwriteParm* param )
{
  GtkWidget* dlg;
  GtkEntry* entry;
  const char* message;
  const char* question;

  int response;
  gboolean has_overwrite_btn = TRUE;
  FileOperation* file_operation;
  char* file_name;
  char* ufile_name;
  char* dir_name;
  gboolean restart_timer = FALSE;

  g_mutex_lock( param->file_operation->mutex );

  file_operation = param->file_operation;

  if( file_operation->timer ) {
    g_source_remove( file_operation->timer );
    file_operation->timer = 0;
    restart_timer = TRUE;
  }

  if( param->different_files && param->is_dest_dir == param->is_src_dir )
  {
    if( param->is_dest_dir ) {
      /* Ask the user whether to overwrite dir content or not */
      question = _("Do you want to overwrite the folder and its content?");
    }
    else {
      /* Ask the user whether to overwrite the file or not */
      question = _("Do you want to overwrite this file?");
    }
  }
  else  { /* Rename is required */
    question = _("Please choose a new file name.");
    has_overwrite_btn = FALSE;
  }

  if( param->different_files ) {
    /* Ths first %s is a file name, and the second one represents followed message.
       ex: "/home/pcman/some_file" has existed.\n\nDo you want to overwrite existing file?" */
    message = _("\"%s\" has existed.\n\n%s");
  }
  else {
    /* Ths first %s is a file name, and the second one represents followed message. */
    message = _("\"%s\"\n\nDestination and source are the same file.\n\n%s");
  }

  dlg = gtk_message_dialog_new( param->file_operation->parent_window,
                                GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_NONE,
                                message,
                                param->dest_file,
                                question );

  if( has_overwrite_btn ) {
    gtk_dialog_add_buttons ( GTK_DIALOG(dlg),
                             _("Overwrite"), RESPONSE_OVERWRITE,
                             _("Overwrite All"), RESPONSE_OVERWRITEALL,
                             NULL );
  }

  gtk_dialog_add_buttons ( GTK_DIALOG(dlg),
                           _("Rename"), RESPONSE_RENAME,
                           _("Skip"), RESPONSE_SKIP,
                           _("Skip All"), RESPONSE_SKIPALL,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           NULL );

  file_name = g_path_get_basename(param->dest_file);
  ufile_name = g_filename_to_utf8( file_name, -1, NULL, NULL, NULL );
  g_free( file_name );

  entry = (GtkEntry*)gtk_entry_new();
  g_object_set_data_full( G_OBJECT(entry), "old_name",
                          ufile_name, g_free );
  g_signal_connect( G_OBJECT(entry), "changed",
                    G_CALLBACK(on_file_name_entry_changed), dlg );

  gtk_entry_set_text( entry, ufile_name );

  gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox), GTK_WIDGET(entry),
                      FALSE, TRUE, 4 );

  gtk_widget_show_all( dlg );
  response = gtk_dialog_run( GTK_DIALOG(dlg) );
  param->ret_val = response;

  switch( response )
  {
    case RESPONSE_OVERWRITEALL: /* Overwrite All */
      param->file_operation->overwrite_all = TRUE;
      param->ret_val = RESPONSE_OVERWRITE;
      break;
    case RESPONSE_SKIPALL: /* Skip All */
      param->file_operation->skip_all = TRUE;
      param->ret_val = RESPONSE_SKIP;
      break;
    case RESPONSE_RENAME: /* Rename */
        dir_name = g_path_get_dirname( param->dest_file );
        param->new_dest_file = g_build_filename( dir_name,
                                                 gtk_entry_get_text(entry),
                                                 NULL );
        g_free( dir_name );
      break;
    case GTK_RESPONSE_CANCEL:
      param->file_operation->state = FOS_CANCELLED;
      break;
  }
  gtk_widget_destroy(dlg);

  if( restart_timer ) {
    file_operation->timer = g_timeout_add( 500,
                                           (GSourceFunc)open_up_progress_dlg,
                                           (gpointer)file_operation );
  }

  g_cond_signal( param->file_operation->wait_gui );
  g_mutex_unlock( param->file_operation->mutex );
  return FALSE;
}

/*
* Check if the destination file exists.
* If the dest_file exists, let the user choose a new destination,
* skip this file, overwrite existing file, or cancel all file operation.
* The returned string is the new destination file choosed by the user
*/
gint check_overwrite( const gchar* dest_file,
                      char** new_dest_file,
                      FileOperation* file_operation )
{
  gint ret = 0;
  _FileOperationQueryOverwriteParm param = {0};

  *new_dest_file = NULL;

  if( lstat( file_operation->current_file, &param.src_stat ) != -1) {
    while( lstat( dest_file, &param.dest_stat ) != -1 ) {
      /* destination file exists */
      if( file_operation->skip_all ) /* Skip without check */
        return RESPONSE_SKIP;

      param.different_files = (0 != strcmp( file_operation->current_file,
                                            dest_file ));

      param.is_src_dir = !!S_ISDIR( param.dest_stat.st_mode );
      param.is_dest_dir = !!S_ISDIR( param.src_stat.st_mode );

      /* Overwrite without check if possible */
      if( file_operation->overwrite_all
          && param.different_files
          && (param.is_src_dir == param.is_dest_dir) )
        return RESPONSE_OVERWRITE;

      file_operation->state = FOS_QUERYOVERWRITE;
      param.dest_file = dest_file;
      param.file_operation = file_operation;

      g_mutex_lock( file_operation->mutex );
      g_idle_add( (GSourceFunc)query_overwrite, &param );
      g_cond_wait( file_operation->wait_gui, file_operation->mutex );
      g_mutex_unlock( file_operation->mutex );

      dest_file = param.new_dest_file;
      *new_dest_file = param.new_dest_file;

      ret = param.ret_val;
      if( ret != RESPONSE_RENAME )
        break;
      file_operation->state = FOS_RUNNING;
    }
  }
  return ret;
}

static void
file_operation_copy_do ( const char* src_file, const gchar* dest_file,
                         FileOperation* file_operation )
{
  GDir* dir;
  gchar* file_name;
  gchar* sub_src_file;
  gchar* sub_dest_file;
  struct stat file_stat;
  char buffer[4096];
  int rfd;
  int wfd;

  ssize_t rsize;
  char* new_dest_file = NULL;
  int overwrite_mode;
  int result;

  if( lstat( src_file, &file_stat ) == -1 )
  {
    /* g_print("stat error: %d\n", errno); */
    return; /* Error occurred */
  }

  check_cancel( file_operation );
  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }

  file_operation->current_file = src_file;

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_cur_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  result = 0;
  if( S_ISDIR( file_stat.st_mode ) )
  {
    if( overwrite_mode = check_overwrite( dest_file, &new_dest_file,
                                          file_operation ) )
    {
      if( overwrite_mode & (RESPONSE_SKIP|GTK_RESPONSE_CANCEL) )
        return;
      if( new_dest_file )
        dest_file = new_dest_file;
    }

    if( overwrite_mode != RESPONSE_OVERWRITE )  {
      result = mkdir( dest_file, file_stat.st_mode );
    }

    if( result == 0 )
    {
      file_operation->progress += file_stat.st_size;
      update_percentage(file_operation);

      dir = g_dir_open( src_file, 0, NULL );
      while( file_name = g_dir_read_name( dir ) )
      {
        if( file_operation->state == FOS_CANCELLED )
          break;
        sub_src_file = g_build_filename( src_file, file_name, NULL );
        sub_dest_file = g_build_filename( dest_file, file_name, NULL );
        file_operation_copy_do( sub_src_file, sub_dest_file, file_operation );
        g_free( sub_dest_file );
        g_free( sub_src_file );
      }
      g_dir_close( dir );

      /* Move files to different device: Need to delete source files */
      if( file_operation->action == FO_MOVE ) {
        if( result = rmdir( src_file ) )
          report_error( file_operation );
      }
    }
    else {  /* result != 0, error occurred */
      report_error( file_operation );
    }
  }
  else if( S_ISLNK( file_stat.st_mode ) )
  {
    if( (rfd = readlink( src_file, buffer, sizeof(buffer) )) > 0 )
    {
      if( overwrite_mode = check_overwrite( dest_file, &new_dest_file,
          file_operation ) )
      {
        if( overwrite_mode & (RESPONSE_SKIP|GTK_RESPONSE_CANCEL) )
          return;
        if( new_dest_file )
          dest_file = new_dest_file;
      }

      if( (wfd = symlink(buffer, dest_file) ) == 0 )
      {
        /* Move files to different device: Need to delete source files */
        if( file_operation->action == FO_MOVE ) {
          result = unlink(src_file);
          if( result ) {
            report_error( file_operation );
          }
        }
        file_operation->progress += file_stat.st_size;
        update_percentage(file_operation);
      }
      else  {
        report_error( file_operation );
      }
    }
    else  {
      report_error( file_operation );
    }
  }
  else
  {
    if( (rfd = open( src_file, O_RDONLY )) >= 0 )
    {
      if( overwrite_mode = check_overwrite( dest_file, &new_dest_file,
          file_operation ) )
      {
        if( overwrite_mode & (RESPONSE_SKIP|GTK_RESPONSE_CANCEL) )
          return;
        if( new_dest_file )
          dest_file = new_dest_file;
      }

      if( (wfd = creat(dest_file, file_stat.st_mode) ) >= 0 )
      {
        while( (rsize = read( rfd, buffer, sizeof(buffer) )) > 0 )
        {
          check_cancel( file_operation );
          if( file_operation->state == FOS_CANCELLED )  {
            return;
          }

          if( write( wfd, buffer, rsize ) > 0 )
          {
            file_operation->progress += rsize;
          }
          else
          {
            report_error( file_operation );
            close( wfd );
          }

          update_percentage(file_operation);
        }
        close( wfd );

        /* Move files to different device: Need to delete source files */
        if( file_operation->action == FO_MOVE ) {
          result = unlink(src_file);
          if( result ) {
            report_error( file_operation );
          }
        }

      }
      else  {
        report_error( file_operation );
      }
      close(rfd);
    }
    else  {
      report_error( file_operation );
    }
  }
  if( new_dest_file )
    g_free( new_dest_file );
}

static void
file_operation_copy( char* src_file, FileOperation* file_operation )
{
  file_operation->src_path = src_file;

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_src_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  gchar* file_name = g_path_get_basename( src_file );
  gchar* dest_file = g_build_filename( file_operation->dest_path, file_name, NULL );
  g_free(file_name);
  file_operation_copy_do( src_file, dest_file, file_operation );
  g_free(dest_file);
}


static void
file_operation_move_do ( const char* src_file, const gchar* dest_file,
                             FileOperation* file_operation )
{
  GDir* dir;
  gchar* file_name;
  gchar* new_dest_file;
  gchar* sub_src_file;
  gchar* sub_dest_file;
  struct stat file_stat;

  int overwrite_mode, result;

  check_cancel( file_operation );
  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }

  if( lstat( src_file, &file_stat ) == -1 )
  {
    /* g_print("stat error: %d\n", errno); */
    return; /* Error occurred */
  }

  file_operation->current_file = src_file;

  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_cur_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  if( overwrite_mode = check_overwrite( dest_file, &new_dest_file,
      file_operation ) )
  {
    if( overwrite_mode & (RESPONSE_SKIP|GTK_RESPONSE_CANCEL) )
      return;
    if( new_dest_file )
      dest_file = new_dest_file;
  }

  result = rename( src_file, dest_file );

  if( new_dest_file )
    g_free( new_dest_file );

  if( result != 0 ) {
    report_error( file_operation );
    return;
  }

  file_operation->progress += file_stat.st_size;
  update_percentage(file_operation);
}


static void
file_operation_move( char* src_file, FileOperation* file_operation )
{
  struct stat src_stat;
  struct stat dest_stat;

  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }

  file_operation->src_path = src_file;

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_src_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  gchar* file_name = g_path_get_basename( src_file );
  gchar* dest_file = g_build_filename( file_operation->dest_path, file_name, NULL );
  g_free(file_name);

  if( lstat( src_file, &src_stat ) >= 0
      && lstat( file_operation->dest_path, &dest_stat ) >= 0)
  {
    /* Not on the same device */
    if( src_stat.st_dev != dest_stat.st_dev )  {
      /* g_print("not on the same dev: %s\n", src_file); */
      file_operation_copy_do( src_file, dest_file, file_operation );
      /* Remove source files */
//      if( file_operation->state != FOS_CANCELLED )  {
//        file_operation_delete_do( src_file, file_operation );
//      }
    }
    else  {
      /* g_print("on the same dev: %s\n", src_file); */
      file_operation_move_do( src_file, dest_file, file_operation );
    }
  }
  g_free(dest_file);
}


void
file_operation_delete_do ( const char* src_file,
                           FileOperation* file_operation )
{
  GDir* dir;
  gchar* file_name;
  gchar* sub_src_file;
  struct stat file_stat;
  int result;

  check_cancel( file_operation );
  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }

  if( lstat( src_file, &file_stat ) == -1 ) {
    return; /* Error occurred */
  }

  file_operation->current_file = src_file;

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_cur_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  if( S_ISDIR( file_stat.st_mode ) )
  {
    /* g_print( "Mkdir: %s\n\n", dest_file ); */
    dir = g_dir_open( src_file, 0, NULL );
    while( file_name = g_dir_read_name( dir ) )
    {
      if( file_operation->state == FOS_CANCELLED ) {
        break;
      }
      sub_src_file = g_build_filename( src_file, file_name, NULL );
      file_operation_delete_do( sub_src_file, file_operation );
      g_free( sub_src_file );
    }
    g_dir_close( dir );
    if( file_operation->state == FOS_CANCELLED ) {
      return;
    }
    result = rmdir( src_file );
    if( result != 0 )  {
      report_error( file_operation );
      file_operation->state = FOS_CANCELLED;
      return;
    }

    file_operation->progress += file_stat.st_size;
    update_percentage(file_operation);
  }
  else
  {
    result = unlink( src_file );
    if( result != 0 )  {
      report_error( file_operation );
      file_operation->state = FOS_CANCELLED;
      return;
    }

    file_operation->progress += file_stat.st_size;
    update_percentage(file_operation);
  }
}

static void
file_operation_delete( char* src_file, FileOperation* file_operation )
{
  file_operation->src_path = src_file;

  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_src_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  file_operation_delete_do( src_file, file_operation );
}

static void
file_operation_link( char* src_file, FileOperation* file_operation )
{
  int result;
  gchar* dest_file;
  gchar* file_name = g_path_get_basename( src_file );
  dest_file = g_build_filename( file_operation->dest_path, file_name, NULL );
  g_free(file_name);
  result = symlink( src_file, dest_file );
  g_print("link %s to %s\n", src_file, dest_file);
  g_free( dest_file );
}


static void
file_operation_chown_chmod( char* src_file, FileOperation* file_operation )
{
  struct stat src_stat;
  int i;
  GDir* dir;
  gchar* sub_src_file;
  gchar* file_name;
  mode_t new_mode;

  int result;

  check_cancel( file_operation );
  if( file_operation->state == FOS_CANCELLED )  {
    return;
  }
  file_operation->current_file = src_file;
  if( file_operation->progress_dlg )
  {
    g_mutex_lock( file_operation->mutex );
    g_idle_add( (GSourceFunc)update_dialog_cur_file, file_operation );
    g_cond_wait( file_operation->wait_gui, file_operation->mutex );
    g_mutex_unlock( file_operation->mutex );
  }

  if( lstat( src_file, &src_stat ) >= 0 )
  {
    /* chown */
    if( file_operation->uid != -1 || file_operation->gid != -1 ) {
      result = chown( src_file, file_operation->uid, file_operation->gid );
      if( result != 0 ) {
        report_error( file_operation );
        return;
      }
    }

    /* chmod */
    if( file_operation->chmod_actions ) {
      new_mode = src_stat.st_mode;
      for( i = 0; i < N_CHMOD_ACTIONS; ++i ) {
        if( file_operation->chmod_actions[i] == 2 ) /* Don't change */
          continue;
        if( file_operation->chmod_actions[i] == 0 ) /* Remove this bit */
          new_mode &= ~chmod_flags[i];
        else  /* Add this bit */
          new_mode |= chmod_flags[i];
      }
      if( new_mode != src_stat.st_mode ) {
        result = chmod( src_file, new_mode );
        if( result != 0 ) {
          report_error( file_operation );
          return;
        }
      }
    }

    file_operation->progress += src_stat.st_size;
    update_percentage(file_operation);

    if( S_ISDIR(src_stat.st_mode) && file_operation->recursive )
    {
      if( dir = g_dir_open( src_file, NULL, NULL ) )
      {
        while( file_name = g_dir_read_name(dir) )
        {
          if( file_operation->state == FOS_CANCELLED )
            break;
          sub_src_file = g_build_filename( src_file, file_name, NULL );
          file_operation_chown_chmod( sub_src_file, file_operation );
          g_free( sub_src_file );
        }
        g_dir_close( dir );
      }
    }
  }
}


gboolean open_up_progress_dlg( FileOperation* file_operation )
{
  char* actions[] = { _("Move: "), _("Copy: "), _("Delete: "), _("Link: "), _("Change Properties: ") };
  char* titles[] = { _("Moving..."), _("Copying..."), _("Deleting..."), _("Linking..."), _("Changing Properties") };
  GtkLabel* action;
  GtkLabel* from;
  GtkLabel* to;
  GtkLabel* cur;

  g_source_remove( file_operation->timer );
  file_operation->timer = 0;

  file_operation->progress_dlg = create_fileOperation();
  gtk_window_set_transient_for( GTK_WINDOW(file_operation->progress_dlg),
                                file_operation->parent_window );
  gtk_window_set_title( GTK_WINDOW(file_operation->progress_dlg),
                        titles[file_operation->action] );

  action = GTK_LABEL(lookup_widget(file_operation->progress_dlg, "action"));
  gtk_label_set_text( action, actions[file_operation->action] );

  from = GTK_LABEL( lookup_widget( file_operation->progress_dlg, "from" ) );
  gtk_label_set_text( from, file_operation->src_path );

  to = GTK_LABEL( lookup_widget( file_operation->progress_dlg, "to" ) );
  if( file_operation->dest_path )  {
    gtk_label_set_text( to, file_operation->dest_path );
  }
  else  {
    gtk_widget_hide( GTK_WIDGET(to) );
    gtk_widget_hide( lookup_widget( file_operation->progress_dlg, "to_label" ) );
  }

  cur = GTK_LABEL( lookup_widget( file_operation->progress_dlg, "current" ) );
  if( file_operation->src_path ) {
    gtk_label_set_text( cur, file_operation->src_path );
  }
  file_operation->from_label = from;
  file_operation->current_label = cur;
  file_operation->progress_bar = GTK_PROGRESS_BAR( lookup_widget(
                                             file_operation->progress_dlg,
                                             "progress" ) );

  gtk_widget_show( file_operation->progress_dlg );

  g_object_set_data( G_OBJECT( file_operation->progress_dlg ),
                     "FileOperation", file_operation );

  return TRUE;
}

static gboolean destroy_file_operation( FileOperation* file_operation )
{
  file_operation_free( file_operation );
  return FALSE;
}

static gpointer file_operation_thread ( FileOperation* file_operation )
{
  GList* l;
  struct stat file_stat;
  dev_t dest_dev;
  char* src_path;
  gboolean cancelled = FALSE;
  GFunc funcs[] = {(GFunc)&file_operation_move,
                   (GFunc)&file_operation_copy,
                   (GFunc)&file_operation_delete,
                   (GFunc)&file_operation_link,
                   (GFunc)&file_operation_chown_chmod};

  if( file_operation->action < FO_MOVE
      || file_operation->action >= FO_LAST )
    return NULL;

  file_operation->timer = g_timeout_add( 500,
                                         (GSourceFunc)open_up_progress_dlg,
                                         (gpointer)file_operation );
  file_operation->src_path = (char*)file_operation->source_paths->data;
  file_operation->total_size = 0;
  file_operation->state = FOS_RUNNING;

  /* Calculate total size of all files */
  if( file_operation->recursive )
  {
    for( l = file_operation->source_paths; l; l = l->next ) {
      get_total_size_of_dir( (char*)l->data, &file_operation->total_size, &cancelled );
    }
  }
  else
  {
    if( file_operation->action != FO_CHMOD_CHOWN ) {
      if( file_operation->dest_path &&
          lstat( file_operation->dest_path, &file_stat ) < 0 )
        return NULL;
      dest_dev = file_stat.st_dev;
    }

    /* g_print("%s\n", file_operation->source_paths->data); */
    for( l = file_operation->source_paths; l; l = l->next ) {
      if( lstat( (char*)l->data, &file_stat ) < 0 )
        continue;
      if( S_ISLNK( file_stat.st_mode ) )  /* Don't do deep copy for symlinks */
        file_operation->recursive = FALSE;
      else if( file_operation->action == FO_MOVE )
        file_operation->recursive = ( file_stat.st_dev != dest_dev );

      if( file_operation->recursive ) {
        get_total_size_of_dir( (char*)l->data,
                                &file_operation->total_size,
                                &cancelled );
      }
      else {
        file_operation->total_size += file_stat.st_size;
      }
    }
  }

  /*  g_print( "Total size = %lu\n", file_operation->total_size );  */
  g_list_foreach( file_operation->source_paths,
                  funcs[file_operation->action],
                  file_operation );

  /* Cancel the progress dialog if the file operation has been done in 1 second. */
  if( file_operation->timer ){
    g_source_remove( file_operation->timer );
    file_operation->timer = 0;
  }

  if( file_operation->chmod_actions )
    g_free(file_operation->chmod_actions);

  if( file_operation->callback )
    g_idle_add( (GSourceFunc)file_operation->callback, file_operation );
  else
    g_idle_add( (GSourceFunc)destroy_file_operation, file_operation );

  return NULL;
}


/*
* source_files sould be a newly allocated list, and it will be
* freed after file operation has been completed
* If callback is not NULL, you have to call file_operation_free
* to release the resources in your own callback.
*/
FileOperation* file_operation_new ( GList* source_files,
                                    const char* dest_path,
                                    FileOperationType action,
                                    FileOperationCallback callback,
                                    GtkWindow* parent_window )
{
  FileOperation* file_operation;

  file_operation = g_new0( FileOperation, 1 );
  file_operation->action = action;
  file_operation->source_paths = source_files;
  if( dest_path )
    file_operation->dest_path = g_strdup(dest_path);
  file_operation->callback = callback;
  file_operation->parent_window = parent_window;
  if( file_operation->action == FO_COPY
      || file_operation->action == FO_DELETE )
    file_operation->recursive = TRUE;

  return file_operation;
}

/* Set some actions for chmod, this array will be copied
* and stored in FileOperation */
void file_operation_set_chmod( FileOperation* file_operation,
                               guchar* chmod_actions )
{
  file_operation->chmod_actions = g_new0( guchar, N_CHMOD_ACTIONS );
  memcpy( (void*)file_operation->chmod_actions, (void*)chmod_actions,
           sizeof(guchar)*N_CHMOD_ACTIONS );
}

void file_operation_set_chown( FileOperation* file_operation,
                               uid_t uid, gid_t gid )
{
  file_operation->uid = uid;
  file_operation->gid = gid;
}

void file_operation_run ( FileOperation* file_operation )
{
  file_operation->wait_gui = g_cond_new();
  file_operation->mutex = g_mutex_new();
  file_operation->thread = g_thread_create((GThreadFunc)file_operation_thread,
                                            file_operation, TRUE, NULL );
}

void file_operation_cancel ( FileOperation* file_operation )
{
  g_mutex_lock( file_operation->mutex );
  file_operation->state = FOS_QUERYCANCEL;
  g_mutex_unlock( file_operation->mutex );
}

void file_operation_free ( FileOperation* file_operation )
{
  if( file_operation->source_paths )
    g_list_foreach( file_operation->source_paths, (GFunc)g_free, NULL );

  if( file_operation->wait_gui ){
    g_cond_free( file_operation->wait_gui );
    g_mutex_free( file_operation->mutex );
  }

  if( file_operation->progress_dlg )
    gtk_widget_destroy( file_operation->progress_dlg );
  g_free( file_operation );
}


void
on_cancel_button_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  FileOperation* file_operation;
  GtkWidget* dlg = (GtkWidget*)button;
  file_operation = (FileOperation*)g_object_get_data( dlg,
                                                      "FileOperation" );
  file_operation_cancel( file_operation );
}

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
void get_total_size_of_dir( const char* path,
                            off_t* size,
                            gboolean* cancel )
{
  GDir* dir;
  const char* name;
  char* full_path;
  struct stat file_stat;

  if( *cancel )
    return;

  lstat( path, &file_stat );

  *size += file_stat.st_size;
  if( S_ISLNK(file_stat.st_mode) )  /* Don't follow symlinks */
    return;

  dir = g_dir_open( path, 0, NULL );
  if( dir )  {
    while( name = g_dir_read_name(dir) )  {
      if( *cancel )
        break;
      full_path = g_build_filename( path, name, NULL );
      lstat( full_path, &file_stat );
      if( S_ISDIR( file_stat.st_mode ) )  {
        get_total_size_of_dir( full_path, size, cancel );
      }
      else {
        *size += file_stat.st_size;
      }
      g_free( full_path );
    }
    g_dir_close( dir );
  }
}

static gboolean report_error_do( FileOperation* file_operation )
{
  GtkWidget* dlg;
  GtkWindow* parent_window;
  gboolean restart_timer = FALSE;

  g_mutex_lock( file_operation->mutex );

  if( file_operation->timer ) {
    g_source_remove( file_operation->timer );
    file_operation->timer = 0;
    restart_timer = TRUE;
  }

  if( file_operation->progress_dlg )
    parent_window = file_operation->progress_dlg;
  else
    parent_window = file_operation->parent_window;

  dlg = gtk_message_dialog_new( parent_window, GTK_DIALOG_MODAL,
                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                /* The first %s is file name, and the second is
                                   detailed error message.  Their order shouldn't be changed */
                                _("File operation failed.\n\n"
                                  "Unable to process \"%s\"\n\n%s"),
                                file_operation->current_file,
                                file_operation->error_message );
  gtk_dialog_run( GTK_DIALOG(dlg) );
  file_operation->state = FOS_CANCELLED; /* Cancel the operation */
  gtk_widget_destroy( dlg );

  if( restart_timer ) {
    file_operation->timer = g_timeout_add( 500,
                                           (GSourceFunc)open_up_progress_dlg,
                                           (gpointer)file_operation );
  }

  g_cond_signal( file_operation->wait_gui );
  g_mutex_unlock( file_operation->mutex );
  return FALSE;
}

void report_error( FileOperation* file_operation )
{
  g_mutex_lock( file_operation->mutex );
  file_operation->error_message = g_strdup( g_strerror (errno) );
  g_idle_add( (GSourceFunc)report_error_do, file_operation );
  g_cond_wait( file_operation->wait_gui, file_operation->mutex );
  g_free( file_operation->error_message );
  file_operation->error_message = NULL;
  g_mutex_unlock( file_operation->mutex );
}

