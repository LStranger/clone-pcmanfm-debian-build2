/*
*  C Implementation: appchooserdlg
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "app-chooser-dialog.h"
#include "app-chooser-dialog-ui.h"
#include "mime-action.h"

#include <gtk/gtk.h>
#include <glib.h>

#include "xdgmime.h"

enum{
  COL_APP_ICON = 0,
  COL_APP_NAME,
  COL_DESKTOP_FILE,
  N_COLS
};

static void init_list_view( GtkTreeView* view )
{
  GtkTreeViewColumn* col = gtk_tree_view_column_new();
  GtkCellRenderer* renderer;

  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start( col, renderer, FALSE );
  gtk_tree_view_column_set_attributes( col, renderer, "pixbuf",
                                       COL_APP_ICON, NULL );

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start( col, renderer, TRUE );
  gtk_tree_view_column_set_attributes( col, renderer, "text",
                                       COL_APP_NAME, NULL );

  gtk_tree_view_append_column ( view, col );
}

static gint sort_by_name(GtkTreeModel *model,
                         GtkTreeIter *a,
                         GtkTreeIter *b,
                         gpointer user_data )
{
  char* name_a, *name_b;
  gint ret = 0;
  gtk_tree_model_get( model, a, COL_APP_NAME, &name_a, -1 );
  if( name_a ) {
    gtk_tree_model_get( model, b, COL_APP_NAME, &name_b, -1 );
    if( name_b ) {
      ret = strcmp( name_a, name_b );
      g_free(name_b);
    }
    g_free(name_a);
  }
  return ret;
}

static GtkTreeModel* create_app_list( const char* mime_type )
{
  char** apps;
  GtkListStore* list;
  GtkTreeIter it;
  char**app;
  char* name;
  GdkPixbuf* icon = NULL;

  if( mime_type )  {
    apps = get_all_apps_for_mime_type( mime_type );
    if( !apps && xdg_mime_mime_type_subclass(mime_type, XDG_MIME_TYPE_PLAIN_TEXT) )
      apps = get_all_apps_for_mime_type( XDG_MIME_TYPE_PLAIN_TEXT );
  }
  else
    apps = get_apps_for_all_mime_types();

  list = gtk_list_store_new( N_COLS, GDK_TYPE_PIXBUF,
                             G_TYPE_STRING, G_TYPE_STRING );

  if( apps )
  {
    icon = NULL;
    for( app = apps; *app; ++app ) {
      if( ! mime_type || g_str_has_suffix(*app, ".desktop") )
        get_app_info_from_desktop_file( *app, &name, &icon, 24 );
      else{
        name = g_strdup( *app );
        icon = NULL;
      }

      gtk_list_store_append( list, &it );
      gtk_list_store_set( list, &it, COL_APP_ICON, icon,
                                    COL_APP_NAME, name,
                                    COL_DESKTOP_FILE, *app, -1);
      g_free( name );
    }
    g_strfreev( apps );
  }
  gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE(list),
                                    COL_APP_NAME, sort_by_name, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE(list),
                                         COL_APP_NAME, GTK_SORT_ASCENDING );
  return GTK_TREE_MODEL( list );
}

GtkWidget* app_chooser_dialog_new( GtkWindow* parent, const char* mime_type )
{
  GtkWidget* dlg = create_app_chooser_dlg();
  GtkWidget* file_type = lookup_widget( dlg, "file_type" );
  const char* mime_desc = get_mime_description( mime_type );
  GtkTreeView* view;

  if( mime_desc ) {
    /* mime_desc is cached and shouldn't be freed. */
    gtk_label_set_text( GTK_LABEL(file_type), mime_desc );
  }

  /* Don't set default handler for directories and files with unknown type */
  if( mime_type == XDG_MIME_TYPE_UNKNOWN ||
      mime_type == XDG_MIME_TYPE_DIRECTORY ) {
      gtk_widget_hide( lookup_widget(dlg, "set_default") );
  }

  view = GTK_TREE_VIEW( lookup_widget( dlg, "recommended_apps" ) );

  gtk_tree_view_set_model( view, create_app_list(mime_type) );
  init_list_view( view );
  gtk_widget_grab_focus( GTK_WIDGET(view) );

  gtk_window_set_transient_for( GTK_WINDOW(dlg), parent );
  return dlg;
}

void
on_notebook_switch_page                (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
  GtkWidget* dlg = (GtkWidget*)notebook;
  GtkTreeView* view;
  if( page_num == 1 )
  {
    view = GTK_TREE_VIEW( lookup_widget( dlg, "all_apps" ) );
    if( ! gtk_tree_view_get_model(view) ) {
      gtk_tree_view_set_model( view, create_app_list(NULL) );
      init_list_view( view );
      gtk_widget_grab_focus( GTK_WIDGET(view) );
    }
  }
}

/*
* Return selected application in a ``newly allocated'' string.
* Returned string is the file name of the *.desktop file or a command line.
* These two can be separated by check if the returned string is ended
* with ".desktop" postfix.
*/
char* app_chooser_dialog_get_selected_app( GtkWidget* dlg )
{
  char* app = NULL;
  GtkEntry* entry = GTK_ENTRY( lookup_widget(dlg, "cmdline") );
  GtkNotebook* notebook;
  int idx;
  GtkBin* scroll;
  GtkTreeView* view;
  GtkTreeSelection* tree_sel;
  GtkTreeModel* model;
  GtkTreeIter it;

  if( (app = gtk_entry_get_text(entry)) && *app ) {
    return g_strdup( app );
  }

  notebook = GTK_NOTEBOOK(lookup_widget( dlg, "notebook" ));
  idx = gtk_notebook_get_current_page ( notebook );
  scroll = GTK_BIN( gtk_notebook_get_nth_page(notebook, idx) );
  view = gtk_bin_get_child( scroll );
  tree_sel = gtk_tree_view_get_selection( view );

  if( gtk_tree_selection_get_selected (tree_sel, &model, &it) ) {
    gtk_tree_model_get( model, &it, COL_DESKTOP_FILE, &app, -1 );
  }
  return app;
}

/*
* Check if the user set the selected app default handler.
*/
gboolean app_chooser_dialog_get_set_default( GtkWidget* dlg )
{
  GtkWidget* check = lookup_widget( dlg, "set_default" );
  return gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON(check) );
}

void
on_browse_btn_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  char *filename;
  GtkEntry* entry;
  GtkWidget* parent = GTK_WIDGET( button );
  GtkWidget* dlg = gtk_file_chooser_dialog_new(NULL, GTK_WINDOW(parent),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               GTK_STOCK_OPEN,
                                               GTK_RESPONSE_OK,
                                               GTK_STOCK_CANCEL,
                                               GTK_RESPONSE_CANCEL,
                                               NULL );
  /*
   Don't use this because it's too slow.
   gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER (dlg), "/usr/bin" );
  */
  if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
  {
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
    entry = GTK_ENTRY( lookup_widget(parent, "cmdline") );
    gtk_entry_set_text( entry, filename );
    g_free (filename);
  }
  gtk_widget_destroy( dlg );
}

