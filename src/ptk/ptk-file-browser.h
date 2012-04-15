#ifndef  _FILE_BROWSER_H_
#define  _FILE_BROWSER_H_

#include <gtk/gtk.h>
#include "vfs-dir.h"
#include <sys/types.h>

#include "ptk-icon-view.h"

G_BEGIN_DECLS

#define PTK_TYPE_FILE_BROWSER             (ptk_file_browser_get_type())
#define PTK_FILE_BROWSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  PTK_TYPE_FILE_BROWSER, PtkFileBrowser))
#define PTK_FILE_BROWSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  PTK_TYPE_FILE_BROWSER, PtkFileBrowserClass))
#define PTK_IS_FILE_BROWSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_FILE_BROWSER))
#define PTK_IS_FILE_BROWSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  PTK_TYPE_FILE_BROWSER))
#define PTK_FILE_BROWSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  PTK_TYPE_FILE_BROWSER, PtkFileBrowserClass))

typedef enum{
    FBVM_ICON_VIEW,
    FBVM_LIST_VIEW
}PtkFileBrowserViewMode;

typedef enum{
    FB_SIDE_PANE_BOOKMARKS,
    FB_SIDE_PANE_DIR_TREE
}PtkFileBrowserSidePaneMode;

typedef enum{
    FB_SORT_BY_NAME = 0,
    FB_SORT_BY_SIZE,
    FB_SORT_BY_MTIME,
    FB_SORT_BY_TYPE,
    FB_SORT_BY_PERM,
    FB_SORT_BY_OWNER
}FileBrowserSortOrder;

typedef struct _PtkFileBrowser PtkFileBrowser;
typedef struct _PtkFileBrowserClass PtkFileBrowserClass;

struct _PtkFileBrowser
{
    /* parent class */
    GtkHPaned parent;

    /* <private> */
    GList* history;
    GList* curHistory;
    int historyNum;

    VFSDir* dir;
    GtkTreeModel* file_list;

    gboolean show_hidden_files;
    int max_thumbnail;

    FileBrowserSortOrder sort_order;
    GtkSortType sort_type;

    int n_sel_files;
    off_t sel_size;

    /* side pane */
    GtkToggleToolButton* location_btn;
    GtkToggleToolButton* dir_tree_btn;

    gboolean show_side_pane;
    PtkFileBrowserSidePaneMode side_pane_mode;

    GtkWidget* side_pane;
    GtkTreeView* side_view;
    GtkWidget* side_view_scroll;

    /* folder view */
    GtkWidget* folder_view;
    GtkWidget* folder_view_scroll;
    PtkFileBrowserViewMode view_mode;

    gboolean busy;
    glong prev_update_time;
    guint update_timeout;
};

typedef enum{
    PTK_OPEN_DIR,
    PTK_OPEN_NEW_TAB,
    PTK_OPEN_NEW_WINDOW,
    PTK_OPEN_TERMINAL,
    PTK_OPEN_FILE
}PtkOpenAction;

struct _PtkFileBrowserClass
{
    GtkPanedClass parent;

    /* Default signal handlers */
    void ( *before_chdir ) ( PtkFileBrowser* file_browser, const char* path, gboolean* cancel );
    void ( *after_chdir ) ( PtkFileBrowser* file_browser );
    void ( *open_item ) ( PtkFileBrowser* file_browser, const char* path, int action );
    void ( *content_change ) ( PtkFileBrowser* file_browser );
    void ( *sel_change ) ( PtkFileBrowser* file_browser );
    void ( *pane_mode_change ) ( PtkFileBrowser* file_browser );
};

GType ptk_file_browser_get_type ( void );

GtkWidget* ptk_file_browser_new( GtkWidget* mainWindow,
                                 PtkFileBrowserViewMode view_mode );

/*
* folder_path should be encodede in on-disk encoding
*/
gboolean ptk_file_browser_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path,
                                 gboolean add_history );

/*
* returned path should be encodede in on-disk encoding
*/
const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser );

/*
* returned path should be encodede in UTF-8
*/
const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser );


guint ptk_file_browser_get_n_all_files( PtkFileBrowser* file_browser );
guint ptk_file_browser_get_n_visible_files( PtkFileBrowser* file_browser );

guint ptk_file_browser_get_n_sel( PtkFileBrowser* file_browser,
                                  guint64* sel_size );

gboolean ptk_file_browser_can_back( PtkFileBrowser* file_browser );
void ptk_file_browser_go_back( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_can_forward( PtkFileBrowser* file_browser );
void ptk_file_browser_go_forward( PtkFileBrowser* file_browser );

void ptk_file_browser_refresh( PtkFileBrowser* file_browser );
void ptk_file_browser_update_mime_icons( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_is_busy( PtkFileBrowser* file_browser );

GtkWidget* ptk_file_browser_get_folder_view( PtkFileBrowser* file_browser );
GtkTreeView* ptk_file_browser_get_side_view( PtkFileBrowser* file_browser );

void ptk_file_browser_show_hidden_files( PtkFileBrowser* file_browser,
                                         gboolean show );

/* Side pane */
void ptk_file_browser_set_side_pane_mode( PtkFileBrowser* file_browser,
                                          PtkFileBrowserSidePaneMode mode );
PtkFileBrowserSidePaneMode ptk_file_browser_get_side_pane_mode( PtkFileBrowser* file_browser );

void ptk_file_browser_show_side_pane( PtkFileBrowser* file_browser,
                                      PtkFileBrowserSidePaneMode mode );
void ptk_file_browser_hide_side_pane( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_is_side_pane_visible( PtkFileBrowser* file_browser );


/* Sorting files */
void ptk_file_browser_set_sort_order( PtkFileBrowser* file_browser,
                                      FileBrowserSortOrder order );

void ptk_file_browser_set_sort_type( PtkFileBrowser* file_browser,
                                     GtkSortType order );

FileBrowserSortOrder ptk_file_browser_get_sort_order( PtkFileBrowser* file_browser );

GtkSortType ptk_file_browser_get_sort_type( PtkFileBrowser* file_browser );

GList* ptk_file_browser_get_selected_files( PtkFileBrowser* file_browser );

/* Return a list of selected filenames (full paths in on-disk encoding) */
void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_can_paste( PtkFileBrowser* file_browser );
void ptk_file_browser_paste( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_can_cut_or_copy( PtkFileBrowser* file_browser );
void ptk_file_browser_cut( PtkFileBrowser* file_browser );
void ptk_file_browser_copy( PtkFileBrowser* file_browser );

gboolean ptk_file_browser_can_delete( PtkFileBrowser* file_browser );
void ptk_file_browser_delete( PtkFileBrowser* file_browser );

void ptk_file_browser_select_all( PtkFileBrowser* file_browser );
void ptk_file_browser_invert_selection( PtkFileBrowser* file_browser );

void ptk_file_browser_rename_selected_file( PtkFileBrowser* file_browser );

void ptk_file_browser_file_properties( PtkFileBrowser* file_browser );

void ptk_file_browser_view_as_icons( PtkFileBrowser* file_browser );
void ptk_file_browser_view_as_list ( PtkFileBrowser* file_browser );

void ptk_file_browser_create_new_file( PtkFileBrowser* file_browser,
                                       gboolean create_folder );
void ptk_file_browser_open_terminal( PtkFileBrowser* file_browser );

void ptk_file_browser_show_thumbnails( PtkFileBrowser* file_browser,
                                       int max_file_size );

void ptk_file_browser_update_display( PtkFileBrowser* file_browser );

void ptk_file_browser_emit_open( PtkFileBrowser* file_browser,
                                 const char* path,
                                 PtkOpenAction action );

G_END_DECLS

#endif
