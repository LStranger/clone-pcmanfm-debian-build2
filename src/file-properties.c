#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "file-properties.h"
#include "file-properties-ui.h"
#include "glade-support.h"

#include "xdgmime.h"
#include "mime-description.h"

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "file-operation.h"

const char* chmod_names[]={
  "owner_r", "owner_w", "owner_x",
  "group_r", "group_w", "group_x",
  "others_r", "others_w", "others_x",
  "set_uid", "set_gid", "sticky"
};


typedef struct  {
  GList* file_list;
  GList* mime_type_list;
  GtkWidget* dlg;

  GtkEntry* owner;
  GtkEntry* group;
  char* owner_name;
  char* group_name;

  GtkToggleButton* chmod_btns[N_CHMOD_ACTIONS];
  guchar chmod_states[N_CHMOD_ACTIONS];

  GtkLabel* total_size_label;
  GtkLabel* size_on_disk_label;
  off_t total_size;
  off_t size_on_disk;
  guint total_count;
  gboolean cancel;
  gboolean done;
  GThread* calc_size_thread;
  guint update_label_timer;
}FilePropertiesDialogData;

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: path is encoded in on-disk encoding and not necessarily UTF-8.
*/
static void calc_total_size_of_files( const char* path, FilePropertiesDialogData* data )
{
  GDir* dir;
  const char* name;
  char* full_path;
  struct stat file_stat;

  if( data->cancel )
    return;

  if( lstat( path, &file_stat ) )
    return;

  data->total_size += file_stat.st_size;
  data->size_on_disk += (file_stat.st_blocks << 9 ); /* block x 512 */
  ++data->total_count;

  dir = g_dir_open( path, 0, NULL );
  if( dir )  {
    while( !data->cancel && (name = g_dir_read_name(dir)) )  {
      full_path = g_build_filename( path, name, NULL );
      lstat( full_path, &file_stat );
      if( S_ISDIR( file_stat.st_mode ) )  {
        calc_total_size_of_files( full_path, data );
      }
      else
      {
        data->total_size += file_stat.st_size;
        data->size_on_disk += (file_stat.st_blocks << 9);
        ++data->total_count;
      }
      g_free( full_path );
    }
    g_dir_close( dir );
  }
}

static gpointer calc_size( gpointer user_data )
{
  FilePropertiesDialogData* data = (FilePropertiesDialogData*)user_data;
  GList* l;
  char* path;
  for ( l = data->file_list; l; l = l->next ) {
    if( data->cancel )
      break;
    path = g_filename_from_utf8( (char*)l->data, -1, NULL, NULL, NULL );
    if( path ) {
      calc_total_size_of_files( path, data );
      g_free( path );
    }
  }
  data->done = TRUE;
  return NULL;
}

gboolean on_update_labels( FilePropertiesDialogData* data )
{
  char buf[64];
  char buf2[32];

  file_size_to_string( buf2, data->total_size );
  sprintf( buf, "%s  (%llu Bytes)", buf2, (guint64)data->total_size );
  gtk_label_set_text( data->total_size_label, buf );

  file_size_to_string( buf2, data->size_on_disk );
  sprintf( buf, "%s  (%llu Bytes)", buf2, (guint64)data->size_on_disk );
  gtk_label_set_text( data->size_on_disk_label, buf );

  return !data->done;
}

static void on_chmod_btn_toggled( GtkToggleButton* btn,
                                  FilePropertiesDialogData* data )
{
  /* Bypass the default handler */
  g_signal_stop_emission_by_name( btn, "toggled" );
  /* Block this handler while we are changing the state of buttons,
    or this handler will be called recursively. */
  g_signal_handlers_block_matched( btn, G_SIGNAL_MATCH_FUNC, 0,
                                   NULL, NULL, on_chmod_btn_toggled, NULL );

  if( gtk_toggle_button_get_inconsistent( btn ) ) {
    gtk_toggle_button_set_inconsistent( btn, FALSE );
    gtk_toggle_button_set_active( btn, FALSE );
  }
  else if( ! gtk_toggle_button_get_active( btn ) )
  {
    gtk_toggle_button_set_inconsistent( btn, TRUE );
  }

  g_signal_handlers_unblock_matched( btn, G_SIGNAL_MATCH_FUNC, 0,
                                     NULL, NULL, on_chmod_btn_toggled, NULL );
}

GtkWidget* file_properties_dlg_new( GtkWindow* parent, GList* sel_files,
                                    GList* sel_mime_types  )
{
  GtkWidget* dlg = create_filePropertiesDlg();

  FilePropertiesDialogData* data;
  gboolean need_calc_size = TRUE;

  const char* multiple_files = _("Multiple files are selected");
  const char* calculating;
  const char* mime;
  GtkWidget* name = lookup_widget( dlg, "file_name" );
  GtkWidget* location = lookup_widget( dlg, "location" );
  GtkWidget* mime_type = lookup_widget( dlg, "mime_type" );

  GtkWidget* mtime = lookup_widget( dlg, "mtime" );
  GtkWidget* atime = lookup_widget( dlg, "atime" );

  char buf[64];
  char buf2[32];
  const time_format = "%Y-%m-%d %H:%M";

  gchar* file_name;
  gchar* file_dir;
  gchar* file_path;
  gchar* file_type;

  struct passwd* pw;
  struct group* grp;

  struct stat statbuf;
  int i;
  GList* l;
  gboolean same_type = TRUE;

  gtk_window_set_transient_for( GTK_WINDOW(dlg), parent);

  data = g_new0(FilePropertiesDialogData, 1);
  g_object_set_data( G_OBJECT(dlg), "DialogData", data );
  data->file_list = sel_files;
  data->mime_type_list = sel_mime_types;
  data->dlg = dlg;

  file_dir = g_path_get_dirname( (char*)sel_files->data );
  gtk_label_set_text( GTK_LABEL(location), file_dir );
  g_free( file_dir );

  data->total_size_label = GTK_LABEL(lookup_widget( dlg, "total_size" ));
  data->size_on_disk_label = GTK_LABEL(lookup_widget( dlg, "size_on_disk" ));
  data->owner = GTK_ENTRY( lookup_widget( dlg, "owner" ) );
  data->group = GTK_ENTRY( lookup_widget( dlg, "group" ) );

  for( i = 0; i < N_CHMOD_ACTIONS; ++i ) {
    data->chmod_btns[i] = GTK_TOGGLE_BUTTON(lookup_widget( dlg, chmod_names[i] ));
  }

  for( l = sel_mime_types; l; l = l->next ) {
    if( l->next && l->next->data != l->data ) {
      same_type = FALSE;
      break;
    }
  }

  if( same_type )
  {
    mime = get_mime_description( (const char*)sel_mime_types->data );
    /*
    file_type = g_strdup_printf( "%s (%s)", mime, (const char*)sel_mime_types->data );
    gtk_label_set_text( GTK_LABEL(mime_type), file_type );
    g_free( file_type );
    */
    gtk_label_set_text( GTK_LABEL(mime_type), mime );
  }
  else {
    gtk_label_set_text( GTK_LABEL(mime_type), _("Multiple files of different types") );
  }

  /* Multiple files are selected */
  if( sel_files && sel_files->next )
  {
    gtk_widget_set_sensitive( name, FALSE );
    gtk_entry_set_text( GTK_ENTRY(name), multiple_files );

    gtk_label_set_text( GTK_LABEL(mtime), multiple_files );
    gtk_label_set_text( GTK_LABEL(atime), multiple_files );

    for( i = 0; i < N_CHMOD_ACTIONS; ++i ) {
      gtk_toggle_button_set_inconsistent ( data->chmod_btns[i], TRUE );
      data->chmod_states[i] = 2;
      g_signal_connect(G_OBJECT(data->chmod_btns[i]), "toggled",
                       G_CALLBACK(on_chmod_btn_toggled), data );
    }
  }
  else
  {
    gtk_editable_set_editable (GTK_EDITABLE(name), FALSE);
    file_name = g_path_get_basename( (char*)sel_files->data );
    gtk_entry_set_text( GTK_ENTRY(name), file_name );
    g_free( file_name );

    file_path = g_filename_from_utf8( (char*)sel_files->data,
                                       -1, NULL, NULL, NULL );
    if( stat( file_path, &statbuf ) )
    {
      if( ! S_ISDIR(statbuf.st_mode) )
      {
        /* Only single "file" is selected, so we don't need to
           caculate total file size */
        need_calc_size = FALSE;

        file_size_to_string( buf2, statbuf.st_size );
        sprintf( buf, "%s  (%llu Bytes)", buf2, (guint64)statbuf.st_size );
        gtk_label_set_text( data->total_size_label, buf );

        file_size_to_string( buf2, (guint64)(statbuf.st_blocks * 512) );
        sprintf( buf, "%s  (%llu Bytes)", buf2, (guint64)(statbuf.st_blocks * 512) );
        gtk_label_set_text( data->size_on_disk_label, buf );
      }
    }
    g_free( file_path );

    strftime( buf, sizeof(buf),
              time_format, localtime( &statbuf.st_mtime ) );
    gtk_label_set_text( GTK_LABEL(mtime), buf );

    strftime( buf, sizeof(buf),
              time_format, localtime( &statbuf.st_atime ) );
    gtk_label_set_text( GTK_LABEL(atime), buf );

    pw = getpwuid( statbuf.st_uid );
    if( pw && pw->pw_name && *pw->pw_name )
      data->owner_name = g_strdup(pw->pw_name);
    else
      data->owner_name = g_strdup_printf("%u", statbuf.st_uid );
    gtk_entry_set_text( GTK_ENTRY(data->owner), data->owner_name );

    grp = getgrgid( statbuf.st_gid );
    if( grp && grp->gr_name && *grp->gr_name )
      data->group_name = g_strdup(grp->gr_name);
    else
      data->group_name = g_strdup_printf("%u", statbuf.st_gid );
    gtk_entry_set_text( GTK_ENTRY(data->group), data->group_name );

    for( i = 0; i < N_CHMOD_ACTIONS; ++i ) {
      if( data->chmod_btns[i] != 2 ) {
        data->chmod_states[i] = (statbuf.st_mode & chmod_flags[i] ? 1 : 0);
        gtk_toggle_button_set_active( data->chmod_btns[i], data->chmod_states[i] );
      }
    }
  }

  if( need_calc_size ) {
    /* The total file size displayed in "File Properties" is not
       completely calculated yet. So "Calculating..." is displayed. */
    calculating = _("Calculating...");
    gtk_label_set_text( data->total_size_label, calculating );
    gtk_label_set_text( data->size_on_disk_label, calculating );

    g_object_set_data( G_OBJECT(dlg), "calc_size", data );
    data->calc_size_thread = g_thread_create ( (GThreadFunc)calc_size,
                                                data, TRUE, NULL );
    data->update_label_timer = g_timeout_add( 250,
                                              (GSourceFunc)on_update_labels,
                                              data );
  }

  return dlg;
}

void file_size_to_string( char* buf, guint64 size )
{
  char* unit;
  guint point;

  if( size > ((guint64)1)<<30 ) {
    if( size > ((guint64)1)<<40 ) {
      size /= (((guint64)1<<40)/10);
      point = (guint)(size % 10);
      size /= 10;
      unit = "TB";
    }
    else {
      size /= ((1<<30)/10);
      point = (guint)(size % 10);
      size /= 10;
      unit = "GB";
    }
  }
  else if( size > (1<<20) ) {
    size /= ((1<<20)/10);
    point = (guint)(size % 10);
    size /= 10;
    unit = "MB";
  }
  else if( size > (1<<10) ) {
    size /= ((1<<10)/10);
    point = size % 10;
    size /= 10;
    unit = "KB";
  }
  else {
    unit = size > 1 ? "Bytes" : "Byte";
    sprintf( buf, "%u %s", (guint)size, unit );
    return;
  }
  sprintf( buf, "%llu.%u %s", size, point, unit );
}

uid_t uid_from_name( const char* user_name )
{
  struct passwd* pw;
  uid_t uid = -1;
  const char* p;

  pw = getpwnam( user_name );
  if( pw ) {
    uid = pw->pw_uid;
  }
  else {
    uid = 0;
    for( p = user_name; *p; ++p ) {
      if( !g_ascii_isdigit(*p) )
        return -1;
      uid *= 10;
      uid += (*p-'0');
    }
#if 0 /* This is not needed */
    /* Check the existance */
    pw = getpwuid( uid );
    if( !pw ) /* Invalid uid */
      return -1;
#endif
  }
  return uid;
}

gid_t gid_from_name( const char* group_name )
{
  struct group* grp;
  gid_t gid = -1;
  const char* p;

  grp = getgrnam( group_name );
  if( grp ) {
    gid = grp->gr_gid;
  }
  else {
    gid = 0;
    for( p = group_name; *p; ++p ) {
      if( !g_ascii_isdigit(*p) )
        return -1;
      gid *= 10;
      gid += (*p-'0');
    }
#if 0 /* This is not needed */
    /* Check the existance */
    grp = getgrgid( gid );
    if( !grp ) /* Invalid gid */
      return -1;
#endif
  }
  return gid;
}

void
on_filePropertiesDlg_response          (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data)
{
  FilePropertiesDialogData* data;
  FileOperation* file_operation;
  gboolean mod_change;
  uid_t uid = -1;
  gid_t gid = -1;
  const char* owner_name;
  const char* group_name;
  guchar* chmod_actions;
  int i;
  GList* l;
  GtkWidget* ask_recursive;

  data = (FilePropertiesDialogData*)g_object_get_data( G_OBJECT(dialog),
                                                       "DialogData" );
  if( data )
  {
    if( data->update_label_timer )
      g_source_remove( data->update_label_timer );
    data->cancel = TRUE;

    if( data->calc_size_thread )
      g_thread_join( data->calc_size_thread );

    if( response_id == GTK_RESPONSE_OK )
    {
      /* Check if we need chown */
      owner_name = gtk_entry_get_text(data->owner);
      if( owner_name && *owner_name && strcmp( owner_name, data->owner_name ) )  {
        uid = uid_from_name( owner_name );
        if( uid == -1 ) {
          ptk_show_error( GTK_WINDOW(dialog), _("Invalid User") );
          return;
        }
      }
      group_name = gtk_entry_get_text(data->group);
      if( group_name && *group_name && strcmp( group_name, data->group_name ) )  {
        gid = gid_from_name( group_name );
        if( gid == -1 ) {
          ptk_show_error( GTK_WINDOW(dialog), _("Invalid Group") );
          return;
        }
      }

      for( i = 0; i < N_CHMOD_ACTIONS; ++i ) {
        if( gtk_toggle_button_get_inconsistent( data->chmod_btns[i] ) )
        {
          data->chmod_states[i] = 2;  /* Don't touch this bit */
        }
        else if( data->chmod_states[i] != gtk_toggle_button_get_active( data->chmod_btns[i] ) )
        {
          mod_change = TRUE;
          data->chmod_states[i] = gtk_toggle_button_get_active( data->chmod_btns[i] );
        }
        else /* Don't change this bit */
        {
          data->chmod_states[i] = 2;
        }
      }

      if( uid != -1 || gid != -1 || mod_change )
      {
        file_operation = file_operation_new( data->file_list, NULL, FO_CHMOD_CHOWN,
                                            NULL, gtk_widget_get_parent(GTK_WIDGET(dialog)) );

        for( l = data->mime_type_list; l; l = l->next ) {
          if( l->data ==  XDG_MIME_TYPE_DIRECTORY ) {
            ask_recursive = gtk_message_dialog_new( GTK_WINDOW(data->dlg),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_QUESTION,
                                                    GTK_BUTTONS_YES_NO,
                                                    _("Do you want to recursively apply these changes to all files and sub-folders?"));
            file_operation->recursive = (GTK_RESPONSE_YES == gtk_dialog_run( GTK_DIALOG(ask_recursive) ));
            gtk_widget_destroy( ask_recursive );
            break;
          }
        }

        if( mod_change ) { /* If the permissions of file has been changed by the user */
          file_operation_set_chmod( file_operation, data->chmod_states );
        }
        /* For chown */
        file_operation_set_chown( file_operation, uid, gid );
        file_operation_run( file_operation );

        /*
        * This file list will be freed by file operation, so we don't
        * need to do this. Just set the pointer to NULL.
        */
        data->file_list = NULL;
      }
    }

    g_free( data->owner_name );
    g_free( data->group_name );
    /*
     *NOTE: File operation chmod/chown will free the list when it's done,
     *and we only need to free it when there is no file operation applyed.
    */
    if( data->file_list ) {
      g_list_foreach( data->file_list, (GFunc)g_free, NULL );
      g_list_free( data->file_list );
    }
    if( data->mime_type_list ) {
      /*
        NO, this is NOT required because these strings are not newly allocated
        g_list_foreach( data->mime_type_list, (GFunc)g_free, NULL );
      */
      g_list_free( data->mime_type_list );
    }
    g_free( data );
  }

  gtk_widget_destroy( GTK_WIDGET(dialog) );
}

