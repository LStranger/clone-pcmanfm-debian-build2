#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <string.h>

#include <sys/types.h>
#include <fcntl.h>

#include "ptk-file-browser.h"

#include "main-window.h"
#include "ptk-utils.h"

#include "glade-support.h"

#include "pref-dialog.h"
#include "ptk-bookmarks.h"
#include "edit-bookmarks.h"
#include "file-properties.h"
#include "file-assoc-dlg.h"

#include "settings.h"

static void fm_main_window_class_init( FMMainWindowClass* klass );
static void fm_main_window_init( FMMainWindow* main_window );
static void fm_main_window_finalize( GObject *obj );
static void fm_main_window_get_property ( GObject *obj,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec );
static void fm_main_window_set_property ( GObject *obj,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec );

static GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window );
static GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                               const char* item );

static gboolean fm_main_window_delete_event ( GtkWidget *widget,
                                              GdkEvent *event );
static void fm_main_window_destroy ( GtkObject *object );

static gboolean fm_main_window_key_press_event ( GtkWidget *widget,
                                                 GdkEventKey *event );


/* Signal handlers */
static void on_back_activate ( GtkToolButton *toolbutton,
                               FMMainWindow *main_window );
static void on_forward_activate ( GtkToolButton *toolbutton,
                                  FMMainWindow *main_window );
static void on_up_activate ( GtkToolButton *toolbutton,
                             FMMainWindow *main_window );
static void on_home_activate( GtkToolButton *toolbutton,
                              FMMainWindow *main_window );
static void on_refresh_activate ( GtkToolButton *toolbutton,
                                  gpointer user_data );
static void on_quit_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_new_folder_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_text_file_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_preference_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_file_assoc_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_about_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
static gboolean on_back_btn_popup_menu ( GtkWidget *widget,
                                         gpointer user_data );
static gboolean on_forward_btn_popup_menu ( GtkWidget *widget,
                                            gpointer user_data );
static void on_address_bar_activate ( GtkWidget *entry,
                                      gpointer user_data );
static void on_new_window_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_tab_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data );
static void on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                             GtkNotebookPage *page,
                                             guint page_num,
                                             gpointer user_data );
static void on_cut_activate ( GtkMenuItem *menuitem,
                              gpointer user_data );
static void on_copy_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_paste_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
static void on_delete_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
static void on_select_all_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_invert_selection_activate ( GtkMenuItem *menuitem,
                                           gpointer user_data );
static void on_close_tab_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data );
static void on_rename_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
#if 0
static void on_fullscreen_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#endif
static void on_show_hidden_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data );
static void on_sort_by_name_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_size_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_ascending_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_sort_descending_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_view_as_icon_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_view_as_list_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_open_side_pane_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_show_dir_tree ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_show_loation_pane ( GtkMenuItem *menuitem,
                                   gpointer user_data );
static void on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                                   gpointer user_data );
static void on_go_btn_clicked ( GtkToolButton *toolbutton,
                                gpointer user_data );
static void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                             gpointer user_data );
static void on_file_properties_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data );


static void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                          const char* path, gboolean* cancel,
                                          FMMainWindow* main_window );
static void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                       const char* path, PtkOpenAction action,
                                       FMMainWindow* main_window );
static void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window );
static void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                            FMMainWindow* main_window );
static void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                        FMMainWindow* main_window );
static void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
                                              FMMainWindow* main_window );
static void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                                 GParamSpec *param,
                                                 FMMainWindow* main_window );

static gboolean on_tab_drag_motion ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     gint x,
                                     gint y,
                                     guint time,
                                     PtkFileBrowser* file_browser );

static void on_view_menu_popup( GtkMenuItem* item,
                                FMMainWindow* main_window );


/* Callback called when the bookmarks change */
static void on_bookmarks_change( PtkBookmarks* bookmarks,
                                 FMMainWindow* main_window );


/* Utilities */
static void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );
static void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );

/* Automatically process busy cursor */
static void fm_main_window_start_busy_task( FMMainWindow* main_window );
static gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window );


static GtkWindowClass *parent_class = NULL;

static int n_windows = 0;
static GList* all_windows = NULL;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

GType fm_main_window_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( FMMainWindowClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) fm_main_window_class_init,
                NULL,
                NULL,
                sizeof ( FMMainWindow ),
                0,
                ( GInstanceInitFunc ) fm_main_window_init,
                NULL,
            };
        type = g_type_register_static ( GTK_TYPE_WINDOW, "FMMainWindow", &info, 0 );
    }
    return type;
}

void fm_main_window_class_init( FMMainWindowClass* klass )
{
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = fm_main_window_set_property;
    object_class->get_property = fm_main_window_get_property;
    object_class->finalize = fm_main_window_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );
    widget_class->delete_event = fm_main_window_delete_event;
    widget_class->key_press_event = fm_main_window_key_press_event;
}


/* Main menu definition */

static PtkMenuItemEntry fm_file_create_new_manu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_new_folder_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_new_text_file_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_file_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "New _Window" ), "gtk-add", on_new_window_activate, GDK_N, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_( "New _Tab" ), "gtk-add", on_new_tab_activate, GDK_T, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Close Tab" ), "gtk-close", on_close_tab_activate, GDK_W, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", &fm_file_create_new_manu ),
        PTK_IMG_MENU_ITEM( N_( "File _Properties" ), "gtk-info", on_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( "gtk-quit", on_quit_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_edit_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_copy_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_rename_activate, GDK_F2, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Select _All" ), on_select_all_activate, GDK_A, GDK_CONTROL_MASK ),
        PTK_MENU_ITEM( N_( "_Invert Selection" ), on_invert_selection_activate, GDK_I, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        // PTK_IMG_MENU_ITEM( N_( "_File Associations" ), "gtk-execute", on_file_assoc_activate , 0, 0 ),
        PTK_STOCK_MENU_ITEM( "gtk-preferences", on_preference_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_go_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Go _Back" ), "gtk-go-back", on_back_activate, GDK_Left, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Forward" ), "gtk-go-forward", on_forward_activate, GDK_Right, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go to _Parent Folder" ), "gtk-go-up", on_up_activate, GDK_Up, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Home" ), "gtk-home", on_home_activate, GDK_Home, GDK_MOD1_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_help_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-about", on_about_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_sort_menu[] =
    {
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Name" ), on_sort_by_name_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Size" ), on_sort_by_size_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Modification Time" ), on_sort_by_mtime_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Type" ), on_sort_by_type_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Permission" ), on_sort_by_perm_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Owner" ), on_sort_by_owner_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Ascending" ), on_sort_ascending_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Descending" ), on_sort_descending_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_side_pane_menu[] =
    {
        PTK_CHECK_MENU_ITEM( N_( "_Open Side Pane" ), on_open_side_pane_activate, GDK_F7, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Show _Location Pane" ), on_show_loation_pane, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Show _Directory Tree" ), on_show_dir_tree, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_view_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Refresh" ), "gtk-refresh", on_refresh_activate, GDK_F5, 0 ),
        PTK_POPUP_MENU( N_( "Side _Pane" ), &fm_side_pane_menu ),
        /* PTK_CHECK_MENU_ITEM( N_( "_Full Screen" ), on_fullscreen_activate, GDK_F11, 0 ), */
        PTK_SEPARATOR_MENU_ITEM,
        PTK_CHECK_MENU_ITEM( N_( "Show _Hidden Files" ), on_show_hidden_activate, GDK_H, GDK_CONTROL_MASK ),
        PTK_POPUP_MENU( N_( "_Sort..." ), &fm_sort_menu ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "View as _Icons" ), on_view_as_icon_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "View as Detailed _List" ), on_view_as_list_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_tool_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Open _Terminal" ), GTK_STOCK_EXECUTE, on_open_terminal_activate, GDK_F4, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Open Current Folder as _Root" ),
                           GTK_STOCK_DIALOG_AUTHENTICATION,
                           on_open_current_folder_as_root, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_menu_bar[] =
    {
        PTK_POPUP_MENU( N_( "_File" ), fm_file_menu ),
        PTK_POPUP_MENU( N_( "_Edit" ), fm_edit_menu ),
        PTK_POPUP_MENU( N_( "_Go" ), fm_go_menu ),
        PTK_MENU_ITEM( N_( "_Bookmark" ), NULL, 0, 0 ),
        PTK_POPUP_MENU( N_( "_View" ), fm_view_menu ),
        PTK_POPUP_MENU( N_( "_Tool" ), fm_tool_menu ),
        PTK_POPUP_MENU( N_( "_Help" ), fm_help_menu ),
        PTK_MENU_END
    };

/* Toolbar items defiinition */

static PtkToolItemEntry fm_toolbar_btns[] =
    {
        PTK_TOOL_ITEM( N_( "New Tab" ), "gtk-add", N_( "New Tab" ), on_new_tab_activate ),
        PTK_TOOL_ITEM( N_( "Back" ), "gtk-go-back", N_( "Back" ), on_back_activate ),
        PTK_TOOL_ITEM( N_( "Forward" ), "gtk-go-forward", N_( "Forward" ), on_forward_activate ),
        PTK_TOOL_ITEM( N_( "Parent Folder" ), "gtk-go-up", N_( "Parent Folder" ), on_up_activate ),
        PTK_TOOL_ITEM( N_( "Refresh" ), "gtk-refresh", N_( "Refresh" ), on_refresh_activate ),
        PTK_TOOL_ITEM( N_( "Home Directory" ), "gtk-home", N_( "Home Directory" ), on_home_activate ),
        PTK_CHECK_TOOL_ITEM( N_( "Open Side Pane" ), "gtk-open", N_( "Open Side Pane" ), on_side_pane_toggled ),
        PTK_TOOL_END
    };

static PtkToolItemEntry fm_toolbar_jump_btn[] =
    {
        PTK_TOOL_ITEM( NULL, "gtk-jump-to", N_( "Go" ), on_go_btn_clicked ),
        PTK_TOOL_END
    };

void fm_main_window_init( FMMainWindow* main_window )
{
    GtkWidget * bookmark_menu;
    GtkWidget *view_menu_item;
    GtkToolItem *toolitem;
    GtkWidget *hbox;
    GtkLabel *label;

    /* Initialize parent class */
    GTK_WINDOW( main_window ) ->type = GTK_WINDOW_TOPLEVEL;

    /* Add to total window count */
    ++n_windows;
    all_windows = g_list_prepend( all_windows, main_window );

    /* Set a monitor for changes of the bookmarks */
    ptk_bookmarks_add_callback( ( GFunc ) on_bookmarks_change, main_window );

    /* Start building GUI */
    gtk_window_set_icon_name( GTK_WINDOW( main_window ),
                              "gnome-fs-directory" );

    main_window->main_vbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( main_window ), main_window->main_vbox );

    main_window->splitter_pos = appSettings.splitterPos;

    /* Create menu bar */
    main_window->menu_bar = gtk_menu_bar_new ();
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->menu_bar, FALSE, FALSE, 0 );

    main_window->accel_group = gtk_accel_group_new ();
    fm_side_pane_menu[ 0 ].ret = ( GtkMenuItem** ) & main_window->open_side_pane_menu;
    fm_side_pane_menu[ 2 ].ret = ( GtkMenuItem** ) & main_window->show_location_menu;
    fm_side_pane_menu[ 3 ].ret = ( GtkMenuItem** ) & main_window->show_dir_tree_menu;
    fm_view_menu[ 3 ].ret = ( GtkMenuItem** ) & main_window->show_hidden_files_menu;
    fm_view_menu[ 6 ].ret = ( GtkMenuItem** ) & main_window->view_as_icon;
    fm_view_menu[ 7 ].ret = ( GtkMenuItem** ) & main_window->view_as_list;
    fm_sort_menu[ 0 ].ret = ( GtkMenuItem** ) & main_window->sort_by_name;
    fm_sort_menu[ 1 ].ret = ( GtkMenuItem** ) & main_window->sort_by_size;
    fm_sort_menu[ 2 ].ret = ( GtkMenuItem** ) & main_window->sort_by_mtime;
    fm_sort_menu[ 3 ].ret = ( GtkMenuItem** ) & main_window->sort_by_type;
    fm_sort_menu[ 4 ].ret = ( GtkMenuItem** ) & main_window->sort_by_perm;
    fm_sort_menu[ 5 ].ret = ( GtkMenuItem** ) & main_window->sort_by_owner;
    fm_sort_menu[ 7 ].ret = ( GtkMenuItem** ) & main_window->sort_ascending;
    fm_sort_menu[ 8 ].ret = ( GtkMenuItem** ) & main_window->sort_descending;
    fm_menu_bar[ 3 ].ret = &main_window->bookmarks;
    fm_menu_bar[ 4 ].ret = &view_menu_item;
    ptk_menu_add_items_from_data( main_window->menu_bar, fm_menu_bar,
                                  main_window, main_window->accel_group );
    bookmark_menu = create_bookmarks_menu( main_window );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->bookmarks ),
                               bookmark_menu );
    g_signal_connect( view_menu_item, "activate",
                      G_CALLBACK( on_view_menu_popup ), main_window );

    /* Create toolbar */
    main_window->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style ( GTK_TOOLBAR ( main_window->toolbar ), GTK_TOOLBAR_ICONS );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->toolbar, FALSE, FALSE, 0 );

    main_window->tooltips = gtk_tooltips_new ();
    fm_toolbar_btns[ 1 ].ret = &main_window->back_btn;
    fm_toolbar_btns[ 2 ].ret = &main_window->forward_btn;
    fm_toolbar_btns[ 6 ].ret = &main_window->open_side_pane_btn;
    ptk_toolbar_add_items_from_data( GTK_TOOLBAR ( main_window->toolbar ),
                                     fm_toolbar_btns,
                                     main_window, main_window->tooltips );

    toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand ( toolitem, TRUE );
    gtk_toolbar_insert( main_window->toolbar, toolitem, -1 );

    hbox = gtk_hbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( toolitem ), hbox );

    /* FIXME: Is this label required? What about the hotkey? */
    label = ( GtkLabel* ) gtk_label_new_with_mnemonic ( _( "A_ddress:" ) );
    gtk_box_pack_start ( GTK_BOX ( hbox ), ( GtkWidget* ) label, FALSE, FALSE, 0 );
    gtk_label_set_justify ( label, GTK_JUSTIFY_FILL );
    gtk_misc_set_padding ( GTK_MISC ( label ), 6, 0 );

    /*
    gtk_box_pack_start ( GTK_BOX ( hbox ), gtk_separator_tool_item_new(),
                         FALSE, FALSE, 0 );
    */

    main_window->address_bar = ( GtkEntry* ) gtk_entry_new ();
    g_signal_connect( main_window->address_bar, "activate",
                      on_address_bar_activate, main_window );
    gtk_box_pack_start ( GTK_BOX ( hbox ), main_window->address_bar, TRUE, TRUE, 0 );
    ptk_toolbar_add_items_from_data( GTK_TOOLBAR ( main_window->toolbar ),
                                     fm_toolbar_jump_btn,
                                     main_window, main_window->tooltips );

    gtk_label_set_mnemonic_widget ( GTK_LABEL ( label ), main_window->address_bar );
    gtk_window_add_accel_group ( GTK_WINDOW ( main_window ), main_window->accel_group );
    gtk_widget_grab_focus ( GTK_WIDGET( main_window->address_bar ) );

    /* Create warning bar for super user */
    if ( geteuid() == 0 )               /* Run as super user! */
    {
        main_window->status_bar = gtk_event_box_new();
        gtk_widget_modify_bg( main_window->status_bar, GTK_STATE_NORMAL,
                              &main_window->status_bar->style->bg[ GTK_STATE_SELECTED ] );
        label = gtk_label_new ( _( "Warning: You are in super user mode" ) );
        gtk_misc_set_padding( GTK_MISC( label ), 0, 2 );
        gtk_widget_modify_fg( label, GTK_STATE_NORMAL,
                              &GTK_WIDGET( label ) ->style->fg[ GTK_STATE_SELECTED ] );
        gtk_container_add( GTK_CONTAINER( main_window->status_bar ), label );
        gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                             main_window->status_bar, FALSE, FALSE, 2 );
    }

    /* Create client area */
    main_window->notebook = gtk_notebook_new ();
    gtk_notebook_set_scrollable ( GTK_NOTEBOOK( main_window->notebook ), TRUE );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ), main_window->notebook, TRUE, TRUE, 0 );

    g_signal_connect ( main_window->notebook, "switch_page",
                       G_CALLBACK ( on_folder_notebook_switch_pape ), main_window );

    /* Create Status bar */
    main_window->status_bar = gtk_statusbar_new ();
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->status_bar, FALSE, FALSE, 0 );

    gtk_widget_show_all( main_window->main_vbox );
}

void fm_main_window_finalize( GObject *obj )
{
    all_windows = g_list_remove( all_windows, obj );
    --n_windows;

    /* Remove the monitor for changes of the bookmarks */
    ptk_bookmarks_remove_callback( ( GFunc ) on_bookmarks_change, obj );

    if ( 0 == n_windows )
        gtk_main_quit();
    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void fm_main_window_get_property ( GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec )
{}

void fm_main_window_set_property ( GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec )
{}

static void fm_main_window_close( GtkWidget* main_window )
{
    appSettings.width = GTK_WIDGET( main_window ) ->allocation.width;
    appSettings.height = GTK_WIDGET( main_window ) ->allocation.height;
    appSettings.splitterPos = FM_MAIN_WINDOW( main_window ) ->splitter_pos;
    gtk_widget_destroy( main_window );
}

gboolean
fm_main_window_delete_event ( GtkWidget *widget,
                              GdkEvent *event )
{
    fm_main_window_close( widget );
    return TRUE;
}

static void
on_close_notebook_page( GtkButton* btn, PtkFileBrowser* file_browser )
{
    GtkNotebook * notebook = gtk_widget_get_ancestor( GTK_WIDGET( file_browser ),
                                                      GTK_TYPE_NOTEBOOK );
    gtk_widget_destroy( GTK_WIDGET( file_browser ) );
    if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
        gtk_notebook_set_show_tabs( notebook, FALSE );
}


static GtkWidget* fm_main_window_create_tab_label( FMMainWindow* main_window,
                                                   PtkFileBrowser* file_browser )
{
    GtkEventBox * evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text;
    GtkWidget* tab_icon;
    GtkWidget* close_btn;

    /* Create tab label */
    evt_box = gtk_event_box_new ();
    gtk_event_box_set_visible_window ( GTK_EVENT_BOX( evt_box ), FALSE );

    tab_label = gtk_hbox_new( FALSE, 0 );
    tab_icon = gtk_image_new_from_icon_name ( "gnome-fs-directory",
                                              GTK_ICON_SIZE_MENU );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                        tab_icon, FALSE, FALSE, 4 );

    tab_text = gtk_label_new( "" );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                        tab_text, FALSE, FALSE, 4 );

    close_btn = gtk_button_new ();
    gtk_button_set_focus_on_click ( GTK_BUTTON ( close_btn ), FALSE );
    gtk_button_set_relief( GTK_BUTTON ( close_btn ), GTK_RELIEF_NONE );
    gtk_container_add ( GTK_CONTAINER ( close_btn ),
                        gtk_image_new_from_icon_name( "gtk-close", GTK_ICON_SIZE_MENU ) );
    gtk_widget_set_size_request ( close_btn, 20, 20 );
    gtk_box_pack_start ( GTK_BOX( tab_label ),
                         close_btn, FALSE, FALSE, 0 );
    g_signal_connect( G_OBJECT( close_btn ), "clicked",
                      G_CALLBACK( on_close_notebook_page ), file_browser );

    gtk_container_add ( GTK_CONTAINER ( evt_box ), tab_label );

    gtk_widget_set_events ( evt_box, GDK_ALL_EVENTS_MASK );
    gtk_drag_dest_set ( evt_box, GTK_DEST_DEFAULT_ALL,
                        drag_targets,
                        sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                        GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
    g_signal_connect ( ( gpointer ) evt_box, "drag-motion",
                       G_CALLBACK ( on_tab_drag_motion ),
                       file_browser );

    gtk_widget_show_all( evt_box );
    return evt_box;
}

void fm_main_window_add_new_tab( FMMainWindow* main_window,
                                 const char* folder_path,
                                 gboolean open_side_pane,
                                 PtkFileBrowserSidePaneMode side_pane_mode )
{
    GtkWidget * tab_label;
    gint idx;
    GtkNotebook* notebook = GTK_NOTEBOOK( main_window->notebook );
    PtkFileBrowser* file_browser = ptk_file_browser_new ( main_window,
                                                          appSettings.viewMode );
    gtk_paned_set_position ( GTK_PANED ( file_browser ), main_window->splitter_pos );
    ptk_file_browser_show_hidden_files( file_browser,
                                        appSettings.showHiddenFiles );
    ptk_file_browser_show_thumbnails( file_browser,
                                      appSettings.showThumbnail ? appSettings.maxThumbSize : 0 );
    if ( open_side_pane )
    {
        ptk_file_browser_show_side_pane( file_browser, side_pane_mode );
    }
    else
    {
        ptk_file_browser_hide_side_pane( file_browser );
    }
    ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
    ptk_file_browser_set_sort_type( file_browser, appSettings.sortType );

    gtk_widget_show( GTK_WIDGET( file_browser ) );

    g_signal_connect( file_browser, "before-chdir",
                      G_CALLBACK( on_file_browser_before_chdir ), main_window );
    g_signal_connect( file_browser, "after-chdir",
                      G_CALLBACK( on_file_browser_after_chdir ), main_window );
    g_signal_connect( file_browser, "open-item",
                      G_CALLBACK( on_file_browser_open_item ), main_window );
    g_signal_connect( file_browser, "content-change",
                      G_CALLBACK( on_file_browser_content_change ), main_window );
    g_signal_connect( file_browser, "sel-change",
                      G_CALLBACK( on_file_browser_sel_change ), main_window );
    g_signal_connect( file_browser, "pane-mode-change",
                      G_CALLBACK( on_file_browser_pane_mode_change ), main_window );
    g_signal_connect( file_browser, "notify::position",
                      G_CALLBACK( on_file_browser_splitter_pos_change ), main_window );

    tab_label = fm_main_window_create_tab_label( main_window, file_browser );
    idx = gtk_notebook_append_page( notebook, GTK_WIDGET( file_browser ), tab_label );
    gtk_notebook_set_current_page ( notebook, idx );

    if ( gtk_notebook_get_n_pages ( notebook ) > 1 )
        gtk_notebook_set_show_tabs( notebook, TRUE );
    else
        gtk_notebook_set_show_tabs( notebook, FALSE );

    ptk_file_browser_chdir( file_browser, folder_path, TRUE );

    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

GtkWidget* fm_main_window_new()
{
    return ( GtkWidget* ) g_object_new ( FM_TYPE_MAIN_WINDOW, NULL );
}

GtkWidget* fm_main_window_get_current_file_browser ( FMMainWindow* main_window )
{
    GtkNotebook * notebook = main_window->notebook;
    gint idx = gtk_notebook_get_current_page( notebook );
    return gtk_notebook_get_nth_page( notebook, idx );
}

void on_back_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = fm_main_window_get_current_file_browser( main_window );
    if ( file_browser && ptk_file_browser_can_back( file_browser ) )
        ptk_file_browser_go_back( file_browser );
}


void on_forward_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = fm_main_window_get_current_file_browser( main_window );
    if ( file_browser && ptk_file_browser_can_forward( file_browser ) )
        ptk_file_browser_go_forward( file_browser );
}


void on_up_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    char * parent_dir;
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );

    parent_dir = g_path_get_dirname( ptk_file_browser_get_cwd( file_browser ) );
    ptk_file_browser_chdir( file_browser, parent_dir, TRUE );
    g_free( parent_dir );
}


void on_home_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    GtkWidget * file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_chdir( file_browser, g_get_home_dir(), TRUE );
}


void on_address_bar_activate ( GtkWidget *entry,
                               gpointer user_data )
{
    FMMainWindow * main_window = GTK_WIDGET( user_data );
    gchar* dir_path;
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );

    /* Convert to on-disk encoding */
    dir_path = g_filename_from_utf8( gtk_entry_get_text( entry ), -1,
                                     NULL, NULL, NULL );
    ptk_file_browser_chdir( file_browser, dir_path, TRUE );
    if ( dir_path )
        g_free( dir_path );
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

#if 0 
/* This is not needed by a file manager */
/* Patched by <cantona@cantona.net> */
void on_fullscreen_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    GtkWindow * window;
    int is_fullscreen;

    window = GTK_WINDOW( user_data );
    is_fullscreen = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );

    if ( is_fullscreen )
    {
        gtk_window_fullscreen( GTK_WINDOW( window ) );
    }
    else
    {
        gtk_window_unfullscreen( GTK_WINDOW( window ) );
    }
}
#endif

void on_refresh_activate ( GtkToolButton *toolbutton,
                           gpointer user_data )
{
    FMMainWindow * main_window = GTK_WIDGET( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_refresh( file_browser );
}


void
on_quit_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    fm_main_window_close( GTK_WIDGET( user_data ) );
}


void
on_new_folder_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = GTK_WIDGET( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( file_browser, TRUE );
}


void
on_new_text_file_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = GTK_WIDGET( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( file_browser, FALSE );
}


void
on_preference_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    int max_thumb = appSettings.maxThumbSize;
    gboolean show_thumbnails = appSettings.showThumbnail;
    int big_icon = appSettings.bigIconSize;
    int small_icon = appSettings.smallIconSize;
    int i, n;
    GList* l;
    PtkFileBrowser* file_browser;

    show_preference_dialog( GTK_WINDOW( main_window ) );
    if ( show_thumbnails != appSettings.showThumbnail
            || max_thumb != appSettings.maxThumbSize )
    {
        for ( l = all_windows; l; l = l->next )
        {
            main_window = FM_MAIN_WINDOW( l->data );
            n = gtk_notebook_get_n_pages( main_window->notebook );
            for ( i = 0; i < n; ++i )
            {
                file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                     main_window->notebook, i ) );
                ptk_file_browser_show_thumbnails( file_browser,
                                                  appSettings.showThumbnail ? appSettings.maxThumbSize : 0 );
            }
        }
    }
    if ( big_icon != appSettings.bigIconSize
            || small_icon != appSettings.smallIconSize )
    {
        vfs_mime_type_set_icon_size( appSettings.bigIconSize, appSettings.smallIconSize );
        vfs_file_info_set_thumbnail_size( appSettings.bigIconSize, appSettings.smallIconSize );
        for ( l = all_windows; l; l = l->next )
        {
            main_window = FM_MAIN_WINDOW( l->data );
            n = gtk_notebook_get_n_pages( main_window->notebook );
            for ( i = 0; i < n; ++i )
            {
                file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                     main_window->notebook, i ) );
                ptk_file_browser_update_display( file_browser );
            }
        }
    }
}

void
on_file_assoc_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    GtkWindow * main_window = GTK_WINDOW( user_data );
    edit_file_associations( main_window );
}

void
on_about_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    GtkWidget * aboutDialog;
    const gchar *authors[] =
        {
            "洪任諭 Hong Jen Yee  <pcman.tw@gmail.com>",
            NULL
        };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _( "translator-credits" );

    aboutDialog = gtk_about_dialog_new ();
    gtk_container_set_border_width ( GTK_CONTAINER ( aboutDialog ), 2 );
    gtk_about_dialog_set_version ( GTK_ABOUT_DIALOG ( aboutDialog ), VERSION );
    gtk_about_dialog_set_name ( GTK_ABOUT_DIALOG ( aboutDialog ), _( "PCMan File Manager" ) );
    gtk_about_dialog_set_copyright ( GTK_ABOUT_DIALOG ( aboutDialog ), _( "Copyright (C) 2005 - 2006" ) );
    gtk_about_dialog_set_comments ( GTK_ABOUT_DIALOG ( aboutDialog ), _( "Lightweight file manager\n\nDeveloped by Hon Jen Yee (PCMan)" ) );
    gtk_about_dialog_set_license ( GTK_ABOUT_DIALOG ( aboutDialog ), "PCMan File Manager\n\nCopyright (C) 2005 - 2006 Hong Jen Yee (PCMan)\n\nThis program is free software; you can redistribute it and/or\nmodify it under the terms of the GNU General Public License\nas published by the Free Software Foundation; either version 2\nof the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful,\nbut WITHOUT ANY WARRANTY; without even the implied warranty of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\nGNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License\nalong with this program; if not, write to the Free Software\nFoundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA." );
    gtk_about_dialog_set_website ( GTK_ABOUT_DIALOG ( aboutDialog ), "http://pcmanfm.sourceforge.net/" );
    gtk_about_dialog_set_authors ( GTK_ABOUT_DIALOG ( aboutDialog ), authors );
    gtk_about_dialog_set_translator_credits ( GTK_ABOUT_DIALOG ( aboutDialog ), translators );
    gtk_window_set_transient_for( GTK_WINDOW( aboutDialog ), GTK_WINDOW( user_data ) );

    gtk_dialog_run( GTK_DIALOG( aboutDialog ) );
    gtk_widget_destroy( aboutDialog );
}


gboolean
on_back_btn_popup_menu ( GtkWidget *widget,
                         gpointer user_data )
{
    /*
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    */ 
    return FALSE;
}


gboolean
on_forward_btn_popup_menu ( GtkWidget *widget,
                            gpointer user_data )
{
    /*
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    */ 
    return FALSE;
}

void fm_main_window_add_new_window( FMMainWindow* main_window,
                                    const char* path,
                                    gboolean open_side_pane,
                                    PtkFileBrowserSidePaneMode side_pane_mode )
{
    GtkWidget * new_win = fm_main_window_new();
    FM_MAIN_WINDOW( new_win ) ->splitter_pos = main_window->splitter_pos;
    gtk_window_set_default_size( GTK_WINDOW( new_win ),
                                 GTK_WIDGET( main_window ) ->allocation.width,
                                 GTK_WIDGET( main_window ) ->allocation.height );
    gtk_widget_show( new_win );
    fm_main_window_add_new_tab( FM_MAIN_WINDOW( new_win ), path,
                                open_side_pane, side_pane_mode );
}

void
on_new_window_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    /*
    * FIXME: There sould be an option to let the users choose wether
    * home dir or current dir should be opened in new windows and new tabs.
    */
    PtkFileBrowser * file_browser;
    const char* path;
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    file_browser = fm_main_window_get_current_file_browser( main_window );
    path = file_browser ? ptk_file_browser_get_cwd( file_browser ) : g_get_home_dir();
    fm_main_window_add_new_window( main_window, path,
                                   appSettings.showSidePane,
                                   file_browser->side_pane_mode );
}

void
on_new_tab_activate ( GtkMenuItem *menuitem,
                      gpointer user_data )
{
    /*
    * FIXME: There sould be an option to let the users choose wether
    * home dir or current dir should be opened in new windows and new tabs.
    */
    PtkFileBrowser * file_browser;
    const char* path;
    gboolean show_side_pane;
    PtkFileBrowserSidePaneMode side_pane_mode;

    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    file_browser = fm_main_window_get_current_file_browser( main_window );

    if ( file_browser )
    {
        path = ptk_file_browser_get_cwd( file_browser );
        show_side_pane = ptk_file_browser_is_side_pane_visible( file_browser );
        side_pane_mode = ptk_file_browser_get_side_pane_mode( file_browser );
    }
    else
    {
        path = g_get_home_dir();
        show_side_pane = appSettings.showSidePane;
        side_pane_mode = appSettings.sidePaneMode;
    }

    fm_main_window_add_new_tab( main_window, path,
                                show_side_pane, side_pane_mode );
}

static gboolean delayed_focus( GtkWidget* widget )
{
    gtk_widget_grab_focus( widget );
    return FALSE;
}

void
on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                 GtkNotebookPage *page,
                                 guint page_num,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* path;
    char* disp_path;

    file_browser = ( PtkFileBrowser* ) gtk_notebook_get_nth_page( notebook, page_num );

    fm_main_window_update_command_ui( main_window, file_browser );
    fm_main_window_update_status_bar( main_window, file_browser );

    gtk_paned_set_position ( GTK_PANED ( file_browser ), main_window->splitter_pos );

    path = ptk_file_browser_get_cwd( file_browser );
    if ( path )
    {
        disp_path = g_filename_display_name( path );
        gtk_entry_set_text( main_window->address_bar, disp_path );
        gtk_window_set_title( GTK_WINDOW( main_window ), disp_path );
        g_free( disp_path );
    }
    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}


void
on_cut_activate ( GtkMenuItem *menuitem,
                  gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    if ( gtk_widget_is_focus ( main_window->address_bar ) )
    {
        gtk_editable_cut_clipboard( GTK_EDITABLE( main_window->address_bar ) );
        return ;
    }
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_cut( file_browser );
}


void
on_copy_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    if ( gtk_widget_is_focus ( main_window->address_bar ) )
    {
        gtk_editable_copy_clipboard( GTK_EDITABLE( main_window->address_bar ) );
        return ;
    }
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_copy( file_browser );
}


void
on_paste_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    if ( gtk_widget_is_focus ( main_window->address_bar ) )
    {
        gtk_editable_paste_clipboard( GTK_EDITABLE( main_window->address_bar ) );
        return ;
    }
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_paste( file_browser );
}


void
on_delete_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;

    if ( gtk_widget_is_focus ( main_window->address_bar ) )
    {
        GtkEditable* edit;
        int pos;

        edit = GTK_EDITABLE( main_window->address_bar );
        if( gtk_editable_get_selection_bounds( edit, NULL, NULL ) )
            gtk_editable_delete_selection( edit );
        else
        {
            pos = gtk_editable_get_position(edit);
            gtk_editable_delete_text( edit, pos, pos+1 );
        }
        return ;
    }
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_delete( file_browser );
}


void
on_select_all_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_select_all( file_browser );
}

void
on_edit_bookmark_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* bookmark;
    GtkWidget* bookmark_menu;

    edit_bookmarks( GTK_WINDOW( main_window ) );
}

int bookmark_item_comp( const char* item, const char* path )
{
    return strcmp( ptk_bookmarks_item_get_path( item ), path );
}

void
on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    GList * l;
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* bookmark;
    GtkWidget* bookmark_menu;
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    const char* path = ptk_file_browser_get_cwd( file_browser );
    gchar* name;

    if ( ! g_list_find_custom( appSettings.bookmarks->list,
                               path,
                               ( GCompareFunc ) bookmark_item_comp ) )
    {
        name = g_path_get_basename( path );
        ptk_bookmarks_append( name, path );
        g_free( name );
    }
}

void
on_invert_selection_activate ( GtkMenuItem *menuitem,
                               gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_invert_selection( file_browser );
}


void
on_close_tab_activate ( GtkMenuItem *menuitem,
                        gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkNotebook* notebook = main_window->notebook;
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    gint idx;

    if ( gtk_notebook_get_n_pages ( notebook ) <= 1 )
    {
        fm_main_window_close( main_window );
        return ;
    }
    idx = gtk_notebook_page_num ( GTK_NOTEBOOK( notebook ),
                                  file_browser );
    gtk_notebook_remove_page( notebook, idx );
    if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
        gtk_notebook_set_show_tabs( notebook, FALSE );
}


void
on_rename_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_rename_selected_file( file_browser );
}


void
on_open_side_pane_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkCheckMenuItem* check = GTK_CHECK_MENU_ITEM( menuitem );
    PtkFileBrowser* file_browser;
    GtkNotebook* nb = main_window->notebook;
    GtkToggleToolButton* btn = main_window->open_side_pane_btn;
    int i;
    int n = gtk_notebook_get_n_pages( nb );

    appSettings.showSidePane = gtk_check_menu_item_get_active( check );
    g_signal_handlers_block_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( btn, appSettings.showSidePane );
    g_signal_handlers_unblock_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( nb, i ) );
        if ( appSettings.showSidePane )
        {
            ptk_file_browser_show_side_pane( file_browser,
                                             file_browser->side_pane_mode );
        }
        else
        {
            ptk_file_browser_hide_side_pane( file_browser );
        }
    }
}


void on_show_dir_tree ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    appSettings.sidePaneMode = FB_SIDE_PANE_DIR_TREE;

    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, FB_SIDE_PANE_DIR_TREE );
    }
}

void on_show_loation_pane ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    appSettings.sidePaneMode = FB_SIDE_PANE_BOOKMARKS;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, FB_SIDE_PANE_BOOKMARKS );
    }
}

void
on_show_hidden_activate ( GtkMenuItem *menuitem,
                          gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.showHiddenFiles = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );
    if ( file_browser )
        ptk_file_browser_show_hidden_files( file_browser,
                                            appSettings.showHiddenFiles );
}


void
on_sort_by_name_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_NAME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}


void
on_sort_by_size_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_SIZE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}


void
on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_MTIME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}

void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_TYPE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}

void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_PERM;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}

void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortOrder = FB_SORT_BY_OWNER;
    if ( file_browser )
        ptk_file_browser_set_sort_order( file_browser, appSettings.sortOrder );
}

void
on_sort_ascending_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortType = GTK_SORT_ASCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( file_browser, appSettings.sortType );
}


void
on_sort_descending_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.sortType = GTK_SORT_DESCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( file_browser, appSettings.sortType );
}


void
on_view_as_icon_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.viewMode = FBVM_ICON_VIEW;
    if ( file_browser && GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        ptk_file_browser_view_as_icons( file_browser );
}


void
on_view_as_list_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    appSettings.viewMode = FBVM_LIST_VIEW;
    if ( file_browser )
        ptk_file_browser_view_as_list( file_browser );
}


void
on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                       gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    !appSettings.showSidePane );
}

void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                const char* path, PtkOpenAction action,
                                FMMainWindow* main_window )
{
    char** argv = NULL;
    int argc = 0;

    if ( G_LIKELY( path ) )
    {
        switch ( action )
        {
        case PTK_OPEN_NEW_TAB:
            file_browser = fm_main_window_get_current_file_browser( main_window );
            fm_main_window_add_new_tab( main_window, path,
                                        file_browser->show_side_pane,
                                        file_browser->side_pane_mode );
            break;
        case PTK_OPEN_NEW_WINDOW:
            file_browser = fm_main_window_get_current_file_browser( main_window );
            fm_main_window_add_new_window( main_window, path,
                                           file_browser->show_side_pane,
                                           file_browser->side_pane_mode );
            break;
        case PTK_OPEN_TERMINAL:
            if ( !appSettings.terminal )
            {
                ptk_show_error( GTK_WINDOW( main_window ), _( "Terminal program has not been set" ) );
                show_preference_dialog( GTK_WINDOW( main_window ) );
            }
            if ( appSettings.terminal )
            {
							  if( g_shell_parse_argv( appSettings.terminal, 
									                      &argc, &argv, NULL ) )
								{
									g_spawn_async( path, argv, NULL, G_SPAWN_SEARCH_PATH,
																 NULL, NULL, NULL, NULL );
									g_strfreev( argv );
								}
            }
            break;
        case PTK_OPEN_FILE:
            fm_main_window_start_busy_task( main_window );
            g_timeout_add( 1000, ( GSourceFunc ) fm_main_window_stop_busy_task, main_window );
            break;
        }
    }
}

void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_open_terminal( file_browser );
}

void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* cwd;
    char* argv[ 4 ];
    char* su = g_find_program_in_path( "gksu" );
    if ( ! su )
        su = g_find_program_in_path( "kdesu" );
    if ( ! su )
    {
        ptk_show_error( main_window, _( "gksu or kdesu is not found" ) );
        return ;
    }
    file_browser = fm_main_window_get_current_file_browser( main_window );
    cwd = ptk_file_browser_get_cwd( file_browser );
    argv[ 0 ] = su;
    argv[ 1 ] = g_get_prgname();
    argv[ 2 ] = cwd;
    argv[ 3 ] = NULL;
    g_spawn_async( NULL, argv, NULL,
                   G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                   NULL, NULL, NULL, NULL );
    g_free( su );
}

void on_file_properties_activate ( GtkMenuItem *menuitem,
                                   gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_file_properties( file_browser );
}


void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    char* path = ( char* ) g_object_get_data( menu, "path" );
    if ( !path )
        return ;

    switch ( appSettings.openBookmarkMethod )
    {
    case 1:              /* current tab */
        ptk_file_browser_chdir( file_browser, path, TRUE );
        break;
    case 3:              /* new window */
        fm_main_window_add_new_window( main_window, path,
                                       appSettings.showSidePane,
                                       appSettings.sidePaneMode );
        break;
    case 2:              /* new tab */
        fm_main_window_add_new_tab( main_window, path,
                                    appSettings.showSidePane,
                                    appSettings.sidePaneMode );
        break;
    }
}

void on_bookmarks_change( PtkBookmarks* bookmarks, FMMainWindow* main_window )
{
    GtkWidget * bookmarks_menu = create_bookmarks_menu( main_window );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( main_window->bookmarks ),
                                bookmarks_menu );
}

GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                        const char* item )
{
    GtkWidget * folder_image;
    GtkWidget* menu_item;
    const char* path;

    menu_item = gtk_image_menu_item_new_with_label( item );
    path = ptk_bookmarks_item_get_path( item );
    g_object_set_data( menu_item, "path", path );
    folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory",
                                                 GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ),
                                    folder_image );
    g_signal_connect( menu_item, "activate",
                      G_CALLBACK( on_bookmark_item_activate ), main_window );
    return menu_item;
}

static PtkMenuItemEntry fm_bookmarks_menu[] = {
                                                  PTK_IMG_MENU_ITEM( N_( "_Add to Bookmarks" ), "gtk-add", on_add_to_bookmark_activate, 0, 0 ),
                                                  PTK_IMG_MENU_ITEM( N_( "_Edit Bookmarks" ), "gtk-edit", on_edit_bookmark_activate, 0, 0 ),
                                                  PTK_SEPARATOR_MENU_ITEM,
                                                  PTK_MENU_END
                                              };

GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window )
{
    GtkWidget * bookmark_menu;
    GtkWidget* add_to_bookmark;
    GtkWidget* add_image;
    GtkWidget* edit_bookmark;
    GtkWidget* edit_image;
    GtkWidget* menu_item;
    GList* l;

    bookmark_menu = ptk_menu_new_from_data( fm_bookmarks_menu, main_window,
                                            main_window->accel_group );
    for ( l = appSettings.bookmarks->list; l; l = l->next )
    {
        menu_item = create_bookmarks_menu_item( main_window,
                                                ( char* ) l->data );
        gtk_menu_shell_append ( GTK_MENU_SHELL( bookmark_menu ), menu_item );
    }
    gtk_widget_show_all( bookmark_menu );
    return bookmark_menu;
}



void
on_go_btn_clicked ( GtkToolButton *toolbutton,
                    gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    on_address_bar_activate( main_window->address_bar, main_window );
}


gboolean
fm_main_window_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( widget );
    gint page;
    GdkModifierType mod = event->state & ( GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK );

    /* Process Alt 1~0 to switch among tabs */
    if ( mod == GDK_MOD1_MASK && event->keyval >= GDK_0 && event->keyval <= GDK_9 )
    {
        if ( event->keyval == GDK_0 )
            page = 9;
        else
            page = event->keyval - GDK_1;
        if ( page < gtk_notebook_get_n_pages( main_window->notebook ) )
            gtk_notebook_set_current_page( main_window->notebook, page );
        return TRUE;
    }
    return GTK_WIDGET_CLASS( parent_class ) ->key_press_event( widget, event );
}


void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                   const char* path, gboolean* cancel,
                                   FMMainWindow* main_window )
{
    GtkWidget * label;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;

    gchar* name;
    gchar* disp_path;

    fm_main_window_start_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == file_browser )
    {
        disp_path = g_filename_display_name( path );
        gtk_entry_set_text( main_window->address_bar, disp_path );
        g_free( disp_path );
        gtk_statusbar_push( main_window->status_bar, 0, _( "Loading..." ) );
    }

    label = gtk_notebook_get_tab_label ( main_window->notebook,
                                         GTK_WIDGET( file_browser ) );
    label = gtk_bin_get_child ( GTK_BIN( label ) );
    children = gtk_container_get_children( GTK_CONTAINER( label ) );
    icon = GTK_IMAGE( children->data );
    text = GTK_LABEL( children->next->data );
    g_list_free( children );

    /* TODO: Change the icon */

    name = g_path_get_basename( path );
    gtk_label_set_text( text, name );
    g_free( name );
}

void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                  FMMainWindow* main_window )
{
    GtkWidget * label;
    GtkContainer* hbox;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;

    fm_main_window_stop_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == file_browser )
    {
        gtk_window_set_title( GTK_WINDOW( main_window ),
                              ptk_file_browser_get_cwd( file_browser ) );
        gtk_widget_grab_focus( file_browser->folder_view );
        gtk_statusbar_push( main_window->status_bar, 0, "" );
        fm_main_window_update_command_ui( main_window, file_browser );
    }
    label = gtk_notebook_get_tab_label ( main_window->notebook,
                                         GTK_WIDGET( file_browser ) );
    hbox = GTK_CONTAINER( gtk_bin_get_child ( GTK_BIN( label ) ) );
    children = gtk_container_get_children( hbox );
    icon = GTK_IMAGE( children->data );
    text = GTK_LABEL( children->next->data );
    g_list_free( children );

    /* TODO: Change the icon */
}

void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    if ( ! file_browser )
        file_browser = fm_main_window_get_current_file_browser( main_window );

    g_signal_handlers_block_matched( main_window->show_hidden_files_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_show_hidden_activate, NULL );
    gtk_check_menu_item_set_active( main_window->show_hidden_files_menu,
                                    file_browser->show_hidden_files );
    g_signal_handlers_unblock_matched( main_window->show_hidden_files_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_show_hidden_activate, NULL );

    /* Open side pane */
    g_signal_handlers_block_matched( main_window->open_side_pane_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_open_side_pane_activate, NULL );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched( main_window->open_side_pane_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_open_side_pane_activate, NULL );

    g_signal_handlers_block_matched ( main_window->open_side_pane_btn,
                                      G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( main_window->open_side_pane_btn,
                                       ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched ( main_window->open_side_pane_btn,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    switch ( ptk_file_browser_get_side_pane_mode( file_browser ) )
    {
    case FB_SIDE_PANE_BOOKMARKS:
        g_signal_handlers_block_matched( main_window->show_location_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_loation_pane, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_location_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_location_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_loation_pane, NULL );
        break;
    case FB_SIDE_PANE_DIR_TREE:
        g_signal_handlers_block_matched( main_window->show_dir_tree_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_dir_tree, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_dir_tree_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_dir_tree, NULL );
        break;
    }

    switch ( file_browser->view_mode )
    {
    case FBVM_ICON_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_icon,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_icon_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_icon, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_icon,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_icon_activate, NULL );
        break;
    case FBVM_LIST_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_list,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_list_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_list, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_list,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_list_activate, NULL );
        break;
    }

    gtk_widget_set_sensitive( main_window->back_btn,
                              ptk_file_browser_can_back( file_browser ) );
    gtk_widget_set_sensitive( main_window->forward_btn,
                              ptk_file_browser_can_forward( file_browser ) );
}

void on_view_menu_popup( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser * file_browser;

    file_browser = fm_main_window_get_current_file_browser( main_window );
    switch ( ptk_file_browser_get_sort_order( file_browser ) )
    {
    case FB_SORT_BY_NAME:
        gtk_check_menu_item_set_active( main_window->sort_by_name, TRUE );
        break;
    case FB_SORT_BY_SIZE:
        gtk_check_menu_item_set_active( main_window->sort_by_size, TRUE );
        break;
    case FB_SORT_BY_MTIME:
        gtk_check_menu_item_set_active( main_window->sort_by_mtime, TRUE );
        break;
    case FB_SORT_BY_TYPE:
        gtk_check_menu_item_set_active( main_window->sort_by_type, TRUE );
        break;
    case FB_SORT_BY_PERM:
        gtk_check_menu_item_set_active( main_window->sort_by_perm, TRUE );
        break;
    case FB_SORT_BY_OWNER:
        gtk_check_menu_item_set_active( main_window->sort_by_owner, TRUE );
        break;
    }

    if ( ptk_file_browser_get_sort_type( file_browser ) == GTK_SORT_ASCENDING )
        gtk_check_menu_item_set_active( main_window->sort_ascending, TRUE );
    else
        gtk_check_menu_item_set_active( main_window->sort_descending, TRUE );
}

void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    int n, hn;
    guint64 total_size;
    char* msg;
    char size_str[ 64 ];

    if ( ! file_browser )
        file_browser = fm_main_window_get_current_file_browser( main_window );

    n = ptk_file_browser_get_n_sel( file_browser, &total_size );
    if ( n > 0 )
    {
        file_size_to_string( size_str, total_size );
        msg = g_strdup_printf( ngettext( "%d item selected (%s)",
                                         "%d items selected (%s)", n ), n, size_str );
        gtk_statusbar_push( main_window->status_bar, 0, msg );
    }
    else
    {
        n = ptk_file_browser_get_n_visible_files( file_browser );
        hn = ptk_file_browser_get_n_all_files( file_browser ) - n;
        msg = g_strdup_printf( ngettext( "%d visible item (%d hidden)",
                                         "%d visible items (%d hidden)", n ), n, hn );
        gtk_statusbar_push( main_window->status_bar, 0, msg );
    }
    g_free( msg );
}

void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                     FMMainWindow* main_window )
{
    if ( fm_main_window_get_current_file_browser( main_window ) == file_browser )
    {
        fm_main_window_update_status_bar( main_window, file_browser );
    }
}

void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                 FMMainWindow* main_window )
{
    if ( fm_main_window_get_current_file_browser( main_window ) == file_browser )
    {
        fm_main_window_update_status_bar( main_window, file_browser );
    }
}

void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
                                       FMMainWindow* main_window )
{
    int i, n;
    PtkFileBrowserSidePaneMode mode;
    GtkWidget* page;
    GtkCheckMenuItem* check;

    if ( file_browser != fm_main_window_get_current_file_browser( main_window ) )
        return ;

    mode = ptk_file_browser_get_side_pane_mode( file_browser );
    switch ( mode )
    {
    case FB_SIDE_PANE_BOOKMARKS:
        check = GTK_CHECK_MENU_ITEM( main_window->show_location_menu );
        break;
    case FB_SIDE_PANE_DIR_TREE:
        check = GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu );
        break;
    }
    gtk_check_menu_item_set_active( check, TRUE );
}

gboolean on_tab_drag_motion ( GtkWidget *widget,
                              GdkDragContext *drag_context,
                              gint x,
                              gint y,
                              guint time,
                              PtkFileBrowser* file_browser )
{
    GtkNotebook * notebook;
    gint idx;

    notebook = GTK_NOTEBOOK( gtk_widget_get_parent( GTK_WIDGET( file_browser ) ) );
    /* TODO: Add a timeout here and don't set current page immediately */
    idx = gtk_notebook_page_num ( notebook, GTK_WIDGET( file_browser ) );
    gtk_notebook_set_current_page( notebook, idx );
}

void fm_main_window_start_busy_task( FMMainWindow* main_window )
{
    GdkCursor * cursor;
    if ( 0 == main_window->n_busy_tasks )              /* Their is no busy task */
    {
        /* Create busy cursor */
        cursor = gdk_cursor_new( GDK_WATCH );
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, cursor );
        gdk_cursor_unref( cursor );
    }
    ++main_window->n_busy_tasks;
}

/* Return gboolean and it can be used in a timeout callback */
gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window )
{
    --main_window->n_busy_tasks;
    if ( 0 == main_window->n_busy_tasks )              /* Their is no more busy task */
    {
        /* Remove busy cursor */
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, NULL );
    }
    return FALSE;
}

void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                          GParamSpec *param,
                                          FMMainWindow* main_window )
{
    int pos;
    int i, n;
    int current;
    GtkWidget* page;

    pos = gtk_paned_get_position( GTK_PANED( file_browser ) );
    main_window->splitter_pos = pos;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        page = gtk_notebook_get_nth_page( main_window->notebook, i );
        if ( page == file_browser )
            continue;
        g_signal_handlers_block_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_file_browser_splitter_pos_change, NULL );
        gtk_paned_set_position( GTK_PANED( page ), pos );
        g_signal_handlers_unblock_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_file_browser_splitter_pos_change, NULL );
    }
}
