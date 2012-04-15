/*
*  C Implementation: pref_dialog
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pcmanfm.h"

#include <string.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "glib-utils.h"

#include "pref-dialog.h"
#include "settings.h"
#include "ptk-ui-xml.h"
#include "ptk-utils.h"
#include "main-window.h"
#include "ptk-file-browser.h"
#include "desktop.h"

typedef struct _FMPrefDlg FMPrefDlg;
struct _FMPrefDlg
{
    GtkWidget* dlg;
    GtkWidget* notebook;
    GtkWidget* encoding;
    GtkWidget* bm_open_method;
    GtkWidget* max_thumb_size;
    GtkWidget* show_thumbnail;
    GtkWidget* terminal;
    GtkWidget* big_icon_size;
    GtkWidget* small_icon_size;
    GtkWidget* single_click;

    GtkWidget* show_desktop;
    GtkWidget* show_wallpaper;
    GtkWidget* wallpaper;
    GtkWidget* wallpaper_mode;
    GtkWidget* img_preview;
    GtkWidget* show_wm_menu;

    GtkWidget* bg_color1;
    GtkWidget* text_color;
    GtkWidget* shadow_color;
};

extern gboolean daemon_mode;    /* defined in main.c */

static FMPrefDlg* data = NULL;

static const int big_icon_sizes[] = { 96, 72, 64, 48, 36, 32, 24, 20 };
static const int small_icon_sizes[] = { 48, 36, 32, 24, 20, 16, 12 };
static const char* terminal_programs[] =
{
    "lxterminal",
    "aterm",
    "urxvt",
    "mrxvt",
    "roxterm",
    "xfce4-terminal",
    "gnome-terminal",
    "konsole",
    "xterm",
    "eterm",
    "mlterm",
    "sakura"
};

static void
on_show_desktop_toggled( GtkToggleButton* show_desktop, GtkWidget* dlg )
{
    GtkWidget* desktop_page = ptk_ui_xml_get_widget( dlg, "desktop_page" );
    gtk_container_foreach( GTK_CONTAINER(desktop_page),
                           (GtkCallback) gtk_widget_set_sensitive,
                           (gpointer) gtk_toggle_button_get_active( show_desktop ) );
    gtk_widget_set_sensitive( GTK_WIDGET(show_desktop), TRUE );
}

static void set_preview_image( GtkImage* img, const char* file )
{
    GdkPixbuf* pix = NULL;
    pix = gdk_pixbuf_new_from_file_at_scale( file, 128, 128, TRUE, NULL );
    if( pix )
    {
        gtk_image_set_from_pixbuf( img, pix );
        g_object_unref( pix );
    }
}

static void on_update_img_preview( GtkFileChooser *chooser, GtkImage* img )
{
    char* file = gtk_file_chooser_get_preview_filename( chooser );
    if( file )
    {
        set_preview_image( img, file );
        g_free( file );
    }
    else
    {
        gtk_image_clear( img );
    }
}

static void
dir_unload_thumbnails( const char* path, VFSDir* dir, gpointer user_data )
{
    vfs_dir_unload_thumbnails( dir, (gboolean)user_data );
}

static void on_response( GtkDialog* dlg, int response, FMPrefDlg* user_data )
{
    int i, n;
    gboolean b;
    int ibig_icon = -1, ismall_icon = -1;
    const char* filename_encoding;

    int max_thumb;
    gboolean show_thumbnail;
    int big_icon;
    int small_icon;
    gboolean show_desktop;
    gboolean show_wallpaper;
    gboolean single_click;
    WallpaperMode wallpaper_mode;
    GdkColor bg1;
    GdkColor bg2;
    GdkColor text;
    GdkColor shadow;
    char* wallpaper;
    GList* l;
    PtkFileBrowser* file_browser;

    if ( response == GTK_RESPONSE_OK )
    {
        /* file name encoding */
        filename_encoding = gtk_entry_get_text( GTK_ENTRY( data->encoding ) );
        if ( filename_encoding
            && g_ascii_strcasecmp ( filename_encoding, "UTF-8" ) )
        {
            strcpy( app_settings.encoding, filename_encoding );
            setenv( "G_FILENAME_ENCODING", app_settings.encoding, 1 );
        }
        else
        {
            app_settings.encoding[ 0 ] = '\0';
            unsetenv( "G_FILENAME_ENCODING" );
        }

        app_settings.open_bookmark_method = gtk_combo_box_get_active( GTK_COMBO_BOX( data->bm_open_method ) ) + 1;

        show_thumbnail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->show_thumbnail ) );
        max_thumb = ( ( int ) gtk_spin_button_get_value( GTK_SPIN_BUTTON( data->max_thumb_size ) ) ) << 10;

        /* desktpo settings */
        show_desktop = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->show_desktop ) );

        show_wallpaper = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->show_wallpaper ) );
        wallpaper = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( data->wallpaper ) );
        wallpaper_mode = gtk_combo_box_get_active( (GtkComboBox*)data->wallpaper_mode );
        app_settings.show_wm_menu = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->show_wm_menu ) );

        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->bg_color1), &bg1);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->text_color), &text);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->shadow_color), &shadow);

        if( show_desktop != app_settings.show_desktop )
        {
            app_settings.show_wallpaper = show_wallpaper;
            g_free( app_settings.wallpaper );
            app_settings.wallpaper = wallpaper;
            app_settings.wallpaper_mode = wallpaper_mode;
            app_settings.desktop_bg1 = bg1;
            app_settings.desktop_bg2 = bg2;
            app_settings.desktop_text = text;
            app_settings.desktop_shadow = shadow;
            app_settings.show_desktop = show_desktop;

            if ( app_settings.show_desktop )
                fm_turn_on_desktop_icons();
            else
                fm_turn_off_desktop_icons();
        }
        else if ( app_settings.show_desktop )
        {
            gboolean need_update_bg = FALSE;
            /* desktop colors are changed */
            if ( !gdk_color_equal( &bg1, &app_settings.desktop_bg1 ) ||
                    !gdk_color_equal( &bg2, &app_settings.desktop_bg2 ) ||
                    !gdk_color_equal( &text, &app_settings.desktop_text ) ||
                    !gdk_color_equal( &shadow, &app_settings.desktop_shadow ) )
            {
                app_settings.desktop_bg1 = bg1;
                app_settings.desktop_bg2 = bg2;
                app_settings.desktop_text = text;
                app_settings.desktop_shadow = shadow;

                fm_desktop_update_colors();

                if( wallpaper_mode == WPM_CENTER || !show_wallpaper )
                    need_update_bg = TRUE;
            }

            /* wallpaper settings are changed */
            if( need_update_bg ||
                wallpaper_mode != app_settings.wallpaper_mode ||
                show_wallpaper != app_settings.show_wallpaper ||
                ( g_strcmp0( wallpaper, app_settings.wallpaper ) ) )
            {
                app_settings.wallpaper_mode = wallpaper_mode;
                app_settings.show_wallpaper = show_wallpaper;
                g_free( app_settings.wallpaper );
                app_settings.wallpaper = wallpaper;
                fm_desktop_update_wallpaper();
            }
        }

        /* thumbnail settings are changed */
        if( app_settings.show_thumbnail != show_thumbnail || app_settings.max_thumb_size != max_thumb )
        {
            app_settings.show_thumbnail = show_thumbnail;
            app_settings.max_thumb_size = max_thumb;

            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                FMMainWindow* main_window = FM_MAIN_WINDOW( l->data );
                n = gtk_notebook_get_n_pages( main_window->notebook );
                for ( i = 0; i < n; ++i )
                {
                    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         main_window->notebook, i ) );
                    ptk_file_browser_show_thumbnails( file_browser,
                                                      app_settings.show_thumbnail ? app_settings.max_thumb_size : 0 );
                }
            }

            if ( app_settings.show_desktop )
                fm_desktop_update_thumbnails();
        }

        /* icon sizes are changed? */
        ibig_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( data->big_icon_size ) );
        big_icon = ibig_icon >= 0 ? big_icon_sizes[ ibig_icon ] : app_settings.big_icon_size;
        ismall_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( data->small_icon_size ) );
        small_icon = ismall_icon >= 0 ? small_icon_sizes[ ismall_icon ] : app_settings.small_icon_size;

        if ( big_icon != app_settings.big_icon_size
            || small_icon != app_settings.small_icon_size )
        {
            vfs_mime_type_set_icon_size( big_icon, small_icon );
            vfs_file_info_set_thumbnail_size( big_icon, small_icon );

            /* unload old thumbnails (icons of *.desktop files will be unloaded here, too)  */
            if( big_icon != app_settings.big_icon_size )
                vfs_dir_foreach( (GHFunc)dir_unload_thumbnails, (gpointer)TRUE );
            if( small_icon != app_settings.small_icon_size )
                vfs_dir_foreach( (GHFunc)dir_unload_thumbnails, (gpointer)FALSE );

            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                FMMainWindow* main_window = FM_MAIN_WINDOW( l->data );
                n = gtk_notebook_get_n_pages( main_window->notebook );
                for ( i = 0; i < n; ++i )
                {
                    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         main_window->notebook, i ) );
                    ptk_file_browser_update_display( file_browser );
                }
            }

            if ( app_settings.show_desktop && big_icon != app_settings.big_icon_size )
            {
                app_settings.big_icon_size = big_icon;
                fm_desktop_update_icons();
            }
            app_settings.big_icon_size = big_icon;
            app_settings.small_icon_size = small_icon;
        }

        /* terminal program changed? */
        g_free( app_settings.terminal );
        app_settings.terminal = gtk_combo_box_get_active_text( GTK_COMBO_BOX( data->terminal ) );
        if ( app_settings.terminal && !*app_settings.terminal )
        {
            g_free( app_settings.terminal );
            app_settings.terminal = NULL;
        }

        /* single click changed? */
        single_click = gtk_toggle_button_get_active( (GtkToggleButton*)data->single_click );
        if( single_click != app_settings.single_click )
        {
            app_settings.single_click = single_click;
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                FMMainWindow* main_window = FM_MAIN_WINDOW( l->data );
                n = gtk_notebook_get_n_pages( main_window->notebook );
                for ( i = 0; i < n; ++i )
                {
                    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         main_window->notebook, i ) );
                    ptk_file_browser_set_single_click( file_browser, app_settings.single_click );
                    /* ptk_file_browser_set_single_click_timeout( file_browser, 400 ); */
                }
            }
            fm_desktop_set_single_click( app_settings.single_click );
        }

        /* save to config file */
        save_settings();
    }
    gtk_widget_destroy( dlg );
    g_free( data );
    data = NULL;

    pcmanfm_unref();
}

gboolean fm_edit_preference( GtkWindow* parent, int page )
{
    const char* filename_encoding;
    int i;
    int ibig_icon = -1, ismall_icon = -1;
    GtkWidget* img_preview;
    GtkWidget* dlg;

    if( ! data )
    {
        pcmanfm_ref();

        data = g_new0( FMPrefDlg, 1 );
        dlg = ptk_ui_xml_create_widget_from_file( PACKAGE_UI_DIR "/prefdlg.glade" );

        ptk_dialog_fit_small_screen( dlg );
        data->dlg = dlg;
        data->notebook = ptk_ui_xml_get_widget( dlg, "notebook" );
        gtk_dialog_set_alternative_button_order( dlg, GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );

        data->encoding = ptk_ui_xml_get_widget( dlg,  "filename_encoding" );
        data->bm_open_method = ptk_ui_xml_get_widget( dlg,  "bm_open_method" );
        data->show_thumbnail = ptk_ui_xml_get_widget( dlg,  "show_thumbnail" );
        data->max_thumb_size = ptk_ui_xml_get_widget( dlg,  "max_thumb_size" );
        data->terminal = ptk_ui_xml_get_widget( dlg,  "terminal" );
        data->big_icon_size = ptk_ui_xml_get_widget( dlg,  "big_icon_size" );
        data->small_icon_size = ptk_ui_xml_get_widget( dlg,  "small_icon_size" );
        data->single_click = ptk_ui_xml_get_widget( dlg,  "single_click" );

        if ( '\0' == ( char ) app_settings.encoding[ 0 ] )
            gtk_entry_set_text( GTK_ENTRY( data->encoding ), "UTF-8" );
        else
            gtk_entry_set_text( GTK_ENTRY( data->encoding ), app_settings.encoding );

        if ( app_settings.open_bookmark_method >= 1 &&
                app_settings.open_bookmark_method <= 3 )
        {
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->bm_open_method ),
                                      app_settings.open_bookmark_method - 1 );
        }
        else
        {
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->bm_open_method ), 0 );
        }

        gtk_spin_button_set_value ( GTK_SPIN_BUTTON( data->max_thumb_size ),
                                    app_settings.max_thumb_size >> 10 );

        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->show_thumbnail ),
                                       app_settings.show_thumbnail );

        for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
        {
            gtk_combo_box_append_text ( GTK_COMBO_BOX( data->terminal ), terminal_programs[ i ] );
        }

        if ( app_settings.terminal )
        {
            for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
            {
                if ( 0 == strcmp( terminal_programs[ i ], app_settings.terminal ) )
                    break;
            }
            if ( i >= G_N_ELEMENTS( terminal_programs ) )
            { /* Found */
                gtk_combo_box_prepend_text ( GTK_COMBO_BOX( data->terminal ), app_settings.terminal );
                i = 0;
            }
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->terminal ), i );
        }

        for ( i = 0; i < G_N_ELEMENTS( big_icon_sizes ); ++i )
        {
            if ( big_icon_sizes[ i ] == app_settings.big_icon_size )
            {
                ibig_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->big_icon_size ), ibig_icon );

        for ( i = 0; i < G_N_ELEMENTS( small_icon_sizes ); ++i )
        {
            if ( small_icon_sizes[ i ] == app_settings.small_icon_size )
            {
                ismall_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->small_icon_size ), ismall_icon );

        gtk_toggle_button_set_active( (GtkToggleButton*)data->single_click, app_settings.single_click );

        data->show_desktop = ptk_ui_xml_get_widget( dlg,  "show_desktop" );
        g_signal_connect( data->show_desktop, "toggled",
                          G_CALLBACK(on_show_desktop_toggled), dlg );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->show_desktop ),
                                      app_settings.show_desktop );

        data->show_wallpaper = ptk_ui_xml_get_widget( dlg,  "show_wallpaper" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->show_wallpaper ),
                                      app_settings.show_wallpaper );

        data->wallpaper = ptk_ui_xml_get_widget( dlg,  "wallpaper" );

        img_preview = gtk_image_new();
        gtk_widget_set_size_request( img_preview, 128, 128 );
        gtk_file_chooser_set_preview_widget( (GtkFileChooser*)data->wallpaper, img_preview );
        g_signal_connect( data->wallpaper, "update-preview", G_CALLBACK(on_update_img_preview), img_preview );

        if ( app_settings.wallpaper )
        {
            /* FIXME: GTK+ has a known bug here. Sometimes it doesn't update the preview... */
            set_preview_image( img_preview, app_settings.wallpaper ); /* so, we do it manually */
            gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( data->wallpaper ),
                                           app_settings.wallpaper );
        }
        data->wallpaper_mode = ptk_ui_xml_get_widget( dlg,  "wallpaper_mode" );
        gtk_combo_box_set_active( (GtkComboBox*)data->wallpaper_mode, app_settings.wallpaper_mode );

        data->show_wm_menu = ptk_ui_xml_get_widget( dlg,  "show_wm_menu" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->show_wm_menu ),
                                      app_settings.show_wm_menu );

        data->bg_color1 = ptk_ui_xml_get_widget( dlg,  "bg_color1" );
        data->text_color = ptk_ui_xml_get_widget( dlg,  "text_color" );
        data->shadow_color = ptk_ui_xml_get_widget( dlg,  "shadow_color" );
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->bg_color1),
                                   &app_settings.desktop_bg1);
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->text_color),
                                   &app_settings.desktop_text);
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->shadow_color),
                                   &app_settings.desktop_shadow);

        g_signal_connect( dlg, "response", G_CALLBACK(on_response), data );
    }

    if( parent )
        gtk_window_set_transient_for( GTK_WINDOW( data->dlg ), parent );
    gtk_notebook_set_current_page( (GtkNotebook*)data->notebook, page );
    gtk_window_present( (GtkWindow*)data->dlg );
}

