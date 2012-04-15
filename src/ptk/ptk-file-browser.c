#include "ptk-file-browser.h"
#include <gtk/gtk.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <grp.h> /* Query group name */
#include <pwd.h> /* Query user name */
#include <time.h>

/* MIME type processing library from FreeDesktop.org */
#include "xdgmime.h"

#include "file-properties.h"
#include "file-icon.h"
#include "mime-action.h"
#include "input-dialog.h"
#include "folder-content.h"
#include "file-operation.h"
#include "mime-description.h"
#include "app-chooser-dialog.h"

#include "ptk-tree-model-sort.h"
#include "ptk-file-icon-renderer.h"
#include "ptk-utils.h"

#include "ptk-location-view.h"
#include "ptk-dir-tree-view.h"

#include "settings.h"

/* If set to FALSE, all selection changes in folder_view are prevented. */
static gboolean can_folder_view_sel_change = TRUE;

static void ptk_file_browser_class_init( PtkFileBrowserClass* klass );
static void ptk_file_browser_init( PtkFileBrowser* file_browser );
static void ptk_file_browser_finalize( GObject *obj );
static void ptk_file_browser_get_property ( GObject *obj,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec );
static void ptk_file_browser_set_property ( GObject *obj,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec );

/* Utility functions */
static gboolean update_folder_view_visible_region( PtkFileBrowser* file_browser );

static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFileBrowserViewMode view_mode );

static void init_list_view( GtkTreeView* list_view );

static void init_dir_tree( PtkFileBrowser* file_browser );

static GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser );

static GtkTreeView* ptk_file_browser_create_location_view( PtkFileBrowser* file_browser );

static GList* folder_view_get_selected_items( PtkFileBrowser* file_browser,
                                              GtkTreeModel** model );

static gpointer ptk_file_browser_chdir_thread( PtkFileBrowser* file_browser );

static gboolean ptk_file_browser_chdir_complete( PtkFileBrowser* file_browser );

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                       const char* app_desktop );

static GtkWidget* ptk_file_browser_get_popup_menu_for_file( PtkFileBrowser* file_browser,
                                                            const char* file_path,
                                                            const char* mime_type );

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y );

/* sort functions of folder view */
static gint sort_files_by_name  (GtkTreeModel *model,
                           GtkTreeIter *a,
                           GtkTreeIter *b,
                           gpointer user_data);

static gint sort_files_by_size  (GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data);

static gint sort_files_by_time  (GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data);


static gboolean filter_files  (GtkTreeModel *model,
                               GtkTreeIter *it,
                               gpointer user_data);

/* sort functions of dir tree */
static gint sort_dir_tree_by_name  (GtkTreeModel *model,
                                    GtkTreeIter *a,
                                    GtkTreeIter *b,
                                    gpointer user_data);

/* signal handlers */
static void
on_folder_view_item_activated           (PtkIconView     *iconview,
                                         GtkTreePath     *path,
                                         PtkFileBrowser* file_browser);
static void
on_folder_view_row_activated           (GtkTreeView     *tree_view,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn* col,
                                        PtkFileBrowser* file_browser);
static void
on_folder_view_item_sel_change (PtkIconView *iconview,
                               PtkFileBrowser* file_browser);
static gboolean
on_folder_view_key_press_event          (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        PtkFileBrowser* file_browser);
static gboolean
on_folder_view_button_press_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        PtkFileBrowser* file_browser);
static gboolean
on_folder_view_button_release_event       (GtkWidget       *widget,
                                          GdkEventButton  *event,
                                          PtkFileBrowser* file_browser);
static void
on_folder_view_scroll_scrolled       (GtkAdjustment *adjust,
                                        PtkFileBrowser* file_browser);
static void
on_folder_view_size_allocate            (GtkWidget       *widget,
                                        GdkRectangle    *allocation,
                                        PtkFileBrowser* file_browser);
static void
on_dir_tree_sel_changed           (GtkTreeSelection *treesel,
                                   PtkFileBrowser* file_browser);
static void
on_location_view_row_activated           (GtkTreeView *tree_view,
                                          GtkTreePath *path,
                                          GtkTreeViewColumn *column,
                                          PtkFileBrowser* file_browser );

static void
on_folder_view_drag_data_received (GtkWidget        *widget,
                                   GdkDragContext   *drag_context,
                                   gint              x,
                                   gint              y,
                                   GtkSelectionData *sel_data,
                                   guint             info,
                                   guint             time,
                                   gpointer          user_data);

static void
on_folder_view_drag_data_get (GtkWidget        *widget,
                              GdkDragContext   *drag_context,
                              GtkSelectionData *sel_data,
                              guint             info,
                              guint             time,
                              PtkFileBrowser  *file_browser);


static void
on_folder_view_drag_begin           (GtkWidget      *widget,
                                    GdkDragContext *drag_context,
                                    gpointer        user_data);
static void
on_folder_view_drag_begin           (GtkWidget      *widget,
                                    GdkDragContext *drag_context,
                                    gpointer        user_data);

static gboolean
on_folder_view_drag_motion (GtkWidget      *widget,
                            GdkDragContext *drag_context,
                            gint            x,
                            gint            y,
                            guint           time,
                            PtkFileBrowser* file_browser );

static gboolean
on_folder_view_drag_leave ( GtkWidget      *widget,
                            GdkDragContext *drag_context,
                            guint           time,
                            PtkFileBrowser* file_browser );
static gboolean
on_folder_view_drag_drop    (GtkWidget      *widget,
                            GdkDragContext *drag_context,
                            gint            x,
                            gint            y,
                            guint           time,
                            PtkFileBrowser* file_browser );
static void
on_folder_view_drag_end             (GtkWidget      *widget,
                                    GdkDragContext *drag_context,
                                    gpointer        user_data);


/* Signal handlers for popup menu */
static void
on_popup_open_activate                       (GtkMenuItem     *menuitem,
                                            gpointer         user_data);
static void
on_popup_open_with_another_activate          (GtkMenuItem     *menuitem,
                                            gpointer         user_data);
static void
on_file_properties_activate              (GtkMenuItem     *menuitem,
                                            gpointer         user_data);
static void
on_popup_run_app                       (GtkMenuItem     *menuitem,
                                        PtkFileBrowser* file_browser);
static void
on_popup_open_in_new_tab_activate      ( GtkMenuItem *menuitem,
                                         PtkFileBrowser* file_browser );
static void
on_popup_open_in_new_win_activate      ( GtkMenuItem *menuitem,
                                          PtkFileBrowser* file_browser );
static void
on_popup_cut_activate                        (GtkMenuItem     *menuitem,
                                              gpointer         user_data);
static void
on_popup_copy_activate                       (GtkMenuItem     *menuitem,
                                              gpointer         user_data);
static void
on_popup_paste_activate                      (GtkMenuItem     *menuitem,
                                              gpointer         user_data);
static void
on_popup_delete_activate                     (GtkMenuItem     *menuitem,
                                              gpointer         user_data);
static void
on_popup_rename_activate                      (GtkMenuItem     *menuitem,
                                               gpointer         user_data);
static void
on_popup_new_folder_activate                  (GtkMenuItem     *menuitem,
                                               gpointer         user_data);
static void
on_popup_new_text_file_activate               (GtkMenuItem     *menuitem,
                                               gpointer         user_data);
static void
on_popup_file_properties_activate               (GtkMenuItem     *menuitem,
                                                 gpointer         user_data);


/* Default signal handlers */
static void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                           const char* path,
                                           gboolean* cancel );
static void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser );
static void ptk_file_browser_content_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_sel_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action );
static void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser );

static void on_folder_content_update ( FolderContent* content,
                                       PtkFileBrowser* file_browser );


static GtkPanedClass *parent_class = NULL;

enum {
  BEFORE_CHDIR_SIGNAL,
  AFTER_CHDIR_SIGNAL,
  OPEN_ITEM_SIGNAL,
  CONTENT_CHANGE_SIGNAL,
  SEL_CHANGE_SIGNAL,
  PANE_MODE_CHANGE_SIGNAL,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static GdkDragAction clipboard_action = GDK_ACTION_DEFAULT;

static guint folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = 0;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[]={
  {"text/uri-list", 0 , 0 }
};


GType ptk_file_browser_get_type()
{
  static GType type = G_TYPE_INVALID;
  if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
  {
    static const GTypeInfo info =
    {
      sizeof ( PtkFileBrowserClass ),
      NULL,
      NULL,
      ( GClassInitFunc ) ptk_file_browser_class_init,
      NULL,
      NULL,
      sizeof ( PtkFileBrowser ),
      0,
      ( GInstanceInitFunc ) ptk_file_browser_init,
      NULL,
    };
    type = g_type_register_static ( GTK_TYPE_HPANED, "PtkFileBrowser", &info, 0 );
  }
  return type;
}

void ptk_file_browser_class_init( PtkFileBrowserClass* klass )
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = ( GObjectClass * ) klass;
  parent_class = g_type_class_peek_parent ( klass );

  object_class->set_property = ptk_file_browser_set_property;
  object_class->get_property = ptk_file_browser_get_property;
  object_class->finalize = ptk_file_browser_finalize;

  widget_class = GTK_WIDGET_CLASS ( klass );

  /* Signals */

  klass->before_chdir = ptk_file_browser_before_chdir;
  klass->after_chdir = ptk_file_browser_after_chdir;
  klass->open_item = ptk_file_browser_open_item;
  klass->content_change = ptk_file_browser_content_change;
  klass->sel_change = ptk_file_browser_sel_change;
  klass->pane_mode_change = ptk_file_browser_pane_mode_change;

  /* before-chdir is emitted when PtkFileBrowser is about to change
  * its working directory. The 1st param is the path of the dir (in UTF-8),
  * and the 2nd param is a gboolean*, which can be filled by the
  * signal handler with TRUE to cancel the operation.
  */
  signals[BEFORE_CHDIR_SIGNAL] =
      g_signal_new ("before-chdir",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, before_chdir),
                    NULL, NULL,
                    gtk_marshal_VOID__POINTER_POINTER,
                    G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

  signals[AFTER_CHDIR_SIGNAL] =
      g_signal_new ("after-chdir",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, after_chdir),
                    NULL, NULL,
                    gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /*
  * This signal is sent when a directory is about to be opened
  * arg1 is the path to be opened, and arg2 is the type of action,
  * ex: open in tab, open in terminal...etc.
  */
  signals[OPEN_ITEM_SIGNAL] =
      g_signal_new ("open-item",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, open_item),
                    NULL, NULL,
                    gtk_marshal_VOID__POINTER_INT, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

  signals[CONTENT_CHANGE_SIGNAL] =
      g_signal_new ("content-change",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, content_change),
                    NULL, NULL,
                    gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[SEL_CHANGE_SIGNAL] =
      g_signal_new ("sel-change",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, sel_change),
                    NULL, NULL,
                    gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[PANE_MODE_CHANGE_SIGNAL] =
      g_signal_new ("pane-mode-change",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (PtkFileBrowserClass, pane_mode_change),
                    NULL, NULL,
                    gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

}

void ptk_file_browser_init( PtkFileBrowser* file_browser )
{
  GtkAdjustment *adjust;

  file_browser->folder_view_scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_paned_pack2 (GTK_PANED (file_browser),
                   file_browser->folder_view_scroll, TRUE, TRUE);
  gtk_scrolled_window_set_policy (
      GTK_SCROLLED_WINDOW (file_browser->folder_view_scroll),
  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (
      GTK_SCROLLED_WINDOW (file_browser->folder_view_scroll),
  GTK_SHADOW_IN);
  adjust = gtk_scrolled_window_get_vadjustment(
      GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll) );
  g_signal_connect( G_OBJECT(adjust), "value-changed",
                    G_CALLBACK(on_folder_view_scroll_scrolled), file_browser );
  adjust = gtk_scrolled_window_get_hadjustment(
      GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll) );
  g_signal_connect( G_OBJECT(adjust), "value-changed",
                    G_CALLBACK(on_folder_view_scroll_scrolled), file_browser );
}

void ptk_file_browser_finalize( GObject *obj )
{
  PtkFileBrowser* file_browser = PTK_FILE_BROWSER(obj);
  if( file_browser->folder_content )
    folder_content_list_unref( file_browser->folder_content,
                               on_folder_content_update, obj );

  G_OBJECT_CLASS(parent_class)->finalize(obj);
}

void ptk_file_browser_get_property ( GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec )
{

}

void ptk_file_browser_set_property ( GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec )
{

}


/* File Browser API */

GtkWidget* ptk_file_browser_new( GtkWidget* mainWindow,
                                 PtkFileBrowserViewMode view_mode )
{
  PtkFileBrowser* file_browser;
  file_browser = (PtkFileBrowser*)g_object_new( PTK_TYPE_FILE_BROWSER, NULL );

  file_browser->folder_view = create_folder_view( file_browser, view_mode );
  file_browser->view_mode = view_mode;

  gtk_container_add (GTK_CONTAINER (file_browser->folder_view_scroll),
                     file_browser->folder_view);

  gtk_widget_show_all(file_browser->folder_view_scroll);
  return (GtkWidget*)file_browser;
}


guint initial_update_folder( PtkFileBrowser* file_browser )
{
  GtkTreePath *first_iter_path;
  if( G_UNLIKELY( ! gtk_tree_model_iter_n_children( file_browser->list_filter,
                                                    NULL ) ) ) {
    return FALSE;
  }

  first_iter_path = gtk_tree_path_new_from_indices(0, -1);
  if( G_LIKELY(first_iter_path) )
  {
    if( file_browser->view_mode == FBVM_ICON_VIEW ) {
      ptk_icon_view_scroll_to_path( PTK_ICON_VIEW(file_browser->folder_view),
                                    first_iter_path, TRUE, 0, 0 );
    }
    else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
      gtk_tree_view_scroll_to_cell ( GTK_TREE_VIEW(file_browser->folder_view),
                                    first_iter_path,
                                    NULL, TRUE, 0, 0 );
    }
    gtk_tree_path_free( first_iter_path );
  }
  gtk_widget_show( GTK_WIDGET(file_browser->folder_view) );
  return FALSE;
}

static gboolean side_pane_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path )
{
  if( file_browser->side_pane_mode == FB_SIDE_PANE_BOOKMARKS ) {
    ptk_location_view_chdir( file_browser->side_view, folder_path );
  }
  else if( file_browser->side_pane_mode == FB_SIDE_PANE_DIR_TREE ) {
    init_dir_tree( file_browser );
    return ptk_dir_tree_view_chdir( file_browser->side_view, folder_path );
  }
}


gboolean ptk_file_browser_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path,
                                 gboolean add_history )
{
  gboolean cancel = FALSE;
  GtkWidget* folder_view = file_browser->folder_view;
  GtkTreeView* dir_tree = file_browser->side_view;
  GtkEntry* addressBar;
  GtkStatusbar* statusBar;

  char* path_end;
  int ctxid = 0;
  char *cwd;
  char* upath;
  char* path;
  gboolean is_current;

  FolderContent* folder_content;

  /*
  * FIXME:
  * Do nothing when there is unfinished task running in the
  * working thread.
  * This should be fixed with a better way in the future.
  */
  if( file_browser->busy )
    return;

  /* Convert URI to file path */
  if( 0 == g_ascii_strncasecmp(folder_path, "file:", 5) ) {
    upath = g_filename_from_uri( folder_path, NULL, NULL );
  }
  else {
    upath = folder_path ? strdup(folder_path) : NULL;
  }

  if( ! upath )
    return FALSE;

  /* remove redundent '/' */
  if( strcmp( upath, "/" ) )
  {
    path_end = upath + strlen(upath) - 1;
    for( ; path_end > path; --path_end ){
      if( *path_end != '/' )
        break;
      else
        *path_end = '\0';
    }
  }

  path = g_filename_from_utf8( upath, -1, NULL, NULL, NULL );
  if( ! g_file_test( path, (G_FILE_TEST_IS_DIR) ) )
  {
    ptk_show_error( GTK_WINDOW(gtk_widget_get_toplevel(file_browser)),
                    _("Directory does'nt exist!") );
    g_free( path );
    g_free( upath );
    return FALSE;
  }
  else
    g_free( path );

  g_signal_emit( file_browser, signals[BEFORE_CHDIR_SIGNAL], 0, upath, &cancel );
  if( cancel ) {
    g_free( upath );
    return FALSE;
  }

  if( add_history ){
    if( ! file_browser->curHistory || strcmp( file_browser->curHistory->data, upath ) ){
      /* Has forward history */
      if( file_browser->curHistory && file_browser->curHistory->next )  {
        g_list_foreach ( file_browser->curHistory->next, (GFunc)g_free, NULL );
        g_list_free( file_browser->curHistory->next );
        file_browser->curHistory->next = NULL;
      }
      /* Add path to history if there is no forward history */
      file_browser->history = g_list_append( file_browser->history, upath );
      file_browser->curHistory = g_list_last( file_browser->history );
    }
  }

  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    ptk_icon_view_set_model( PTK_ICON_VIEW(folder_view), NULL );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    gtk_tree_view_set_model( GTK_TREE_VIEW(folder_view), NULL );
  }
  if( file_browser->folder_content ) {
    folder_content = file_browser->folder_content;
    file_browser->folder_content = NULL;
    file_browser->list_filter = NULL;
    file_browser->list_sorter = NULL;
    folder_content_list_unref( folder_content,
                               on_folder_content_update, file_browser );
  }
  file_browser->dir_name = strrchr( ptk_file_browser_get_cwd(file_browser), '/' );
  if( file_browser->dir_name[1] != '\0' )
    ++file_browser->dir_name;

  if( ! add_history )
    g_free( upath );

  gtk_widget_set_sensitive( GTK_WIDGET(file_browser), FALSE );
  file_browser->busy = TRUE;
  g_thread_create ( (GThreadFunc)ptk_file_browser_chdir_thread,
                     file_browser, FALSE, NULL );
  return TRUE;
}

static gboolean
ptk_file_browser_delayed_content_change( PtkFileBrowser* file_browser )
{
  GTimeVal t;
  g_get_current_time( &t );
  file_browser->prev_update_time = t.tv_sec;
  g_signal_emit( file_browser, signals[CONTENT_CHANGE_SIGNAL], 0 );
  file_browser->update_timeout = 0;
  return FALSE;
}

void on_folder_content_update ( FolderContent* content,
                                PtkFileBrowser* file_browser )
{
  /*  FIXME: Newly added or deleted files should not be delayed.
      This must be fixed before 0.2.0 release.  */
  GTimeVal t;
  g_get_current_time( &t );
  /*
    Previous update is < 5 seconds before.
    Queue the update, and don't update the view too often
  */
  if( (t.tv_sec - file_browser->prev_update_time) < 5 ) {
    /*
      If the update timeout has been set, wait until the timeout happens, and
      don't do anything here.
    */
    if( 0 == file_browser->update_timeout ) { /* No timeout callback. Add one */
      /* Delay the update */
      file_browser->update_timeout = g_timeout_add( 5000,
          (GSourceFunc)ptk_file_browser_delayed_content_change, file_browser );
    }
  }
  else if( 0 == file_browser->update_timeout ) { /* No timeout callback. Add one */
    file_browser->prev_update_time = t.tv_sec;
    g_signal_emit( file_browser, signals[CONTENT_CHANGE_SIGNAL], 0 );
  }
}

gpointer ptk_file_browser_chdir_thread( PtkFileBrowser* file_browser )
{
  char* upath = ptk_file_browser_get_cwd(file_browser);
  char* path;

  path = g_filename_from_utf8( upath, -1, NULL, NULL, NULL );
  file_browser->folder_content = folder_content_list_get( path,
                              (FolderContentUpdateFunc)on_folder_content_update,
                              file_browser );
  g_free( path );

  g_idle_add( (GSourceFunc)ptk_file_browser_chdir_complete, file_browser );
}

gboolean ptk_file_browser_chdir_complete( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  gboolean is_current;
  GtkTreeIterCompareFunc sort_funcs[] = { &sort_files_by_name,
                                          &sort_files_by_size,
                                          &sort_files_by_time };
  int sort_col;

  /* Set up a sorter to folder content model */
  if( file_browser->list_sorter )
    g_object_unref( G_OBJECT(file_browser->list_sorter) );

  file_browser->list_sorter = ptk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(file_browser->folder_content->list ));

  sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);
  gtk_tree_sortable_set_sort_func( sortable, COL_FILE_NAME,
                                   sort_funcs[ file_browser->sort_order ], file_browser,
                                   NULL );

  if(file_browser->sort_order == FB_SORT_BY_NAME) {
    sort_col = COL_FILE_NAME;
  }
  else {
    COL_FILE_STAT;
  }
  gtk_tree_sortable_set_sort_column_id( sortable,
                                        sort_col,
                                        file_browser->sort_type );

  /* Set up a filter to folder content model */
  if( file_browser->list_filter )
    g_object_unref( G_OBJECT(file_browser->list_filter) );

  file_browser->list_filter = gtk_tree_model_filter_new(
      file_browser->list_sorter, NULL );
  gtk_tree_model_filter_set_visible_func(
      GTK_TREE_MODEL_FILTER(file_browser->list_filter),
      filter_files, file_browser, NULL );

  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    ptk_icon_view_set_model( PTK_ICON_VIEW(file_browser->folder_view),
                             GTK_TREE_MODEL( file_browser->list_filter ) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    gtk_tree_view_set_model( GTK_TREE_VIEW(file_browser->folder_view),
                             GTK_TREE_MODEL( file_browser->list_filter ) );
  }

  /* If side pane is displayed */
  if( file_browser->show_side_pane ){
    side_pane_chdir( file_browser, ptk_file_browser_get_cwd(file_browser) );
  }

  gtk_widget_set_sensitive( file_browser, TRUE );

  g_idle_add( (GSourceFunc)initial_update_folder, file_browser );

  file_browser->n_sel_files = 0;
  file_browser->sel_size = 0;

  file_browser->busy = FALSE;

  g_signal_emit( file_browser, signals[AFTER_CHDIR_SIGNAL], NULL );
  g_signal_emit( file_browser, signals[CONTENT_CHANGE_SIGNAL], NULL );
  g_signal_emit( file_browser, signals[SEL_CHANGE_SIGNAL], NULL );

  return FALSE;
}

const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser )
{
  if( ! file_browser->curHistory )
    return NULL;
  return (const char*)file_browser->curHistory->data;
}

gboolean ptk_file_browser_is_busy( PtkFileBrowser* file_browser )
{
  return file_browser->busy;
}

gboolean ptk_file_browser_can_back( PtkFileBrowser* file_browser )
{
  /* there is back history */
  return ( file_browser->curHistory && file_browser->curHistory->prev );
}

void ptk_file_browser_go_back( PtkFileBrowser* file_browser )
{
  const char* path;

  /*
  * FIXME:
  * Do nothing when there is unfinished task running in the
  * working thread.
  * This should be fixed with a better way in the future.
  */
  if( file_browser->busy )
    return;

  /* there is no back history */
  if ( ! file_browser->curHistory || ! file_browser->curHistory->prev )
    return;
  path = (const char*)file_browser->curHistory->prev->data;
  if( ptk_file_browser_chdir( file_browser, path, FALSE ) ) {
    file_browser->curHistory = file_browser->curHistory->prev;
  }
}

gboolean ptk_file_browser_can_forward( PtkFileBrowser* file_browser )
{
  /* If there is forward history */
  return ( file_browser->curHistory && file_browser->curHistory->next );
}

void ptk_file_browser_go_forward( PtkFileBrowser* file_browser )
{
  const char* path;

  /*
  * FIXME:
  * Do nothing when there is unfinished task running in the
  * working thread.
  * This should be fixed with a better way in the future.
    */
  if( file_browser->busy )
    return;

  /* If there is no forward history */
  if ( ! file_browser->curHistory || ! file_browser->curHistory->next )
    return;
  path = (const char*)file_browser->curHistory->next->data;
  if( ptk_file_browser_chdir( file_browser, path, FALSE ) ) {
    file_browser->curHistory = file_browser->curHistory->next;
  }
}

GtkWidget* ptk_file_browser_get_folder_view( PtkFileBrowser* file_browser )
{
  return file_browser->folder_view;
}

GtkTreeView* ptk_file_browser_get_dir_tree( PtkFileBrowser* file_browser )
{

}

void ptk_file_browser_select_all( PtkFileBrowser* file_browser )
{
  GtkTreeSelection* tree_sel;
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    ptk_icon_view_select_all( PTK_ICON_VIEW(file_browser->folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(file_browser->folder_view) );
    gtk_tree_selection_select_all( tree_sel );
  }
}

static gboolean
invert_selection                    (GtkTreeModel* model,
                                     GtkTreePath *path,
                                     GtkTreeIter* it,
                                     PtkFileBrowser* file_browser )
{
  GtkTreeSelection* tree_sel;
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    if( ptk_icon_view_path_is_selected ( PTK_ICON_VIEW(file_browser->folder_view), path ) )
      ptk_icon_view_unselect_path ( PTK_ICON_VIEW(file_browser->folder_view), path );
    else
      ptk_icon_view_select_path ( PTK_ICON_VIEW(file_browser->folder_view), path );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(file_browser->folder_view) );
    if( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
      gtk_tree_selection_unselect_path ( tree_sel, path );
    else
      gtk_tree_selection_select_path ( tree_sel, path );
  }
  return FALSE;
}

void ptk_file_browser_invert_selection( PtkFileBrowser* file_browser )
{
  GtkTreeModel* model;
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    model = ptk_icon_view_get_model( PTK_ICON_VIEW(file_browser->folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    model = gtk_tree_view_get_model( GTK_TREE_VIEW(file_browser->folder_view) );
  }
  gtk_tree_model_foreach ( model,
                           (GtkTreeModelForeachFunc)invert_selection, file_browser );
}

/* signal handlers */

static gboolean item_activated( PtkFileBrowser* file_browser )
{
  ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
  return FALSE;
}

void
on_folder_view_item_activated           (PtkIconView     *iconview,
                                         GtkTreePath     *path,
                                         PtkFileBrowser* file_browser)
{
  g_idle_add( (GSourceFunc)item_activated, file_browser );
}

void
on_folder_view_row_activated           (GtkTreeView     *tree_view,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn* col,
                                        PtkFileBrowser* file_browser)
{
  g_idle_add( (GSourceFunc)item_activated, file_browser );
}

static gboolean on_folder_view_update_sel( PtkFileBrowser* file_browser )
{
  GList* sel_files;
  GList* sel;
  GtkTreeIter it;
  GtkTreeSelection* tree_sel;
  GtkTreeModel* model;
  struct stat* pstat;

  file_browser->n_sel_files = 0;
  file_browser->sel_size = 0;

  sel_files = folder_view_get_selected_items( file_browser, &model );

  for( sel = sel_files; sel; sel = g_list_next( sel ) )
  {
    if( gtk_tree_model_get_iter( model, &it, (GtkTreePath*)sel->data ) )
    {
      gtk_tree_model_get( model, &it, COL_FILE_STAT, &pstat, -1 );
      if( pstat ){
        file_browser->sel_size += pstat->st_size;
      }
      ++file_browser->n_sel_files;
    }
  }

  g_list_foreach( sel_files,
                  (GFunc)gtk_tree_path_free,
                  NULL );
  g_list_free( sel_files );

  g_signal_emit( file_browser, signals[SEL_CHANGE_SIGNAL], NULL );
  return FALSE;
}

/*
* I don't know why, but calculate selected items in this handler will
* cause crash. So I delay the operation and put it in an idle handler.
*/
void on_folder_view_item_sel_change (PtkIconView *iconview,
                                     PtkFileBrowser* file_browser)
{
  g_idle_add( (GSourceFunc)on_folder_view_update_sel, file_browser );
}


gboolean
on_folder_view_key_press_event          (GtkWidget       *widget,
                                         GdkEventKey     *event,
                                         PtkFileBrowser* file_browser)
{

  return FALSE;
}



gboolean
on_folder_view_button_release_event       (GtkWidget       *widget,
                                          GdkEventButton  *event,
                                          PtkFileBrowser* file_browser)
{
  GtkTreeSelection* tree_sel;
  GtkTreePath* tree_path;
  if( file_browser->view_mode == FBVM_LIST_VIEW && !can_folder_view_sel_change ) {
    can_folder_view_sel_change = TRUE;
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) );
    gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(widget),
                                   event->x, event->y, &tree_path, NULL, NULL, NULL);
    if( tree_path ) {
      if( gtk_tree_selection_path_is_selected( tree_sel, tree_path ) ) {
        gtk_tree_selection_unselect_all( tree_sel );
        gtk_tree_selection_select_path( tree_sel, tree_path );
      }
      gtk_tree_path_free( tree_path );
    }
  }
  return FALSE;
}

gboolean
on_folder_view_button_press_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        PtkFileBrowser* file_browser)
{
  GtkTreeModel* model;
  GtkTreePath *tree_path;
  GtkTreeIter it;
  gchar *file_name;
  const gchar *mime_type;
  gchar* ufile_path;
  gchar* file_path;
  struct stat *file_stat;
  GtkWidget* popup = NULL;
  GtkTreeSelection* tree_sel;

  if( event->type == GDK_BUTTON_PRESS )
  {
    if( file_browser->view_mode == FBVM_ICON_VIEW )
    {
      tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW(widget),
                                                event->x, event->y);
      if( !tree_path && event->button == 3 ){
        ptk_icon_view_unselect_all ( PTK_ICON_VIEW(widget) );
      }
      model = ptk_icon_view_get_model( PTK_ICON_VIEW(widget));
    }
    else if( file_browser->view_mode == FBVM_LIST_VIEW )
    {
      model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget));
      gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(widget),
                   event->x, event->y, &tree_path, NULL, NULL, NULL);
      tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(widget) );

      /* If click on a selected row */
      if( tree_path &&
          event->type == GDK_BUTTON_PRESS &&
          !(event->state & (GDK_SHIFT_MASK|GDK_CONTROL_MASK)) &&
          gtk_tree_selection_path_is_selected (tree_sel, tree_path) )  {
        can_folder_view_sel_change = FALSE;
      }
      else
        can_folder_view_sel_change = TRUE;
    }

    if( tree_path ) /* an item is selected */
    {
      /* get selected file */
      if( gtk_tree_model_get_iter( model, &it, tree_path ) )
      {
        gtk_tree_model_get( model, &it,
                            COL_FILE_NAME, &file_name,
                            COL_FILE_STAT, &file_stat,
                            COL_FILE_TYPE, &mime_type, -1 );
        ufile_path = g_build_filename( ptk_file_browser_get_cwd(file_browser),
                                      file_name, NULL );
        g_free( file_name );
        if( ufile_path ) {
          file_path = g_filename_from_utf8( ufile_path, -1, NULL, NULL, NULL );
        }
      }
    }
    else /* no item is selected */
    {
      file_path = NULL;
      ufile_path = NULL;
      mime_type = NULL;
    }

      /* middle button */
    if( event->button == 2 && file_path ) /* middle click on a item */
    {
      if( G_LIKELY(file_path) ) {
        if( g_file_test(file_path, G_FILE_TEST_IS_DIR) ) {
          g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0,
                          ufile_path, PTK_OPEN_NEW_TAB );
        }
      }
    }
    else if( event->button == 3 ) /* right click */
    {
      /* cancel all selection, and select the item if it's not selected */
      if( file_browser->view_mode == FBVM_ICON_VIEW ) {
        if( tree_path &&
            !ptk_icon_view_path_is_selected (PTK_ICON_VIEW(widget),
                                            tree_path) )
        {
          ptk_icon_view_unselect_all ( PTK_ICON_VIEW(widget) );
          ptk_icon_view_select_path( PTK_ICON_VIEW(widget), tree_path );
        }
      }

      if( ! file_path ) { /* No file is selected */
        mime_type = XDG_MIME_TYPE_DIRECTORY; /* current dir */
        file_path = g_filename_from_utf8( ptk_file_browser_get_cwd(file_browser),
                                          -1, NULL, NULL, NULL );
      }
      popup = ptk_file_browser_get_popup_menu_for_file(file_browser,
                                                       file_path, mime_type);
      gtk_menu_popup( GTK_MENU(popup), NULL, NULL,
                      NULL, NULL, 3, event->time );
    }

    if( file_path )
      g_free( file_path );
    if( ufile_path )
      g_free( ufile_path );
    /*
    g_free( mime_type ); is not needed, since COL_FILE_TYPE
    is stored with "G_POINTER" type, not G_STRING
    */

    gtk_tree_path_free( tree_path );
  }
  return FALSE;
}

void
on_folder_view_scroll_scrolled       (GtkAdjustment *adjust,
                                    PtkFileBrowser* file_browser)
{
  update_folder_view_visible_region(file_browser);
  /*
  * Because of some strange behavior of gtk+, I have to install an idle
  * handler here to ensure all visible items are really upadted.
  * FIXME: This is inefficient and should be improved in the future.
  */
  g_idle_add( (GSourceFunc)update_folder_view_visible_region, file_browser );
}

void
on_folder_view_size_allocate            (GtkWidget       *widget,
                                        GdkRectangle    *allocation,
                                        PtkFileBrowser* file_browser)
{
  GtkTreeModel* list;
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    list = ptk_icon_view_get_model( PTK_ICON_VIEW(file_browser->folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    list = gtk_tree_view_get_model( GTK_TREE_VIEW(file_browser->folder_view) );
  }
  if( list )    /* If the folder has been loaded, update visible region */
    g_idle_add( (GSourceFunc)update_folder_view_visible_region, file_browser );
}

static gboolean on_dir_tree_update_sel ( PtkFileBrowser* file_browser )
{
  char* dir_path;
  dir_path = ptk_dir_tree_view_get_selected_dir(file_browser->side_view);
  if( dir_path ){
    if( strcmp( dir_path, ptk_file_browser_get_cwd(file_browser) ) ) {
      ptk_file_browser_chdir( file_browser, dir_path, TRUE );
    }
    g_free( dir_path );
  }
  return FALSE;
}

void
on_dir_tree_sel_changed           (GtkTreeSelection *treesel,
                                   PtkFileBrowser* file_browser)
{
  g_idle_add( (GSourceFunc)on_dir_tree_update_sel, file_browser );
}

void
on_location_view_row_activated           (GtkTreeView *tree_view,
                                          GtkTreePath *path,
                                          GtkTreeViewColumn *column,
                                          PtkFileBrowser* file_browser )
{
  char* dir_path;
  dir_path = ptk_location_view_get_selected_dir(file_browser->side_view);
  if( dir_path ){
    if( strcmp( dir_path, ptk_file_browser_get_cwd(file_browser) ) ) {
      ptk_file_browser_chdir( file_browser, dir_path, TRUE );
    }
    g_free( dir_path );
  }
}

static void get_file_perm_string( char* perm, mode_t mode )
{
  perm[0] = S_ISDIR( mode ) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
  perm[1] = (mode & S_IRUSR) ? 'r' : '-';
  perm[2] = (mode & S_IWUSR) ? 'w' : '-';
  perm[3] = (mode & S_IXUSR) ? 'x' : '-';
  perm[4] = (mode & S_IRGRP) ? 'r' : '-';
  perm[5] = (mode & S_IWGRP) ? 'w' : '-';
  perm[6] = (mode & S_IXGRP) ? 'x' : '-';
  perm[7] = (mode & S_IROTH) ? 'r' : '-';
  perm[8] = (mode & S_IWOTH) ? 'w' : '-';
  perm[9] = (mode & S_IXOTH) ? 'x' : '-';
  perm[10] = '\0';
}

gboolean update_folder_view_visible_region( PtkFileBrowser* file_browser )
{
  GtkWidget* folder_view = file_browser->folder_view;
  GtkTreeModel* model;
  GtkTreeModelSort* model_sorter;
  GtkListStore* list;
  GtkTreePath *start_path, *end_path;
  GtkTreeIter it;
  GtkTreeIter it2;
  GtkTreePath* real_path;
  GtkTreePath* sorter_path;
  char* cur_dir_path = NULL;
  char* ufile_name;
  char* file_name;
  const char* mime = NULL;
  char* tmp_path;
  char* file_path;
  char* psize;
  char size[32];
  char* owner;
  char perm[16];
  char time[40];
  const char* desc = NULL;

  struct stat* pstat;
  GdkPixbuf* mime_icon;
  int update_count = 0;
  static struct passwd* puser = NULL;
  static struct group* pgroup = NULL;
  static uid_t cached_uid = -1;
  static gid_t cached_gid = -1;
  static char uid_str_buf[32];
  static char* user_name;
  static char gid_str_buf[32];
  static char* group_name;

  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    if( !ptk_icon_view_get_visible_range ( PTK_ICON_VIEW(folder_view),
                                           &start_path, &end_path ) )
        return;
    model = ptk_icon_view_get_model( PTK_ICON_VIEW(folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    if( !gtk_tree_view_get_visible_range ( GTK_TREE_VIEW(folder_view),
                                           &start_path, &end_path ) )
      return;
    model = gtk_tree_view_get_model( GTK_TREE_VIEW(folder_view) );
  }

  /*
    NOTE:It seems that this is a bug of gtk+ 2.8.
    gtk_tree_view_get_visible_range sometimes returns invalid paths.
    This shouldn't happen according to the document.
    So, bug report to gtk+ team is needed.
    Before this problem can be fixed, we do some check ourselves.
  */
  if( (!end_path || gtk_tree_path_get_depth (end_path) <= 0)
      || (!start_path || gtk_tree_path_get_depth (start_path) <= 0) )  {
    if( start_path )
      gtk_tree_path_free( start_path );
    if( end_path )
      gtk_tree_path_free( end_path );
    return;
  }

  model_sorter = PTK_TREE_MODEL_SORT( gtk_tree_model_filter_get_model(
                                            GTK_TREE_MODEL_FILTER(model) ) );
  list = GTK_LIST_STORE( ptk_tree_model_sort_get_model(model_sorter) );

  /* Convert current dir path to on-disk encoding */
  cur_dir_path = g_filename_from_utf8( ptk_file_browser_get_cwd(file_browser),
                                        -1, NULL, NULL, NULL );
  if( ! cur_dir_path )
    return;

  for( ; gtk_tree_path_compare( start_path, end_path ) <= 0;
         gtk_tree_path_next ( start_path ) )
  {
    sorter_path = gtk_tree_model_filter_convert_path_to_child_path (
                            GTK_TREE_MODEL_FILTER(model), start_path );
    if( !sorter_path ) {
      /* g_warning( "invalid sorter_path\n" ); */
      continue;
    }
    real_path = ptk_tree_model_sort_convert_path_to_child_path (
                            model_sorter, sorter_path );
    if( !real_path ) {
      /* g_warning( "invalid real_path\n" ); */
      continue;
    }
    g_free( sorter_path );

    if( gtk_tree_model_get_iter ( list, &it, real_path ) )
    {
      gtk_tree_model_get ( list, &it,
                            COL_FILE_NAME, &ufile_name,
                            COL_FILE_TYPE, &mime,
                            COL_FILE_STAT, &pstat,
                            COL_FILE_SIZE, &psize, -1 );

      if( !mime )
      {
        /*
        Variable mime always points to a ``constant string'',
        so no string copy is needed. The string ptrs are always valid.
        */

        file_path = NULL;
        if( S_ISDIR(pstat->st_mode) ) {
          mime = XDG_MIME_TYPE_DIRECTORY;
        }
        else if( G_LIKELY( ufile_name ) )
        {
          file_name = g_filename_from_utf8( ufile_name, -1, NULL, NULL, NULL );
          g_free( ufile_name );
          if( G_LIKELY(file_name) )
          {
            file_path = g_build_filename( cur_dir_path,
                                          file_name, NULL );
            if( G_LIKELY(file_path) )
            {
              if( G_UNLIKELY( S_ISLNK( pstat->st_mode ) ) )
              {
                tmp_path = file_path;
                file_path = g_file_read_link( file_path, NULL );
                g_free( tmp_path );
                /* This is relative path */
                if( file_path && file_path[0] != '/' )
                {
                  tmp_path = file_path;
                  file_path = g_build_filename( cur_dir_path, file_path, NULL );
                  g_free( tmp_path );
                }
              }
              mime = xdg_mime_get_mime_type_for_file( file_path, file_name, NULL);
            }
            else{
              file_path = NULL;
            }
            g_free( file_name );
          }
        }
        if( G_UNLIKELY(!mime) )
          mime = XDG_MIME_TYPE_UNKNOWN;

        gtk_list_store_set ( list, &it,
                             COL_FILE_TYPE, mime, -1 );

        mime_icon = NULL;

        /*
        * Thumbnail support.
        * FIXME: This should be optional, and shouldn't rely on appSettings.
        */
        if( G_UNLIKELY( appSettings.showThumbnail &&
                        mime != XDG_MIME_TYPE_UNKNOWN &&
                        0 == strncmp(mime, "image/", 6) &&
                        pstat->st_size < appSettings.maxThumbSize) )
        {
          if( file_path ) {
            mime_icon = gdk_pixbuf_new_from_file_at_scale ( file_path, 32, 32,
                                                            TRUE, NULL );
          }
        }

        if( G_LIKELY( file_path ) )
          g_free( file_path );

        if( G_LIKELY( !mime_icon ) )
        {
          mime_icon = get_mime_icon( mime );
          /* If there is no icon */
          if( G_UNLIKELY( !mime_icon ) )
          {
            if( 0 == strncmp( mime, "text/", 5 ) ) {
              mime_icon = get_mime_icon( XDG_MIME_TYPE_PLAIN_TEXT );
            }
            else {
              mime_icon = get_mime_icon( XDG_MIME_TYPE_UNKNOWN );
            }
          }
        }

        gtk_list_store_set ( list, &it,
                             COL_FILE_ICON, mime_icon, -1 );

      }

      if( !psize )
      {
        if( file_browser->view_mode == FBVM_LIST_VIEW )
        {
          file_size_to_string( size, pstat->st_size );
          /* user:group */
          if( G_UNLIKELY(pstat->st_uid != cached_uid) ) {
            cached_uid = pstat->st_uid;
            puser = getpwuid( cached_uid );
            if( puser && puser->pw_name && *puser->pw_name )
              user_name = puser->pw_name;
            else {
              sprintf( uid_str_buf, "%d", cached_uid );
              user_name = uid_str_buf;
            }
          }
          if( G_UNLIKELY(pstat->st_gid != cached_gid) ) {
            cached_gid = pstat->st_gid;
            pgroup = getgrgid( cached_gid );
            if( pgroup && pgroup->gr_name && *pgroup->gr_name )
              group_name = pgroup->gr_name;
            else {
              sprintf( gid_str_buf, "%d", cached_gid );
              group_name = gid_str_buf;
            }
          }
          owner = g_strdup_printf ( "%s:%s", user_name, group_name );
          get_file_perm_string( perm, pstat->st_mode );
          strftime( time, sizeof(time),
                    "%Y-%m-%d %H:%M", localtime( &pstat->st_mtime ) );

          if( G_UNLIKELY( S_ISLNK( pstat->st_mode ) ) )
            desc = get_mime_description( "inode/symlink" );
          else
            desc = get_mime_description( mime );

          gtk_list_store_set ( list, &it,
                                COL_FILE_SIZE, size,
                                COL_FILE_PERM, perm,
                                COL_FILE_DESC, desc,
                                COL_FILE_OWNER, owner,
                                COL_FILE_MTIME, time, -1 );
          g_free( owner );
        }
      }
    }

    g_free( real_path );
  }
  g_free( cur_dir_path );

  gtk_tree_path_free( start_path );
  gtk_tree_path_free( end_path );
  return FALSE;
}

void ptk_file_browser_update_mime_icons( PtkFileBrowser* file_browser )
{
  update_folder_view_visible_region( file_browser );
}

static gint sort_files_by_name  (GtkTreeModel *model,
                           GtkTreeIter *a,
                           GtkTreeIter *b,
                           gpointer user_data)
{
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  struct stat *stat_a, *stat_b;
  gchar* name_a, *name_b;
  gint ret;
  gtk_tree_model_get( model, a, COL_FILE_NAME, &name_a,
                               COL_FILE_STAT, &stat_a, -1 );

  if( !name_a )
    return 0;
  gtk_tree_model_get( model, b, COL_FILE_NAME, &name_b,
                               COL_FILE_STAT, &stat_b, -1 );
  if( !name_b ){
    g_free( name_a );
    return 0;
  }
  ret = S_ISDIR(stat_a->st_mode) - S_ISDIR(stat_b->st_mode);
  if( ret )
    return file_browser->sort_type == GTK_SORT_ASCENDING ? -ret : ret;
  ret = g_ascii_strcasecmp( name_a , name_b );
  g_free( name_a );
  g_free( name_b );
  return ret;
}

static gint sort_files_by_size  (GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data)
{
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  struct stat *stat_a, *stat_b;
  gint ret;
  gtk_tree_model_get( model, a, COL_FILE_STAT, &stat_a, -1 );
  gtk_tree_model_get( model, b, COL_FILE_STAT, &stat_b, -1 );
  ret = S_ISDIR(stat_a->st_mode) - S_ISDIR(stat_b->st_mode);
  if( ret )
    return file_browser->sort_type == GTK_SORT_ASCENDING ? -ret : ret;
  ret = (stat_a->st_size - stat_b->st_size);
  return ret ? ret : sort_files_by_name( model, a, b, user_data );
}

static gint sort_files_by_time  (GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data)
{
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  struct stat *stat_a, *stat_b;
  gint ret;
  gtk_tree_model_get( model, a, COL_FILE_STAT, &stat_a, -1 );
  gtk_tree_model_get( model, b, COL_FILE_STAT, &stat_b, -1 );
  ret = S_ISDIR(stat_a->st_mode) - S_ISDIR(stat_b->st_mode);
  if( ret )
    return file_browser->sort_type == GTK_SORT_ASCENDING ? -ret : ret;
  ret = (stat_a->st_mtime - stat_b->st_mtime);
  return ret ? ret : sort_files_by_name( model, a, b, user_data );
}


gboolean filter_files  (GtkTreeModel *model,
                        GtkTreeIter *it,
                        gpointer user_data)
{
  gboolean ret;
  gchar* name;
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;

  if ( file_browser && file_browser->show_hidden_files )
    return TRUE;

  gtk_tree_model_get( model, it, COL_FILE_NAME, &name, -1 );

  if( name && name[0] == '.' )
    ret = FALSE;
  else
    ret = TRUE;
  g_free( name );
  return ret;
}


static gint sort_dir_tree_by_name  (GtkTreeModel *model,
                                    GtkTreeIter *a,
                                    GtkTreeIter *b,
                                    gpointer user_data)
{
  gchar* name_a, *name_b;
  gint ret;
  gtk_tree_model_get( model, a, COL_DIRTREE_TEXT, &name_a, -1 );
  if( !name_a )
    return -1;
  gtk_tree_model_get( model, b, COL_DIRTREE_TEXT, &name_b, -1 );
  if( !name_b ) {
    g_free( name_a );
    return 1;
  }
  ret = g_ascii_strcasecmp( name_a, name_b );
  g_free( name_a );
  g_free( name_b );
  return ret;
}

void init_dir_tree( PtkFileBrowser* file_browser )
{
  static GtkTreeStore* tree = NULL;
  GtkTreeView* dir_treeView = file_browser->side_view;
  GtkTreeSortable* sortable;
  GtkTreeModelFilter* filter;
  GtkTreeIter* parent = NULL;
  GtkTreeIter it, subit;
  GtkTreePath* iter_path;
  gchar* file_name;
  struct stat file_stat;

  GdkPixbuf* folder_icon = get_folder_icon20();

  if( gtk_tree_view_get_model( dir_treeView ) )
    return;

  if( !tree ){
   tree = gtk_tree_store_new( NUM_COL_DIRTREE, GDK_TYPE_PIXBUF, G_TYPE_STRING );
    gtk_tree_store_append ( tree, &it, parent );
    gtk_tree_store_set ( tree, &it, COL_DIRTREE_ICON, folder_icon,
                                            COL_DIRTREE_TEXT, "/", -1 );
    gtk_tree_store_append ( tree, &subit, &it );
  }
  sortable = GTK_TREE_SORTABLE(tree);
  gtk_tree_sortable_set_sort_func( sortable, COL_DIRTREE_TEXT,
                                   sort_dir_tree_by_name, NULL, NULL );

  gtk_tree_sortable_set_sort_column_id( sortable,
                                        COL_DIRTREE_TEXT,
                                        file_browser->sort_type );

  filter = gtk_tree_model_filter_new( tree, NULL );
  gtk_tree_model_filter_set_visible_func( filter, filter_files, file_browser, NULL );

  gtk_tree_view_set_model( dir_treeView, GTK_TREE_MODEL( filter ) );
  file_browser->tree_filter = filter;

  iter_path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_view_expand_row ( dir_treeView, iter_path, FALSE );
  gtk_tree_path_free( iter_path );
}

static gboolean can_sel_change ( GtkTreeSelection *selection,
                                 GtkTreeModel *model,
                                 GtkTreePath *path,
                                 gboolean path_currently_selected,
                                 gpointer data)
{
  return can_folder_view_sel_change;
}


static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFileBrowserViewMode view_mode )
{
  GtkWidget* folder_view;
  GtkTreeSelection* tree_sel;
  GtkCellRenderer* renderer;
  switch( view_mode )
  {
    case FBVM_ICON_VIEW:
      folder_view = ptk_icon_view_new ();
      ptk_icon_view_set_selection_mode ( PTK_ICON_VIEW(folder_view),
                                         GTK_SELECTION_MULTIPLE);

      ptk_icon_view_set_pixbuf_column ( PTK_ICON_VIEW(folder_view), COL_FILE_ICON );
      ptk_icon_view_set_text_column ( PTK_ICON_VIEW(folder_view), COL_FILE_NAME );

      ptk_icon_view_set_column_spacing( PTK_ICON_VIEW(folder_view), 4 );
      ptk_icon_view_set_item_width ( PTK_ICON_VIEW(folder_view), 108 );

      ptk_icon_view_set_enable_search( PTK_ICON_VIEW(folder_view), TRUE );
      ptk_icon_view_set_search_column( PTK_ICON_VIEW(folder_view), COL_FILE_NAME );

      gtk_cell_layout_clear (GTK_CELL_LAYOUT (folder_view));

      /* renderer = gtk_cell_renderer_pixbuf_new (); */
      renderer = ptk_file_icon_renderer_new();
      /* add the icon renderer */
      g_object_set (G_OBJECT (renderer),
                    "follow_state", TRUE,
                    NULL );
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (folder_view), renderer, FALSE);
      gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (folder_view), renderer,
                                     "pixbuf", COL_FILE_ICON);
      gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (folder_view), renderer,
                                     "stat", COL_FILE_STAT);
      /* add the name renderer */
      renderer = gtk_cell_renderer_text_new ();
      g_object_set (G_OBJECT (renderer),
                    "wrap-mode", PANGO_WRAP_CHAR,
                    "wrap-width", 108,
                    "xalign", 0.5,
                    "yalign", 0.0,
                    NULL);
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (folder_view), renderer, TRUE);
      gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (folder_view), renderer,
                                     "text", COL_FILE_NAME);

      ptk_icon_view_enable_model_drag_source (
          PTK_ICON_VIEW(folder_view),
          (GDK_CONTROL_MASK|GDK_BUTTON1_MASK|GDK_BUTTON3_MASK),
          drag_targets,
          sizeof(drag_targets)/sizeof(GtkTargetEntry),
          GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

      ptk_icon_view_enable_model_drag_dest (
          PTK_ICON_VIEW(folder_view),
          drag_targets,
          sizeof(drag_targets)/sizeof(GtkTargetEntry),
          GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

      g_signal_connect ((gpointer) folder_view,
                         "item_activated",
                         G_CALLBACK (on_folder_view_item_activated),
                         file_browser );

      g_signal_connect ((gpointer) folder_view,
                         "key_press_event",
                         G_CALLBACK (on_folder_view_key_press_event),
                         file_browser);
      g_signal_connect_after ((gpointer) folder_view,
                         "selection-changed",
                         G_CALLBACK (on_folder_view_item_sel_change),
                         file_browser );

      break;
    case FBVM_LIST_VIEW:
      folder_view = gtk_tree_view_new ();
      init_list_view( GTK_TREE_VIEW(folder_view) );
      tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(folder_view) );
      gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE);

      gtk_tree_view_enable_model_drag_source (
          GTK_TREE_VIEW(folder_view),
          (GDK_CONTROL_MASK|GDK_BUTTON1_MASK|GDK_BUTTON3_MASK),
          drag_targets,
          sizeof(drag_targets)/sizeof(GtkTargetEntry),
          GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

      gtk_tree_view_enable_model_drag_dest (
          GTK_TREE_VIEW(folder_view),
          drag_targets,
          sizeof(drag_targets)/sizeof(GtkTargetEntry),
          GDK_ACTION_DEFAULT|GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK);

      g_signal_connect ((gpointer) folder_view,
                        "row_activated",
                        G_CALLBACK (on_folder_view_row_activated),
                        file_browser );

      g_signal_connect_after ((gpointer) tree_sel,
                        "changed",
                        G_CALLBACK (on_folder_view_item_sel_change),
                        file_browser );

      gtk_tree_selection_set_select_function( tree_sel,
                                              can_sel_change, file_browser, NULL );
      break;
  }

  g_signal_connect ((gpointer) folder_view,
                     "button-press-event",
                     G_CALLBACK (on_folder_view_button_press_event),
                     file_browser);
  g_signal_connect ((gpointer) folder_view,
                     "button-release-event",
                     G_CALLBACK (on_folder_view_button_release_event),
                     file_browser);
  g_signal_connect ((gpointer) folder_view,
                     "size_allocate",
                     G_CALLBACK (on_folder_view_size_allocate),
                     file_browser);

  /* init drag & drop support */

  g_signal_connect ((gpointer) folder_view, "drag-data-received",
                     G_CALLBACK (on_folder_view_drag_data_received),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-data-get",
                     G_CALLBACK (on_folder_view_drag_data_get),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-begin",
                     G_CALLBACK (on_folder_view_drag_begin),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-motion",
                     G_CALLBACK (on_folder_view_drag_motion),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-leave",
                     G_CALLBACK (on_folder_view_drag_leave),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-drop",
                     G_CALLBACK (on_folder_view_drag_drop),
                     file_browser );

  g_signal_connect ((gpointer) folder_view, "drag-end",
                     G_CALLBACK (on_folder_view_drag_end),
                     file_browser );

  return folder_view;
}

static void init_list_view( GtkTreeView* list_view )
{
  GtkTreeViewColumn* col;
  GtkCellRenderer *renderer;
  GtkCellRenderer *pix_renderer;

  int cols[] = { COL_FILE_NAME, COL_FILE_SIZE, COL_FILE_DESC,
                 COL_FILE_PERM, COL_FILE_OWNER, COL_FILE_MTIME };
  int i;

  const char* titles[] = { N_("Name"), N_("Size"), N_("Type"),
            N_("Permission"), N_("Owner:Group"), N_("Last Modification") };

  for( i = 0; i < G_N_ELEMENTS(cols); ++i ) {
    col = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_resizable (col, TRUE);

    renderer = gtk_cell_renderer_text_new();

    if( i == 0 ) { /* First column */
      gtk_object_set( GTK_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL );

      /* pix_renderer = gtk_cell_renderer_pixbuf_new(); */
      pix_renderer = ptk_file_icon_renderer_new();

      gtk_tree_view_column_pack_start( col, pix_renderer, FALSE );
      gtk_tree_view_column_set_attributes( col, pix_renderer,
                                           "pixbuf", COL_FILE_ICON,
                                           "stat", COL_FILE_STAT, NULL );
      gtk_tree_view_column_set_expand (col, TRUE);
      gtk_tree_view_column_set_min_width(col, 200);
    }

    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer, "text", cols[i], NULL );
    gtk_tree_view_append_column ( list_view, col );
    gtk_tree_view_column_set_title( col, _( titles[i] ) );
  }

  col = gtk_tree_view_get_column( list_view, 2 );
  gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
  gtk_tree_view_column_set_fixed_width (col, 120);

  /*
  col = gtk_tree_view_get_column( list_view, 0 );
  gtk_tree_view_column_set_clickable (col, TRUE);
  gtk_tree_view_column_set_sort_column_id (col, COL_FILE_NAME);

  col = gtk_tree_view_get_column( list_view, 1 );
  gtk_tree_view_column_set_clickable (col, TRUE);
  gtk_tree_view_column_set_sort_column_id (col, COL_FILE_STAT);

  col = gtk_tree_view_get_column( list_view, 5 );
  gtk_tree_view_column_set_clickable (col, TRUE);
  gtk_tree_view_column_set_sort_column_id (col, COL_FILE_STAT);
  */

  gtk_tree_view_set_rules_hint ( list_view, TRUE );
}

void ptk_file_browser_refresh( PtkFileBrowser* file_browser )
{
  /*
  * FIXME:
  * Do nothing when there is unfinished task running in the
  * working thread.
  * This should be fixed with a better way in the future.
  */
  if( file_browser->busy )
    return;

  ptk_file_browser_chdir( file_browser,
                          ptk_file_browser_get_cwd( file_browser ),
                          FALSE );
}

guint ptk_file_browser_get_n_all_files( PtkFileBrowser* file_browser )
{
  return file_browser->folder_content ? gtk_tree_model_iter_n_children(
                      file_browser->folder_content->list, NULL ) : 0;
}

guint ptk_file_browser_get_n_visible_files( PtkFileBrowser* file_browser )
{
  return file_browser->list_filter ? gtk_tree_model_iter_n_children(
      file_browser->list_filter, NULL ) : 0;
}

GList* folder_view_get_selected_items( PtkFileBrowser* file_browser,
                                       GtkTreeModel** model )
{
  GtkTreeSelection* tree_sel;
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    *model = ptk_icon_view_get_model( PTK_ICON_VIEW(file_browser->folder_view) );
    return ptk_icon_view_get_selected_items( PTK_ICON_VIEW(file_browser->folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(file_browser->folder_view) );
    return gtk_tree_selection_get_selected_rows( tree_sel, model );
  }
  return NULL;
}

static char* folder_view_get_drop_dir( PtkFileBrowser* file_browser, int x, int y )
{
  GtkTreePath* tree_path = NULL;
  GtkTreeModel *model;
  GtkTreeViewColumn* col;
  GtkTreeIter it;
  char* ufilename;
  char* udest_path = NULL;
  struct stat* pstat;

  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    ptk_icon_view_widget_to_icon_coords ( PTK_ICON_VIEW(file_browser->folder_view),
                                          x, y, &x, &y );
    tree_path = folder_view_get_tree_path_at_pos(file_browser, x, y);
    model = ptk_icon_view_get_model( PTK_ICON_VIEW(file_browser->folder_view) );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(file_browser->folder_view), x, y,
                                   NULL, &col, NULL, NULL );
    if( col == gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), 0) )
    {
      gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW(file_browser->folder_view), x, y,
                                          &tree_path, NULL );
      model = gtk_tree_view_get_model( GTK_TREE_VIEW(file_browser->folder_view) );
    }
  }
  if( tree_path )
  {
    gtk_tree_model_get_iter( model, &it, tree_path );
    gtk_tree_model_get( model, &it, COL_FILE_NAME, &ufilename,
                        COL_FILE_STAT, &pstat, -1 );
    if( ufilename )
    {
      if( S_ISDIR( pstat->st_mode ) )
      {
        udest_path = g_build_filename( ptk_file_browser_get_cwd(file_browser),
                                        ufilename, NULL );
      }
      else  /* Drop on a file, not folder */
      {
        /* Return current directory */
        udest_path = g_strdup(ptk_file_browser_get_cwd(file_browser));
      }
      g_free( ufilename );
    }
    gtk_tree_path_free( tree_path );
  }
  else
  {
    return g_strdup(ptk_file_browser_get_cwd(file_browser));
  }
  return udest_path;
}

void on_folder_view_drag_data_received (GtkWidget        *widget,
                                        GdkDragContext   *drag_context,
                                        gint              x,
                                        gint              y,
                                        GtkSelectionData *sel_data,
                                        guint             info,
                                        guint             time,
                                        gpointer          user_data)
{
  gchar **list, **puri;
  GList* files = NULL;
  FileOperation* file_operation;
  FileOperationType file_action = FO_MOVE;
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  char* dest_dir;
  char* file_path;

  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name( widget, "drag-data-received" );

  if ((sel_data->length >= 0) && (sel_data->format == 8))  {
    puri = list = gtk_selection_data_get_uris( sel_data );
    if( puri )
    {
      if( 0 == (drag_context->action  &
          (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)) )
      {
        drag_context->action = GDK_ACTION_MOVE;
      }
      gtk_drag_finish (drag_context, TRUE, FALSE, time);

      while( *puri ){
        if( **puri == '/' ) {
          file_path = g_strdup( *puri );
        }
        else {
          file_path = g_filename_from_uri(*puri, NULL, NULL);
        }

        if( file_path ){
          files = g_list_prepend( files, file_path );
        }
        ++puri;
      }
      g_strfreev( list );

      switch( drag_context->action )
      {
        case GDK_ACTION_COPY:
          file_action = FO_COPY;
          break;
        case GDK_ACTION_LINK:
          file_action = FO_LINK;
          break;
      }

      if( files ) {
        dest_dir = folder_view_get_drop_dir( file_browser, x, y );
        /* g_print( "dest_dir = %s\n", dest_dir ); */
        file_operation = file_operation_new( files, dest_dir, file_action, NULL,
                                             GTK_WINDOW(gtk_widget_get_toplevel(file_browser)) );
        file_operation_run( file_operation );
        g_free( dest_dir );
      }
      return;
    }
  }
  gtk_drag_finish (drag_context, FALSE, FALSE, time);
}

void on_folder_view_drag_data_get (GtkWidget        *widget,
                                   GdkDragContext   *drag_context,
                                   GtkSelectionData *sel_data,
                                   guint             info,
                                   guint             time,
                                   PtkFileBrowser  *file_browser)
{
  GdkAtom type = gdk_atom_intern( "text/uri-list", FALSE );
  gchar* uri;
  GString* uri_list = g_string_sized_new( 8192 );
  GList* sels = ptk_file_browser_get_selected_files( file_browser );
  GList* sel;

  drag_context->actions = GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK;
  drag_context->suggested_action = GDK_ACTION_MOVE;

  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name( widget, "drag-data-get" );

  for( sel = sels; sel; sel = g_list_next(sel) )
  {
    uri = g_filename_to_uri( (char*)sel->data, NULL, NULL );

    g_string_append( uri_list, uri );
    g_free( uri );

    g_string_append( uri_list, "\r\n" );
  }

  g_list_foreach( sels, (GFunc)g_free, NULL );
  g_list_free( sels );

  gtk_selection_data_set ( sel_data, type, 8, ( guchar*)uri_list->str, uri_list->len+1 );
  g_string_free( uri_list, TRUE );
}


void on_folder_view_drag_begin     (GtkWidget      *widget,
                                    GdkDragContext *drag_context,
                                    gpointer        user_data)
{
  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name ( widget, "drag-begin" );

  gtk_drag_set_icon_stock ( drag_context, GTK_STOCK_DND_MULTIPLE, 1, 1 );
}

static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y )
{
  GtkScrolledWindow* scroll;
  GtkAdjustment *vadj;
  gint vy;
  gdouble vpos;
  GtkTreePath *tree_path;

  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW(file_browser->folder_view), x, y );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(file_browser->folder_view), x, y,
                                   &tree_path, NULL, NULL, NULL );
  }
  return tree_path;
}

gboolean on_folder_view_auto_scroll( GtkScrolledWindow* scroll )
{
  GtkAdjustment *vadj;
  gdouble vpos;

  vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
  vpos = gtk_adjustment_get_value( vadj );

  if( folder_view_auto_scroll_direction == GTK_DIR_UP )
  {
    vpos -= vadj->step_increment;
    if( vpos > vadj->lower )
      gtk_adjustment_set_value ( vadj, vpos );
    else
      gtk_adjustment_set_value ( vadj, vadj->lower );
  }
  else
  {
    vpos += vadj->step_increment;
    if( (vpos + vadj->page_size) < vadj->upper )
      gtk_adjustment_set_value ( vadj, vpos );
    else
      gtk_adjustment_set_value ( vadj, (vadj->upper - vadj->page_size) );
  }

  return TRUE;
}

gboolean on_folder_view_drag_motion (GtkWidget      *widget,
                                     GdkDragContext *drag_context,
                                     gint            x,
                                     gint            y,
                                     guint           time,
                                     PtkFileBrowser* file_browser )
{
  GtkScrolledWindow* scroll;
  GtkAdjustment *hadj, *vadj;
  gint vx, vy;
  gdouble hpos, vpos;
  GtkTreeModel* model;
  GtkTreePath *tree_path;
  GtkTreeViewColumn* col;
  GtkTreeIter it;
  gchar *file_name;
  gchar* dir_path;
  struct stat *file_stat;
  GdkDragAction suggested_action;

  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name ( widget, "drag-motion" );

  scroll = GTK_SCROLLED_WINDOW( gtk_widget_get_parent ( widget ) );

  vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
  vpos = gtk_adjustment_get_value( vadj );

  if( y < 32 ){
    /* Auto scroll up */
    if( ! folder_view_auto_scroll_timer ) {
      folder_view_auto_scroll_direction = GTK_DIR_UP;
      folder_view_auto_scroll_timer = g_timeout_add(
                                        150,
                                        (GSourceFunc)on_folder_view_auto_scroll,
                                        scroll );
    }
  }
  else if( y > (widget->allocation.height - 32 ) ) {
    if( ! folder_view_auto_scroll_timer ) {
      folder_view_auto_scroll_direction = GTK_DIR_DOWN;
      folder_view_auto_scroll_timer = g_timeout_add(
                                        150,
                                        (GSourceFunc)on_folder_view_auto_scroll,
                                        scroll );
    }
  }
  else if( folder_view_auto_scroll_timer ) {
    g_source_remove( folder_view_auto_scroll_timer );
    folder_view_auto_scroll_timer = 0;
  }

  tree_path = NULL;
  if( file_browser->view_mode == FBVM_ICON_VIEW )
  {
    ptk_icon_view_widget_to_icon_coords(  PTK_ICON_VIEW(widget), x, y, &x, &y );
    tree_path = ptk_icon_view_get_path_at_pos( PTK_ICON_VIEW(widget), x, y );
    model = ptk_icon_view_get_model( PTK_ICON_VIEW(widget) );
  }
  else
  {
    if( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(widget), x, y,
                                       NULL, &col, NULL, NULL ) )
    {
      if( gtk_tree_view_get_column (GTK_TREE_VIEW(widget), 0) == col ) {
        gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW(widget), x, y,
                                           &tree_path, NULL);
        model = gtk_tree_view_get_model( GTK_TREE_VIEW(widget) );
      }
    }
  }

  if( tree_path )
  {
    if( gtk_tree_model_get_iter( model, &it, tree_path ) )
    {
      gtk_tree_model_get( model, &it, COL_FILE_STAT, &file_stat, -1 );
      if( ! file_stat || ! S_ISDIR(file_stat->st_mode) )
      {
        gtk_tree_path_free( tree_path );
        tree_path = NULL;
      }
    }
  }

  if( file_browser->view_mode == FBVM_ICON_VIEW )
  {
    ptk_icon_view_set_drag_dest_item ( PTK_ICON_VIEW(widget),
                                        tree_path, PTK_ICON_VIEW_DROP_INTO );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW )
  {
    gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW(widget),
                                      tree_path,
                                      GTK_TREE_VIEW_DROP_INTO_OR_AFTER );
  }

  if( tree_path )
    gtk_tree_path_free( tree_path );

  suggested_action = (drag_context->actions  & GDK_ACTION_MOVE) ?
                        GDK_ACTION_MOVE : drag_context->suggested_action;
  gdk_drag_status ( drag_context, suggested_action, time );

  return TRUE;
}

gboolean on_folder_view_drag_leave ( GtkWidget      *widget,
                                     GdkDragContext *drag_context,
                                     guint           time,
                                     PtkFileBrowser* file_browser )
{
  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name( widget, "drag-leave" );

  if( folder_view_auto_scroll_timer ) {
    g_source_remove( folder_view_auto_scroll_timer );
    folder_view_auto_scroll_timer = 0;
  }
}


gboolean on_folder_view_drag_drop   (GtkWidget      *widget,
                                    GdkDragContext *drag_context,
                                    gint            x,
                                    gint            y,
                                    guint           time,
                                    PtkFileBrowser* file_browser )
{
  GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
  /*  Don't call the default handler  */
  g_signal_stop_emission_by_name( widget, "drag-drop" );

  gtk_drag_get_data ( widget, drag_context, target, time );
  return TRUE;
}


void on_folder_view_drag_end        (GtkWidget      *widget,
                                     GdkDragContext *drag_context,
                                     gpointer        user_data)
{
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  if( folder_view_auto_scroll_timer ) {
    g_source_remove( folder_view_auto_scroll_timer );
    folder_view_auto_scroll_timer = 0;
  }
  if( file_browser->view_mode == FBVM_ICON_VIEW ) {
    ptk_icon_view_set_drag_dest_item( PTK_ICON_VIEW(widget), NULL, 0 );
  }
  else if( file_browser->view_mode == FBVM_LIST_VIEW ) {
    gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW(widget), NULL, 0 );
  }
}

void ptk_file_browser_rename_selected_file( PtkFileBrowser* file_browser )
{
  GtkWidget* input_dlg;
  GtkLabel* prompt;
  GList* items;
  GtkTreeModel* model;
  GtkTreeIter it;
  GtkTreeSelection* tree_sel;
  gchar* dir;
  gchar* file_name;
  gchar* ufile_name;
  gchar* to_path;
  gchar* ufull_path;
  gchar* from_path;

  if( gtk_widget_is_focus ( GTK_WIDGET(file_browser->folder_view) ) ){
    items = folder_view_get_selected_items(file_browser, &model);
    if( !items )
      return;
    gtk_tree_model_get_iter( model, &it, (GtkTreePath*)items->data );
    gtk_tree_model_get( model, &it, 1, &ufile_name, -1);
    g_list_foreach( items, (GFunc)gtk_tree_path_free, NULL );
    g_list_free( items );
  }
/*  else if( gtk_widget_is_focus ( GTK_WIDGET(file_browser->side_view) ) ){
    dir = g_path_get_dirname ( ptk_file_browser_get_cwd(file_browser) );
    file_name = g_strdup(
        ptk_file_browser_get_cwd(file_browser) + strlen(dir) );
    tree_sel = gtk_tree_view_get_selection( file_browser->side_view );
    gtk_tree_selection_get_selected (tree_sel, &model, &it );
  }
  else
    return;
*/

  ufull_path = g_build_filename( ptk_file_browser_get_cwd(file_browser),
                                  ufile_name, NULL );
  from_path = g_filename_from_utf8( ufull_path, -1,
                                    NULL, NULL, NULL );
  g_free( ufull_path );

  input_dlg = input_dialog_new( _("Rename File"),
                                _("Please input new file name:"),
                                ufile_name,
                                GTK_WINDOW(gtk_widget_get_toplevel(file_browser)) );
  g_free(ufile_name);

  while( gtk_dialog_run( GTK_DIALOG(input_dlg)) == GTK_RESPONSE_OK )
  {
    ufile_name = input_dialog_get_text( input_dlg );
    ufull_path = g_build_filename( ptk_file_browser_get_cwd(file_browser),
                                   ufile_name, NULL );
    g_free( ufile_name );
    to_path = g_filename_from_utf8( ufull_path, -1,
                                    NULL, NULL, NULL );
    g_free( ufull_path );
    if( g_file_test( to_path, G_FILE_TEST_EXISTS ) )
    {
      prompt = GTK_LABEL( input_dialog_get_label( input_dlg ) );
      gtk_label_set_text( prompt,
                          _("The file name you specified has existed.\n"
                              "Please input a new one:") );
      g_free( to_path );
      continue;
    }
    g_rename( from_path, to_path );
    g_free( from_path );
    g_free( to_path );
    break;
  }

  gtk_widget_destroy( input_dlg );

/*
  This seems to be buggy? Is it a bug of GTK+ 2.8?
  Everytime my program crashes here.
*/
/*  gtk_widget_grab_focus (GTK_WIDGET(file_browser->folder_view));
  ptk_icon_view_set_cursor( file_browser->folder_view,
                            (GtkTreePath*)items->data,
                            NULL, TRUE );
*/
}

gboolean ptk_file_browser_can_paste( PtkFileBrowser* file_browser )
{
  return FALSE;
}

void ptk_file_browser_paste( PtkFileBrowser* file_browser )
{
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
  gchar **uri_list, **puri;
  GtkSelectionData* sel_data = NULL;
  GList* files = NULL;
  GList* sel_files, *sel;
  gchar* file_path;
  gchar* dest_dir;
  FileOperation* file_operation;
  FileOperationType action;

  sel_data = gtk_clipboard_wait_for_contents( clip, target );
  if( ! sel_data ){
    g_warning("no data in clipboard!\n");
    return;
  }

  if( (sel_data->length >= 0) && (sel_data->format == 8)) {
    puri = uri_list = gtk_selection_data_get_uris( sel_data );
    while( *puri ){
      file_path = g_filename_from_uri(*puri, NULL, NULL);
      if( file_path ){
        files = g_list_prepend( files, file_path );
      }
      ++puri;
    }
    g_strfreev( uri_list );
    gtk_selection_data_free( sel_data );

    /*
    * If only one item is selected and the item is a
    * directory, paste the files in that directory;
    * otherwise, paste the file in current directory.
    */
    if( sel_files = ptk_file_browser_get_selected_files(file_browser) )  {
      dest_dir = (char*)sel_files->data;
    }
    else{
      dest_dir = ptk_file_browser_get_cwd(file_browser);
    }
    if( clipboard_action == GDK_ACTION_MOVE )
      action = FO_MOVE;
    else
      action = FO_COPY;
    file_operation = file_operation_new( files,
                                         dest_dir,
                                         action,
                                         NULL,
                                         GTK_WINDOW(gtk_widget_get_toplevel(file_browser)) );
    file_operation_run( file_operation );
    if( sel_files ){
      g_list_foreach( sel_files, (GFunc)g_free, NULL);
      g_list_free( sel_files );
    }
  }
}

gboolean ptk_file_browser_can_cut_or_copy( PtkFileBrowser* file_browser )
{
  return FALSE;
}


static void clipboard_get_data (GtkClipboard *clipboard,
                                GtkSelectionData *selection_data,
                                guint info,
                                gpointer user_data)
{
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
  GList* sel;
  gchar* uri;
  GString* list = g_string_sized_new( 8192 );

  for( sel = file_browser->clipboard_file_list;
       sel; sel = g_list_next(sel) )
  {
    uri = g_filename_to_uri( (char*)sel->data, NULL, NULL );
    g_string_append( list, uri );
    g_free( uri );
    g_string_append( list, "\r\n" );
  }

  g_print("copy: %s\n", list->str);

  gtk_selection_data_set ( selection_data, target, 8,
                           ( guchar*)list->str,
                           list->len + 1 );
  g_string_assign( list, "" );

  for( sel = file_browser->clipboard_file_list;
       sel; sel = g_list_next(sel) )
  {
    g_string_append( list, (char*)sel->data );
    g_string_append( list, "\r\n" );
  }

  g_string_free( list, TRUE );
}

static void clipboard_clean_data (GtkClipboard *clipboard,
                                  gpointer user_data)
{
/*  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
  g_print("clean clipboard!\n");
  if( file_browser->clipboard_file_list )
  {
    g_list_foreach( file_browser->clipboard_file_list,
                    (GFunc)g_free, NULL );
    g_list_free( file_browser->clipboard_file_list );
    file_browser->clipboard_file_list = NULL;
  }
*/  clipboard_action = GDK_ACTION_DEFAULT;
}

void ptk_file_browser_cut( PtkFileBrowser* file_browser )
{
  /* What "cut" and "copy" do are the same.
  *  The only difference is clipboard_action = GDK_ACTION_MOVE.
  */
  ptk_file_browser_copy( file_browser );
  clipboard_action = GDK_ACTION_MOVE;
}

void ptk_file_browser_copy( PtkFileBrowser* file_browser )
{
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
  GList* target;
  gint i, n_targets;
  GtkTargetEntry* targets;
  GtkTargetPair* pair;

  gtk_target_list_add_text_targets( target_list, 0 );
  n_targets = g_list_length( target_list->list ) + 1;

  targets = g_new0( GtkTargetEntry, n_targets );
  target = target_list->list;
  for(  i = 0; target; ++i, target = g_list_next(target) ) {
    pair = (GtkTargetPair*)target->data;
    targets[i].target = gdk_atom_name (pair->target);
  }
  targets[i].target = "text/uri-list";

  gtk_target_list_unref (target_list);

  if( file_browser->clipboard_file_list )
  {
    g_list_foreach( file_browser->clipboard_file_list,
                    (GFunc)g_free, NULL );
    g_list_free( file_browser->clipboard_file_list );
  }
  file_browser->clipboard_file_list = ptk_file_browser_get_selected_files( file_browser );

  if( file_browser->clipboard_file_list )
  {
    gtk_clipboard_set_with_data (
              clip, targets, n_targets,
              clipboard_get_data,
              clipboard_clean_data,
              (gpointer)file_browser);
  }
  g_free( targets );

  clipboard_action = GDK_ACTION_COPY;
}

void ptk_file_browser_can_delete( PtkFileBrowser* file_browser )
{

}

void ptk_file_browser_delete( PtkFileBrowser* file_browser )
{
  GtkWidget* dlg;
  GList* file_list;
  gchar* file_path;
  gint ret;
  GList* sel;
  FileOperation* file_operation;

  if( ! file_browser->n_sel_files )
    return;

  dlg = gtk_message_dialog_new( GTK_WINDOW(gtk_widget_get_toplevel(file_browser)),
                                GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_YES_NO,
                                _("Really delete selected files?"));
  ret = gtk_dialog_run( GTK_DIALOG(dlg) );
  gtk_widget_destroy( dlg );
  if( ret != GTK_RESPONSE_YES ){
    return;
  }

  file_list = ptk_file_browser_get_selected_files( file_browser );
  for( sel = file_list; sel; sel = g_list_next(sel) ) {
    file_path = g_filename_from_utf8( (char*)sel->data,
                                      -1, NULL, NULL, NULL );

    file_operation = file_operation_delete_file(file_path, NULL,
                          GTK_WINDOW(gtk_widget_get_toplevel(file_browser)) );
    file_operation_run( file_operation );
    g_free( file_path );
  }

  g_list_foreach( file_list, (GFunc)g_free, NULL );
  g_list_free( file_list );
}

/*
* Return a list of selected filenames alnog with their mime-types
* (in UTF8-encoded full path)
* If mime_types is not NULL, a list of the mime-types of the
* selected files will be returned in a newly allocated list.
* NOTICE:
* The returned list of mime_types should only be freed with:
* g_list_free( mime_list );
* Don't free the data in that mime-types list because they are const char*.
* The returned file list should be freed by calling:
* g_list_foreach( file_list, (GFunc)g_free, NULL );
* g_list_free( file_list );
*/
GList* ptk_file_browser_get_selected_files_with_mime_types( PtkFileBrowser* file_browser,
                                                        GList** mime_types )
{
  GList* sel_files;
  GList* sel;
  GList* file_list = NULL;
  GtkTreeModel* model;
  GtkTreeIter it;
  gchar* file_name;
  const char* mime_type;

  sel_files = folder_view_get_selected_items(file_browser, &model);

  if( ! sel_files )
    return NULL;
  if( mime_types )
    *mime_types = NULL;

  for( sel = sel_files; sel; sel = g_list_next(sel) ) {
    gtk_tree_model_get_iter( model, &it, (GtkTreePath*)sel->data );
    gtk_tree_model_get( model, &it, COL_FILE_NAME, &file_name,
                                    COL_FILE_TYPE, &mime_type, -1 );
    file_list = g_list_append( file_list,
                               g_build_filename(
                               ptk_file_browser_get_cwd(file_browser),
                               file_name, NULL ) );
    if( mime_types )
      *mime_types = g_list_append( *mime_types, mime_type );
  }
  g_list_foreach( sel_files,
                  (GFunc)gtk_tree_path_free,
                  NULL);
  g_list_free( sel_files );
  return file_list;
}

/* Return a list of selected filenames (in UTF8-encoded full path) */
GList* ptk_file_browser_get_selected_files( PtkFileBrowser* file_browser )
{
  return ptk_file_browser_get_selected_files_with_mime_types( file_browser, NULL );
}

void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser )
{
  ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                    const char* app_desktop )

{
  GList* sel_files = NULL, *file;
  GList* mime_types = NULL, *mime;
  gchar* new_path;
  const char* desktop_file;

  gchar *app, *exec, *cmd, *file_list[2] = {NULL, NULL};

  GError* err;
  gboolean is_dir;

  sel_files = ptk_file_browser_get_selected_files_with_mime_types(file_browser,
                                                                  &mime_types);

  for( file = sel_files, mime = mime_types; file; file = file->next, mime = mime->next )
  {
    if( G_LIKELY(file->data) ) {
      new_path = g_filename_from_utf8( (char*)file->data, -1, NULL, NULL, NULL );

      if( G_LIKELY(new_path) )
      {
        if( ! app_desktop && g_file_test(new_path, G_FILE_TEST_IS_DIR ) )
        {
          ptk_file_browser_chdir( file_browser, (char*)file->data, TRUE );
          g_free( new_path );
          break;
        }
        else
        {
          /* If this file is an executable file, run it. */
          if( !app_desktop && xdg_mime_is_executable_file(new_path, (char*)mime->data) )
          {
            err = NULL;
            if( ! g_spawn_command_line_async (new_path, &err) )  {
              ptk_show_error( gtk_widget_get_toplevel(file_browser), err->message );
              g_error_free( err );
            }
            else  {
              g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, new_path, PTK_OPEN_FILE );
            }
            return;
          }

          if( app_desktop ) {
            app = g_strdup( app_desktop );
          }
          else {
            if( mime->data ) {
              app = get_default_app_for_mime_type( (char*)mime->data );
              if( !app && xdg_mime_is_text_file( new_path, (char*)mime->data ) )
              {
                app = get_default_app_for_mime_type( XDG_MIME_TYPE_PLAIN_TEXT );
              }
            }
            else {
              app = NULL;
            }
          }

          if( app )
          {
            /* Check whether this is an app desktop file or just a command line */
            if( g_str_has_suffix (app, ".desktop") ) {
              exec = get_exec_from_desktop_file(app);
              desktop_file = app;
            }
            else {
              if( ! strchr( app, '%' ) )  { /* No filename parameters */
                exec = g_strconcat( app, " %f", NULL );
              }
              else {
                exec = g_strdup( app );
              }
              desktop_file = NULL;
            }

            if( exec ) {
              file_list[0] = new_path;
              file_list[1] = NULL;
              cmd = translate_app_exec_to_command_line(exec, desktop_file, file_list);

              if( cmd ) {
                /* g_print( "Debug: Execute %s\n", cmd ); */
                g_spawn_command_line_async( cmd, NULL );
                g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, cmd, PTK_OPEN_FILE );
                g_free( cmd );
              }
              g_free( app );
              g_free( exec );
            }
          }
        }
        g_free( new_path );
      }
    }
  }
  if(mime_types) {
    /*
      These strings shouldn't be freed, because they are just cont char* to
      internal data provided by mime_type detection system.
      g_list_foreach( mime_types, (GFunc)g_free, NULL );
    */
    g_list_free( mime_types );
  }
  if( sel_files ) {
    g_list_foreach( sel_files, (GFunc)g_free, NULL );
    g_list_free( sel_files );
  }
}

/* Signal handlers for popup menu */
static void on_popup_selection_done( GtkMenuShell* popup, gpointer data )
{
  GList* additional_items = (GList*)g_object_get_data( G_OBJECT( popup ),
                                                       "additional_items");
  if( additional_items ){
    g_list_foreach( additional_items, (GFunc)gtk_widget_destroy, NULL );
    g_list_free( additional_items );
  }
}

static PtkMenuItemEntry create_new_menu[] = {
  PTK_IMG_MENU_ITEM( N_("_Folder"), "gtk-directory", on_popup_new_folder_activate, 0, 0 ),
  PTK_IMG_MENU_ITEM( N_("_Text File"), "gtk-edit", on_popup_new_text_file_activate, 0, 0 ),
  PTK_MENU_END
};

static PtkMenuItemEntry basic_popup_menu[] = {
  PTK_MENU_ITEM( N_("Open _with..."), NULL, 0, 0 ),
  PTK_SEPARATOR_MENU_ITEM,
  PTK_STOCK_MENU_ITEM( "gtk-cut", on_popup_cut_activate ),
  PTK_STOCK_MENU_ITEM( "gtk-copy", on_popup_copy_activate ),
  PTK_STOCK_MENU_ITEM( "gtk-paste", on_popup_paste_activate ),
  PTK_IMG_MENU_ITEM( N_("_Delete"), "gtk-delete", on_popup_delete_activate, GDK_Delete, 0 ),
  PTK_IMG_MENU_ITEM( N_("_Rename"), "gtk-edit", on_popup_rename_activate, 0, 0 ),
  PTK_SEPARATOR_MENU_ITEM,
  PTK_POPUP_IMG_MENU( N_("_Create New"), "gtk-new", &create_new_menu ),
  PTK_IMG_MENU_ITEM( N_("_Properties"), "gtk-info", on_popup_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
  PTK_MENU_END
};

static GtkWidget* ptk_file_browser_create_basic_popup_menu()
{
  GtkWidget* popup;
  GtkWidget *open_with;
  GtkWidget *create_new;
  GtkAccelGroup *accel_group;

  accel_group = gtk_accel_group_new ();

  popup = gtk_menu_new ();
  g_object_set_data(G_OBJECT(popup), "accel", accel_group);

  basic_popup_menu[0].ret = &open_with;
  basic_popup_menu[8].ret = &create_new;
  ptk_menu_add_items_from_data( popup, basic_popup_menu, popup, accel_group );

  g_object_set_data(G_OBJECT(popup), "open_with", open_with );
  g_object_set_data( G_OBJECT(popup), "create_new", create_new );

  gtk_widget_show_all( GTK_WIDGET(popup) );

  g_signal_connect ((gpointer) popup, "selection-done",
                     G_CALLBACK (on_popup_selection_done),
                     NULL );

  return popup;
}

/* Retrive popup menu for selected files */
GtkWidget* ptk_file_browser_get_popup_menu_for_file( PtkFileBrowser* file_browser,
                                                     const char* file_path,
                                                     const char* mime_type )
{
  static GtkWidget* popup = NULL;
  GtkWidget *open;
  char* open_title;
  GtkWidget *open_in_new_tab;
  GtkWidget *open_in_new_win;
  GtkWidget *open_terminal;
  GtkWidget *open_with;
  GtkWidget *open_with_menu;
  GtkWidget *seperator;
  GtkWidget *open_with_another;

  GtkWidget *image;
  GtkWidget *create_new;

  GtkWidget *app_menu_item;
  GList* additional_items = NULL;
  gboolean is_dir;

  char **apps, **app;
  char* app_name, *default_app_name = NULL;

  GdkPixbuf* app_icon, *open_icon = NULL;
  GtkWidget* app_img;

  if( ! popup ){
    popup = ptk_file_browser_create_basic_popup_menu();
  }

  g_object_set_data( G_OBJECT(popup), "PtkFileBrowser", file_browser );

  open_with = GTK_WIDGET( g_object_get_data( G_OBJECT(popup), "open_with") );
  create_new = GTK_WIDGET( g_object_get_data( G_OBJECT(popup), "create_new") );
  /* Add some special menu items */
  if( mime_type == XDG_MIME_TYPE_DIRECTORY )
  {
    seperator = gtk_separator_menu_item_new ();
    gtk_widget_show (seperator);
    gtk_menu_shell_insert ( GTK_MENU_SHELL(popup), seperator, 0 );
    additional_items = g_list_prepend( additional_items, seperator );
    gtk_widget_set_sensitive (seperator, FALSE);

    open_in_new_tab = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
    gtk_widget_show (open_in_new_tab);
    gtk_menu_shell_insert ( GTK_MENU_SHELL(popup), open_in_new_tab, 1 );
    additional_items = g_list_prepend( additional_items, open_in_new_tab );
    g_signal_connect ((gpointer) open_in_new_tab, "activate",
                       G_CALLBACK (on_popup_open_in_new_tab_activate),
                       file_browser);

    open_in_new_win = gtk_menu_item_new_with_mnemonic (_("Open in New Window"));
    gtk_widget_show (open_in_new_win);
    gtk_menu_shell_insert ( GTK_MENU_SHELL(popup), open_in_new_win, 2 );
    additional_items = g_list_prepend( additional_items, open_in_new_win );
    g_signal_connect ((gpointer) open_in_new_win, "activate",
                       G_CALLBACK (on_popup_open_in_new_win_activate),
                       file_browser);

    open_terminal = gtk_image_menu_item_new_with_mnemonic (_("Open in Terminal"));
    gtk_menu_shell_insert ( GTK_MENU_SHELL(popup), open_terminal, 3 );
    additional_items = g_list_prepend( additional_items, open_terminal );
    g_signal_connect_swapped ((gpointer) open_terminal, "activate",
                              G_CALLBACK (ptk_file_browser_open_terminal),
                              file_browser);
    gtk_widget_add_accelerator( open_terminal, "activate",
                  GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(popup), "accel")),
                  GDK_F4, 0, GTK_ACCEL_VISIBLE );
    image = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image( open_terminal, image );
    gtk_widget_show_all (open_terminal);

    is_dir = TRUE;
    gtk_widget_show( create_new );
  }
  else{
    is_dir = FALSE;
    gtk_widget_show( open_with );
    gtk_widget_hide( create_new );
  }

  /*  Add all of the apps  */
  open_with_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (open_with), open_with_menu);

  apps = get_all_apps_for_mime_type( mime_type );
  if( ! apps )  /* fallback for text files */
  {
    if( xdg_mime_is_text_file( file_path, mime_type ) )
    {
      apps = get_all_apps_for_mime_type( XDG_MIME_TYPE_PLAIN_TEXT );
    }
  }

  default_app_name = NULL;
  if( apps ){
    for( app = apps; *app; ++app ){
      if( (app - apps) == 1 ) /* Add a separator after default app */
      {
        seperator = gtk_separator_menu_item_new ();
        gtk_widget_show (seperator);
        gtk_container_add (GTK_CONTAINER (open_with_menu), seperator);
        gtk_widget_set_sensitive (seperator, FALSE);
      }

      app_name = get_app_name_from_desktop_file( *app );
      if( app_name ){
        app_menu_item = gtk_image_menu_item_new_with_label ( app_name );
        if( app == apps ) {
          default_app_name = app_name;
        }
        else {
          g_free( app_name );
        }
      }
      else{
        app_menu_item = gtk_image_menu_item_new_with_label ( *app );
      }
      g_object_set_data_full( G_OBJECT(app_menu_item), "desktop_file",
                              *app, g_free );
      app_icon = get_app_icon_from_desktop_file( *app, 16 );
      if( app_icon )
      {
        app_img = gtk_image_new_from_pixbuf( app_icon );
        if( app == apps ) { /* Default app */
          open_icon = app_icon;
        }
        else {
          gdk_pixbuf_unref( app_icon );
        }
        gtk_image_menu_item_set_image ( app_menu_item, app_img );
      }
      /*
      *  Little trick: remove the string from string vector so that g_strfreev()
      *  will not free the string *app.
      */
      *app = NULL;
      gtk_container_add (GTK_CONTAINER (open_with_menu), app_menu_item );

      g_signal_connect( G_OBJECT(app_menu_item), "activate",
                        G_CALLBACK(on_popup_run_app), (gpointer)file_browser );
    }
    g_strfreev( apps );

    seperator = gtk_separator_menu_item_new ();
    gtk_container_add (GTK_CONTAINER (open_with_menu), seperator);
  }

  open_with_another = gtk_menu_item_new_with_mnemonic (_("_Open with another program"));
  gtk_container_add (GTK_CONTAINER (open_with_menu), open_with_another);
  g_signal_connect ((gpointer) open_with_another, "activate",
                    G_CALLBACK (on_popup_open_with_another_activate),
                    file_browser);

  gtk_widget_show_all( open_with_menu );

  /* Create Open menu item */
  open = NULL;
  if( !is_dir ) { /* Not a dir */
    if( xdg_mime_is_executable_file(file_path, mime_type) ) {
      open = gtk_image_menu_item_new_with_mnemonic(_("E_xecute"));
      app_img = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU );
      gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(open), app_img );
    }
    else {
      if( default_app_name ) {
        open_title = g_strdup_printf( _("_Open with \"%s\""), default_app_name );
        app_name = get_app_name_from_desktop_file( default_app_name );
        open = gtk_image_menu_item_new_with_mnemonic( open_title );
        g_free( open_title );
      }
      else {
        open = gtk_image_menu_item_new_with_mnemonic( _("_Open") );
      }
      if( open_icon ) {
        app_img = gtk_image_new_from_pixbuf( open_icon );
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(open), app_img );
      }
    }
  }
  else {
    open = gtk_image_menu_item_new_with_mnemonic( _("_Open") );
    app_img = gtk_image_new_from_stock( GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(open), app_img );
  }
  gtk_widget_show(open);
  g_signal_connect( open, "activate", G_CALLBACK(on_popup_open_activate), popup );
  gtk_menu_shell_insert( GTK_MENU_SHELL(popup), open, 0 );
  additional_items = g_list_append( additional_items, open );

  if( default_app_name )
    g_free( default_app_name );
  if( open_icon )
    gdk_pixbuf_unref( open_icon );

  g_object_set_data( G_OBJECT(popup), "additional_items",
                     (gpointer)additional_items );

  return popup;
}


void
on_popup_open_activate                       (GtkMenuItem     *menuitem,
                                              gpointer         user_data)
{
  PtkFileBrowser* file_browser;
  file_browser = (PtkFileBrowser*)g_object_get_data(G_OBJECT(user_data),
                                             "PtkFileBrowser");
  ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void
on_popup_open_with_another_activate          (GtkMenuItem     *menuitem,
                                              gpointer         user_data)
{
  GtkWidget* dlg;
  char* app = NULL;
  GtkTreeModel* model;
  GtkTreeIter it;
  const char* mime_type;
  GList* sel_files;
  PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;

  sel_files = folder_view_get_selected_items(file_browser, &model);
  if( sel_files ) {
    gtk_tree_model_get_iter( model, &it, (GtkTreePath*)sel_files->data );
    gtk_tree_model_get( model, &it, COL_FILE_TYPE, &mime_type, -1 );
    g_list_foreach( sel_files, (GFunc)gtk_tree_path_free, NULL );
    g_list_free( sel_files );
  }
  else{
    mime_type = XDG_MIME_TYPE_DIRECTORY;
  }
  dlg = app_chooser_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(file_browser)),
                               mime_type);

  if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
  {
    app = app_chooser_dialog_get_selected_app(dlg);
    if( app ) {
      /* The selected app is set to default action */
      if( app_chooser_dialog_get_set_default(dlg) ){
        set_default_app_for_mime_type( mime_type, app );
      }
      ptk_file_browser_open_selected_files_with_app( file_browser, app );
      g_free( app );
    }
  }
  gtk_widget_destroy( dlg );
}

void on_popup_run_app( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
  char* desktop_file = g_object_get_data( G_OBJECT(menuitem), "desktop_file");
  if( !desktop_file )
    return;
  ptk_file_browser_open_selected_files_with_app( file_browser, desktop_file );
}

void ptk_file_browser_open_terminal( PtkFileBrowser* file_browser )
{
  GList* list = ptk_file_browser_get_selected_files( file_browser );
  const char* path;
  if( list ) {
    path = (char*)list->data;
    g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, path, PTK_OPEN_TERMINAL );
    g_list_foreach( list, (GFunc)g_free, NULL );
    g_list_free(list);
  }
  else {
    path = ptk_file_browser_get_cwd(file_browser);
    g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, path, PTK_OPEN_TERMINAL );
  }
}


void on_popup_open_in_new_tab_activate( GtkMenuItem *menuitem,
                                        PtkFileBrowser* file_browser )
{
  GList* sel;
  GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
  if( sel_files ) {
    for( sel = sel_files; sel; sel = sel->next ) {
      if( g_file_test((char*)sel->data, G_FILE_TEST_IS_DIR) ) {
        g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, sel->data, PTK_OPEN_NEW_TAB );
      }
    }
    g_list_foreach( sel_files, (GFunc)g_free, NULL );
    g_list_free( sel_files );
  }
  else {
    g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0,
                   ptk_file_browser_get_cwd(file_browser), PTK_OPEN_NEW_TAB );
  }
}

void on_popup_open_in_new_win_activate( GtkMenuItem *menuitem,
                                        PtkFileBrowser* file_browser )
{
  GList* sel;
  GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
  if( sel_files ) {
    for( sel = sel_files; sel; sel = sel->next ) {
      if( g_file_test((char*)sel->data, G_FILE_TEST_IS_DIR) ) {
        g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0, sel->data, PTK_OPEN_NEW_WINDOW );
      }
    }
    g_list_foreach( sel_files, (GFunc)g_free, NULL );
    g_list_free( sel_files );
  }
  else {
    g_signal_emit( file_browser, signals[OPEN_ITEM_SIGNAL], 0,
                   ptk_file_browser_get_cwd(file_browser), PTK_OPEN_NEW_WINDOW );
  }
}

void
on_popup_cut_activate                      (GtkMenuItem     *menuitem,
                                            gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                         "PtkFileBrowser" );
  if( file_browser )
    ptk_file_browser_cut( file_browser );
}


void
on_popup_copy_activate                       (GtkMenuItem     *menuitem,
                                            gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                      "PtkFileBrowser" );
  if( file_browser )
    ptk_file_browser_copy( file_browser );
}


void
on_popup_paste_activate                      (GtkMenuItem     *menuitem,
                                              gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                        "PtkFileBrowser" );
  if( file_browser )
    ptk_file_browser_paste( file_browser );
}


void
on_popup_delete_activate                     (GtkMenuItem     *menuitem,
                                              gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                      "PtkFileBrowser" );
  if( file_browser )
    ptk_file_browser_delete( file_browser );
}

void
on_popup_rename_activate                      (GtkMenuItem     *menuitem,
                                               gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                      "PtkFileBrowser" );
  if( file_browser )
    ptk_file_browser_rename_selected_file( file_browser );
}

void
on_popup_new_folder_activate                  (GtkMenuItem     *menuitem,
                                               gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                              "PtkFileBrowser" );

  if( file_browser )
    ptk_file_browser_create_new_file( file_browser, TRUE );
}

void
on_popup_new_text_file_activate               (GtkMenuItem     *menuitem,
                                               gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                              "PtkFileBrowser" );

  if( file_browser )
    ptk_file_browser_create_new_file( file_browser, FALSE );
}

void ptk_file_browser_file_properties( PtkFileBrowser* file_browser )
{
  GtkWidget* dlg;
  GList* sel_files = NULL;
  GList* sel_mime_types = NULL;

  if( file_browser ) {
    sel_files = ptk_file_browser_get_selected_files_with_mime_types( file_browser,
        &sel_mime_types );
    if( !sel_files ){
      sel_files = g_list_append( sel_files, g_strdup(ptk_file_browser_get_cwd(file_browser)) );
      sel_mime_types = g_list_append( sel_mime_types, XDG_MIME_TYPE_DIRECTORY );
    }
    dlg = file_properties_dlg_new( GTK_WINDOW(gtk_widget_get_toplevel(file_browser)),
                                   sel_files,
                                   sel_mime_types );
    gtk_widget_show( dlg );
  }
}

void
on_popup_file_properties_activate               (GtkMenuItem     *menuitem,
                                                 gpointer         user_data)
{
  GObject* popup = G_OBJECT(user_data);
  PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup,
                                                          "PtkFileBrowser" );
  ptk_file_browser_file_properties( file_browser );
}

void ptk_file_browser_show_hidden_files( PtkFileBrowser* file_browser,
                                         gboolean show )
{
  file_browser->show_hidden_files = show;
  gboolean emit = FALSE;
  if( file_browser->list_filter ) {
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(file_browser->list_filter) );
    emit = TRUE;
  }
  if( file_browser->tree_filter ) {
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(file_browser->tree_filter) );
    emit = TRUE;
  }
  if( emit )
    g_signal_emit( file_browser, signals[CONTENT_CHANGE_SIGNAL], 0 );
}

GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser )
{
  GtkWidget* dir_tree;
  GtkTreeSelection* dir_tree_sel;
  dir_tree = ptk_dir_tree_view_new();
  dir_tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dir_tree) );
  g_signal_connect ( dir_tree_sel, "changed",
                     G_CALLBACK (on_dir_tree_sel_changed),
                     file_browser);
  return GTK_TREE_VIEW ( dir_tree );
}

GtkTreeView* ptk_file_browser_create_location_view( PtkFileBrowser* file_browser )
{
  GtkTreeView* location_view = GTK_TREE_VIEW( ptk_location_view_new() );
  g_signal_connect ( location_view, "row-activated",
                     G_CALLBACK (on_location_view_row_activated),
                     file_browser);
  return location_view;
}

void ptk_file_browser_sort_by_name( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  if( file_browser->sort_order == FB_SORT_BY_NAME )
    return;
  file_browser->sort_order = FB_SORT_BY_NAME;
  if( file_browser->list_sorter )
  {
    sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);
    gtk_tree_sortable_set_sort_func( sortable, COL_FILE_NAME,
                                     sort_files_by_name, file_browser,
                                     NULL );
    gtk_tree_sortable_set_sort_column_id( sortable,
                                          COL_FILE_NAME,
                                          file_browser->sort_type );
    update_folder_view_visible_region(file_browser);
  }
}

void ptk_file_browser_sort_by_size( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  if( file_browser->sort_order == FB_SORT_BY_SIZE )
    return;
  file_browser->sort_order = FB_SORT_BY_SIZE;
  if( file_browser->list_sorter )
  {
    sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);
    gtk_tree_sortable_set_sort_func( sortable, COL_FILE_STAT,
                                     sort_files_by_size, file_browser,
                                     NULL );
    gtk_tree_sortable_set_sort_column_id( sortable,
                                          COL_FILE_STAT,
                                          file_browser->sort_type );
    update_folder_view_visible_region(file_browser);
  }
}

void ptk_file_browser_sort_by_time( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  if( file_browser->sort_order == FB_SORT_BY_TIME )
    return;
  file_browser->sort_order = FB_SORT_BY_TIME;
  if( file_browser->list_sorter )
  {
    sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);

    gtk_tree_sortable_set_sort_func( sortable, COL_FILE_STAT,
                                     sort_files_by_time, file_browser,
                                     NULL );

    gtk_tree_sortable_set_sort_column_id( sortable,
                                          COL_FILE_STAT,
                                          file_browser->sort_type );
    update_folder_view_visible_region(file_browser);
  }
}

void ptk_file_browser_sort_ascending( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  gint col;
  GtkSortType order;
  if( file_browser->sort_type != GTK_SORT_ASCENDING )
  {
    file_browser->sort_type = GTK_SORT_ASCENDING;
    sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);
    gtk_tree_sortable_get_sort_column_id ( sortable, &col, &order );
    gtk_tree_sortable_set_sort_column_id ( sortable, col, GTK_SORT_ASCENDING );
  }
  update_folder_view_visible_region(file_browser);
}

void ptk_file_browser_sort_descending( PtkFileBrowser* file_browser )
{
  GtkTreeSortable* sortable;
  gint col;
  GtkSortType order;
  if( file_browser->sort_type != GTK_SORT_DESCENDING )
  {
    file_browser->sort_type = GTK_SORT_DESCENDING;
    sortable = GTK_TREE_SORTABLE(file_browser->list_sorter);
    gtk_tree_sortable_get_sort_column_id ( sortable, &col, &order );
    gtk_tree_sortable_set_sort_column_id ( sortable, col, GTK_SORT_DESCENDING );
  }
  update_folder_view_visible_region(file_browser);
}

void ptk_file_browser_view_as_icons( PtkFileBrowser* file_browser )
{
  if( file_browser->view_mode == FBVM_ICON_VIEW )
    return;
  file_browser->view_mode = FBVM_ICON_VIEW;
  gtk_widget_destroy( file_browser->folder_view );
  file_browser->folder_view = create_folder_view( file_browser, FBVM_ICON_VIEW );
  ptk_icon_view_set_model( PTK_ICON_VIEW(file_browser->folder_view), file_browser->list_filter );
  gtk_widget_show( file_browser->folder_view );
  gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
  g_idle_add( (GSourceFunc)update_folder_view_visible_region, file_browser);
}

void ptk_file_browser_view_as_list ( PtkFileBrowser* file_browser )
{
  if( file_browser->view_mode == FBVM_LIST_VIEW )
    return;
  file_browser->view_mode = FBVM_LIST_VIEW;
  gtk_widget_destroy( file_browser->folder_view );
  file_browser->folder_view = create_folder_view( file_browser, FBVM_LIST_VIEW );
  gtk_tree_view_set_model( GTK_TREE_VIEW(file_browser->folder_view), file_browser->list_filter );
  gtk_widget_show( file_browser->folder_view );
  gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
  g_idle_add( (GSourceFunc)update_folder_view_visible_region, file_browser);
}

void ptk_file_browser_create_new_file( PtkFileBrowser* file_browser,
                                   gboolean create_folder )
{
  gchar* ufull_path;
  gchar* full_path;
  gchar* ufile_name;
  GtkLabel* prompt;
  int result;
  GtkWidget* dlg;

  if( create_folder ) {
    dlg = input_dialog_new( _("New Folder"),
                            _("Input a name for the new folder:"),
                            _("New Folder"),
                            GTK_WINDOW(gtk_widget_get_toplevel(file_browser)));
  }
  else  {
    dlg = input_dialog_new( _("New File"),
                            _("Input a name for the new file:"),
                            _("New File"),
                            GTK_WINDOW(gtk_widget_get_toplevel(file_browser)));
  }

  while( gtk_dialog_run( GTK_DIALOG(dlg)) == GTK_RESPONSE_OK )
  {
    ufile_name = input_dialog_get_text( dlg );
    ufull_path = g_build_filename(
        ptk_file_browser_get_cwd(file_browser),
    ufile_name, NULL );
    full_path = g_filename_from_utf8( ufull_path, -1,
                                      NULL, NULL, NULL );
    g_free( ufull_path );
    g_free( ufile_name );
    if( g_file_test( full_path, G_FILE_TEST_EXISTS ) )
    {
      prompt = GTK_LABEL( input_dialog_get_label( dlg ) );
      gtk_label_set_text( prompt,
                          _("The file name you specified has existed.\n"
                              "Please input a new one:") );
      g_free( full_path );
      continue;
    }
    if( create_folder ) {
      result = g_mkdir( full_path, S_IRWXU|S_IRUSR|S_IRGRP );
    }
    else  {
      result = g_open( full_path, O_CREAT, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH );
      if( result != -1 )
        close( result );
    }
    g_free( full_path );
    break;
  }
  gtk_widget_destroy( dlg );

  if( result == -1 )
    ptk_show_error( GTK_WINDOW(gtk_widget_get_toplevel(file_browser)),
                  "The new file cannot be created!" );

}

guint ptk_file_browser_get_n_sel( PtkFileBrowser* file_browser,
                                  guint64* sel_size )
{
  if( sel_size )
    *sel_size = file_browser->sel_size;
  return file_browser->n_sel_files;
}

void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                    const char* path,
                                    gboolean* cancel )
{

}

void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser )
{

}

void ptk_file_browser_content_change( PtkFileBrowser* file_browser )
{
  update_folder_view_visible_region( file_browser );
}

void ptk_file_browser_sel_change( PtkFileBrowser* file_browser )
{

}

void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser )
{

}

void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action )
{

}

/* Side pane */

void ptk_file_browser_set_side_pane_mode( PtkFileBrowser* file_browser,
                                          PtkFileBrowserSidePaneMode mode )
{
  if( file_browser->side_pane_mode == mode )
    return;
  file_browser->side_pane_mode = mode;

  if( ! file_browser->side_pane )
    return;

  gtk_widget_destroy( GTK_WIDGET( file_browser->side_view ) );
  switch( file_browser->side_pane_mode ){
    case FB_SIDE_PANE_DIR_TREE:
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(file_browser->side_view_scroll),
                                      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
      file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
      gtk_toggle_tool_button_set_active ( file_browser->dir_tree_btn, TRUE );
      break;
    case FB_SIDE_PANE_BOOKMARKS:
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(file_browser->side_view_scroll),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
      file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
      gtk_toggle_tool_button_set_active ( file_browser->location_btn, TRUE );
      break;
  }
  gtk_container_add( GTK_CONTAINER(file_browser->side_view_scroll),
                     GTK_WIDGET(file_browser->side_view) );
  gtk_widget_show( GTK_WIDGET(file_browser->side_view));

  if( file_browser->list_filter ) {
    side_pane_chdir( file_browser, ptk_file_browser_get_cwd(file_browser) );
  }

  g_signal_emit( file_browser, signals[PANE_MODE_CHANGE_SIGNAL], 0 );
}

PtkFileBrowserSidePaneMode
ptk_file_browser_get_side_pane_mode( PtkFileBrowser* file_browser )
{
  return file_browser->side_pane_mode;
}

static void on_show_location_view( GtkWidget* item, PtkFileBrowser* file_browser )
{
  if( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON(item) ) )
    ptk_file_browser_set_side_pane_mode(file_browser, FB_SIDE_PANE_BOOKMARKS);
}

static void on_show_dir_tree( GtkWidget* item, PtkFileBrowser* file_browser )
{
  if( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON(item) ) )
    ptk_file_browser_set_side_pane_mode(file_browser, FB_SIDE_PANE_DIR_TREE);
}

static PtkToolItemEntry side_pane_bar[] = {
  PTK_RADIO_TOOL_ITEM( NULL, "gtk-harddisk", N_("Location"), on_show_location_view ),
  PTK_RADIO_TOOL_ITEM( NULL, "gtk-open", N_("Directory Tree"), on_show_dir_tree ),
  PTK_TOOL_END
};

static void ptk_file_browser_create_side_pane( PtkFileBrowser* file_browser )
{
  GtkWidget* toolbar = gtk_toolbar_new ();
  GtkTooltips* tooltips = gtk_tooltips_new ();
  file_browser->side_pane = gtk_vbox_new( FALSE, 0 );
  file_browser->side_view_scroll = gtk_scrolled_window_new( NULL, NULL );
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(file_browser->side_view_scroll),
                                       GTK_SHADOW_IN );

  switch( file_browser->side_pane_mode ){
    case FB_SIDE_PANE_DIR_TREE:
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(file_browser->side_view_scroll),
                                      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
      file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
      break;
    case FB_SIDE_PANE_BOOKMARKS:
    default:
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(file_browser->side_view_scroll),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
      file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
      break;
  }

  gtk_container_add(GTK_CONTAINER(file_browser->side_view_scroll),
                    file_browser->side_view );
  gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                      GTK_WIDGET(file_browser->side_view_scroll),
                      TRUE, TRUE, 0 );

  gtk_toolbar_set_style( toolbar, GTK_TOOLBAR_ICONS );
  side_pane_bar[0].ret = (GtkWidget**)&file_browser->location_btn;
  side_pane_bar[1].ret = (GtkWidget**)&file_browser->dir_tree_btn;
  ptk_toolbar_add_items_from_data( toolbar, side_pane_bar,
                                   file_browser, tooltips );

  gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                      toolbar, FALSE, FALSE, 0 );
  gtk_widget_show_all( file_browser->side_pane );
  gtk_paned_pack1( GTK_PANED(file_browser),
                   file_browser->side_pane, FALSE, TRUE );
}

void ptk_file_browser_show_side_pane( PtkFileBrowser* file_browser,
                                      PtkFileBrowserSidePaneMode mode )
{
  file_browser->side_pane_mode = mode;
  if( ! file_browser->side_pane ) {
    ptk_file_browser_create_side_pane( file_browser );
    if( file_browser->list_filter ) {
      side_pane_chdir( file_browser, ptk_file_browser_get_cwd(file_browser) );
    }

    switch( mode )
    {
      case FB_SIDE_PANE_BOOKMARKS:
        gtk_toggle_tool_button_set_active(file_browser->location_btn, TRUE);
        break;
      case FB_SIDE_PANE_DIR_TREE:
        gtk_toggle_tool_button_set_active(file_browser->dir_tree_btn, TRUE);
        break;
    }
  }
  gtk_widget_show( file_browser->side_pane );
  file_browser->show_side_pane = TRUE;
}

void ptk_file_browser_hide_side_pane( PtkFileBrowser* file_browser)
{
  if( file_browser->side_pane )  {
    file_browser->show_side_pane = FALSE;
    gtk_widget_destroy( file_browser->side_pane );
    file_browser->side_pane = NULL;
    file_browser->side_view = NULL;
    file_browser->side_view_scroll = NULL;
  }
}

gboolean ptk_file_browser_is_side_pane_visible( PtkFileBrowser* file_browser)
{
  return file_browser->show_side_pane;
}
