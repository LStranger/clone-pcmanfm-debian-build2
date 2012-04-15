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

#include <string.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "pref-dialog.h"
#include "settings.h"
#include "ptk-ui-xml.h"
#include "main-window.h"
#include "ptk-file-browser.h"

static void
on_show_desktop_toggled( GtkToggleButton* show_desktop, GtkWidget* dlg )
{
    GtkWidget* desktop_page = ptk_ui_xml_get_widget( dlg,  "desktop_page" );
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

static gboolean show_preference_dialog( GtkWindow* parent, int page )
{
    gboolean ret;
    GtkWidget* encoding;
    GtkWidget* bm_open_method;
    GtkWidget* max_thumb_size;
    GtkWidget* show_thumbnail;
    GtkWidget* terminal;
    GtkWidget* big_icon_size;
    GtkWidget* small_icon_size;

    GtkWidget* show_desktop;
    GtkWidget* show_wallpaper;
    GtkWidget* wallpaper;
    GtkWidget* wallpaper_mode;
    GtkWidget* img_preview;
    GtkWidget* show_wm_menu;

    GtkWidget* bg_color1;
    GtkWidget* text_color;
    GtkWidget* shadow_color;

    const char* filename_encoding;
    int i;
    const int big_icon_sizes[] =
        {
            96, 72, 64, 48, 36, 32, 24, 20
        };
    const int small_icon_sizes[] =
        {
            48, 36, 32, 24, 20, 16, 12
        };
    int ibig_icon = -1, ismall_icon = -1;
    const char* terminal_programs[] =
        {
            "aterm",
	    "rxvt",
	    "mrxvt",
	    "xfce4-terminal",
            "gnome-terminal",
            "konsole",
            "rxvt",
            "xterm",
            "eterm",
            "mlterm",
            "sakura"
        };

    GtkWidget* dlg = ptk_ui_xml_create_widget_from_file( PACKAGE_UI_DIR "/prefdlg.glade" );

    if( parent )
        gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent );

    gtk_notebook_set_current_page( (GtkNotebook*)ptk_ui_xml_get_widget( dlg, "notebook" ), page );

    encoding = ptk_ui_xml_get_widget( dlg,  "filename_encoding" );
    bm_open_method = ptk_ui_xml_get_widget( dlg,  "bm_open_method" );
    show_thumbnail = ptk_ui_xml_get_widget( dlg,  "show_thumbnail" );
    max_thumb_size = ptk_ui_xml_get_widget( dlg,  "max_thumb_size" );
    terminal = ptk_ui_xml_get_widget( dlg,  "terminal" );
    big_icon_size = ptk_ui_xml_get_widget( dlg,  "big_icon_size" );
    small_icon_size = ptk_ui_xml_get_widget( dlg,  "small_icon_size" );

    if ( '\0' == ( char ) app_settings.encoding[ 0 ] )
        gtk_entry_set_text( GTK_ENTRY( encoding ), "UTF-8" );
    else
        gtk_entry_set_text( GTK_ENTRY( encoding ), app_settings.encoding );

    if ( app_settings.open_bookmark_method >= 1 &&
            app_settings.open_bookmark_method <= 3 )
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( bm_open_method ),
                                  app_settings.open_bookmark_method - 1 );
    }
    else
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( bm_open_method ), 0 );
    }

    gtk_spin_button_set_value ( GTK_SPIN_BUTTON( max_thumb_size ),
                                app_settings.max_thumb_size >> 10 );

    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( show_thumbnail ),
                                   app_settings.show_thumbnail );

    for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
    {
        gtk_combo_box_append_text ( GTK_COMBO_BOX ( terminal ), terminal_programs[ i ] );
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
            gtk_combo_box_prepend_text ( GTK_COMBO_BOX( terminal ), app_settings.terminal );
            i = 0;
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( terminal ), i );
    }

    for ( i = 0; i < G_N_ELEMENTS( big_icon_sizes ); ++i )
    {
        if ( big_icon_sizes[ i ] == app_settings.big_icon_size )
        {
            ibig_icon = i;
            break;
        }
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( big_icon_size ), ibig_icon );

    for ( i = 0; i < G_N_ELEMENTS( small_icon_sizes ); ++i )
    {
        if ( small_icon_sizes[ i ] == app_settings.small_icon_size )
        {
            ismall_icon = i;
            break;
        }
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( small_icon_size ), ismall_icon );

    show_desktop = ptk_ui_xml_get_widget( dlg,  "show_desktop" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show_desktop ),
                                  app_settings.show_desktop );
    on_show_desktop_toggled( GTK_TOGGLE_BUTTON(show_desktop), dlg );
    g_signal_connect( show_desktop, "toggled",
                      G_CALLBACK(on_show_desktop_toggled), dlg );

    show_wallpaper = ptk_ui_xml_get_widget( dlg,  "show_wallpaper" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show_wallpaper ),
                                  app_settings.show_wallpaper );

    wallpaper = ptk_ui_xml_get_widget( dlg,  "wallpaper" );

    img_preview = gtk_image_new();
    gtk_widget_set_size_request( img_preview, 128, 128 );
    gtk_file_chooser_set_preview_widget( (GtkFileChooser*)wallpaper, img_preview );
    g_signal_connect( wallpaper, "update-preview", G_CALLBACK(on_update_img_preview), img_preview );

    if ( app_settings.wallpaper )
    {
        /* FIXME: GTK+ has a known bug here. Sometimes it doesn't update the preview... */
        set_preview_image( img_preview, app_settings.wallpaper ); /* so, we do it manually */
        gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( wallpaper ),
                                       app_settings.wallpaper );
    }
    wallpaper_mode = ptk_ui_xml_get_widget( dlg,  "wallpaper_mode" );
    gtk_combo_box_set_active( (GtkComboBox*)wallpaper_mode, app_settings.wallpaper_mode );

    show_wm_menu = ptk_ui_xml_get_widget( dlg,  "show_wm_menu" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show_wm_menu ),
                                  app_settings.show_wm_menu );

    bg_color1 = ptk_ui_xml_get_widget( dlg,  "bg_color1" );
    text_color = ptk_ui_xml_get_widget( dlg,  "text_color" );
    shadow_color = ptk_ui_xml_get_widget( dlg,  "shadow_color" );
    gtk_color_button_set_color(GTK_COLOR_BUTTON(bg_color1),
                               &app_settings.desktop_bg1);
    gtk_color_button_set_color(GTK_COLOR_BUTTON(text_color),
                               &app_settings.desktop_text);
    gtk_color_button_set_color(GTK_COLOR_BUTTON(shadow_color),
                               &app_settings.desktop_shadow);

    ret = ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK );
    if ( ret )
    {
        filename_encoding = gtk_entry_get_text( GTK_ENTRY( encoding ) );
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

        app_settings.open_bookmark_method = gtk_combo_box_get_active(
                                             GTK_COMBO_BOX( bm_open_method ) ) + 1;

        app_settings.show_thumbnail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_thumbnail ) );
        app_settings.max_thumb_size = ( ( int ) gtk_spin_button_get_value (
                                         GTK_SPIN_BUTTON( max_thumb_size ) ) ) << 10;

        ibig_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( big_icon_size ) );
        if ( ibig_icon >= 0 )
            app_settings.big_icon_size = big_icon_sizes[ ibig_icon ];
        ismall_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( small_icon_size ) );
        if ( ismall_icon >= 0 )
            app_settings.small_icon_size = small_icon_sizes[ ismall_icon ];

        g_free( app_settings.terminal );
        app_settings.terminal = gtk_combo_box_get_active_text( GTK_COMBO_BOX( terminal ) );
        if ( app_settings.terminal && !*app_settings.terminal )
        {
            g_free( app_settings.terminal );
            app_settings.terminal = NULL;
        }

        app_settings.show_desktop = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_desktop ) );
        app_settings.show_wallpaper = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_wallpaper ) );
        g_free( app_settings.wallpaper );
        app_settings.wallpaper = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( wallpaper ) );

        app_settings.wallpaper_mode = gtk_combo_box_get_active( (GtkComboBox*)wallpaper_mode );

        app_settings.show_wm_menu = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_wm_menu ) );

        gtk_color_button_get_color(GTK_COLOR_BUTTON(bg_color1),
                                   &app_settings.desktop_bg1);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(text_color),
                                   &app_settings.desktop_text);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(shadow_color),
                                   &app_settings.desktop_shadow);
    }
    gtk_widget_destroy( dlg );
    return ret;
}

static void
dir_unload_thumbnails( const char* path, VFSDir* dir, gpointer user_data )
{
    vfs_dir_unload_thumbnails( dir, (gboolean)user_data );
}

gboolean fm_edit_preference( GtkWindow* parent, int page )
{
    int max_thumb = app_settings.max_thumb_size;
    gboolean show_thumbnails = app_settings.show_thumbnail;
    int big_icon = app_settings.big_icon_size;
    int small_icon = app_settings.small_icon_size;
    gboolean show_desktop = app_settings.show_desktop;
    gboolean show_wallaper = app_settings.show_wallpaper;
    WallpaperMode wallpaper_mode = app_settings.wallpaper_mode;
    GdkColor bg1 = app_settings.desktop_bg1;
    GdkColor bg2 = app_settings.desktop_bg2;
    GdkColor text = app_settings.desktop_text;
    GdkColor shadow = app_settings.desktop_shadow;
    char* wallpaper;
    int i, n;
    GList* l;
    PtkFileBrowser* file_browser;

    if ( app_settings.wallpaper )
        wallpaper = g_strdup( app_settings.wallpaper );
    else
        wallpaper = g_strdup( "" );

    show_preference_dialog( parent, page );

    if ( show_thumbnails != app_settings.show_thumbnail
            || max_thumb != app_settings.max_thumb_size )
    {
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
    if ( big_icon != app_settings.big_icon_size
            || small_icon != app_settings.small_icon_size )
    {
        vfs_mime_type_set_icon_size( app_settings.big_icon_size, app_settings.small_icon_size );
        vfs_file_info_set_thumbnail_size( app_settings.big_icon_size, app_settings.small_icon_size );

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
            fm_desktop_update_icons();
    }

    if ( show_desktop != app_settings.show_desktop )
    {
        if ( app_settings.show_desktop )
            fm_turn_on_desktop_icons();
        else
            fm_turn_off_desktop_icons();
    }
    else if ( app_settings.show_desktop )
    {
        if ( wallpaper_mode != app_settings.wallpaper_mode ||
                show_wallaper != app_settings.show_wallpaper ||
                ( app_settings.wallpaper &&
                  strcmp( wallpaper, app_settings.wallpaper ) ) )
        {
            fm_desktop_update_wallpaper();
        }
        if ( !gdk_color_equal( &bg1, &app_settings.desktop_bg1 ) ||
                !gdk_color_equal( &bg2, &app_settings.desktop_bg2 ) ||
                !gdk_color_equal( &text, &app_settings.desktop_text ) ||
                !gdk_color_equal( &shadow, &app_settings.desktop_shadow ) )
        {
            fm_desktop_update_colors();
        }
    }

    g_free( wallpaper );
    save_settings();
}
