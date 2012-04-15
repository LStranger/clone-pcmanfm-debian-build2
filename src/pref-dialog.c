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

static void
on_show_desktop_toggled( GtkToggleButton* show_desktop, GtkWidget* dlg )
{
    GtkWidget* desktop_page = ptk_ui_xml_get_widget( dlg,  "desktop_page" );
    gtk_container_foreach( GTK_CONTAINER(desktop_page),
                           (GtkCallback) gtk_widget_set_sensitive,
                           (gpointer) gtk_toggle_button_get_active( show_desktop ) );
    gtk_widget_set_sensitive( GTK_WIDGET(show_desktop), TRUE );
}

gboolean show_preference_dialog( GtkWindow* parent )
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

    GtkWidget* bg_color1;
    GtkWidget* text_color;

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
            "xfce4-terminal",
            "gnome-terminal",
            "konsole",
            "rxvt",
            "xterm",
            "eterm",
            "mlterm"
        };

    GtkWidget* dlg = ptk_ui_xml_create_widget_from_file( PACKAGE_UI_DIR "/prefdlg.glade" );
    gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent );

    encoding = ptk_ui_xml_get_widget( dlg,  "filename_encoding" );
    bm_open_method = ptk_ui_xml_get_widget( dlg,  "bm_open_method" );
    show_thumbnail = ptk_ui_xml_get_widget( dlg,  "show_thumbnail" );
    max_thumb_size = ptk_ui_xml_get_widget( dlg,  "max_thumb_size" );
    terminal = ptk_ui_xml_get_widget( dlg,  "terminal" );
    big_icon_size = ptk_ui_xml_get_widget( dlg,  "big_icon_size" );
    small_icon_size = ptk_ui_xml_get_widget( dlg,  "small_icon_size" );

    if ( '\0' == ( char ) appSettings.encoding[ 0 ] )
        gtk_entry_set_text( GTK_ENTRY( encoding ), "UTF-8" );
    else
        gtk_entry_set_text( GTK_ENTRY( encoding ), appSettings.encoding );

    if ( appSettings.openBookmarkMethod >= 1 &&
            appSettings.openBookmarkMethod <= 3 )
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( bm_open_method ),
                                  appSettings.openBookmarkMethod - 1 );
    }
    else
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( bm_open_method ), 0 );
    }

    gtk_spin_button_set_value ( GTK_SPIN_BUTTON( max_thumb_size ),
                                appSettings.maxThumbSize >> 10 );

    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( show_thumbnail ),
                                   appSettings.showThumbnail );

    for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
    {
        gtk_combo_box_append_text ( GTK_COMBO_BOX ( terminal ), terminal_programs[ i ] );
    }

    if ( appSettings.terminal )
    {
        for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
        {
            if ( 0 == strcmp( terminal_programs[ i ], appSettings.terminal ) )
                break;
        }
        if ( i >= G_N_ELEMENTS( terminal_programs ) )
        { /* Found */
            gtk_combo_box_prepend_text ( GTK_COMBO_BOX( terminal ), appSettings.terminal );
            i = 0;
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( terminal ), i );
    }

    for ( i = 0; i < G_N_ELEMENTS( big_icon_sizes ); ++i )
    {
        if ( big_icon_sizes[ i ] == appSettings.bigIconSize )
        {
            ibig_icon = i;
            break;
        }
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( big_icon_size ), ibig_icon );

    for ( i = 0; i < G_N_ELEMENTS( small_icon_sizes ); ++i )
    {
        if ( small_icon_sizes[ i ] == appSettings.smallIconSize )
        {
            ismall_icon = i;
            break;
        }
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( small_icon_size ), ismall_icon );

    show_desktop = ptk_ui_xml_get_widget( dlg,  "show_desktop" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show_desktop ),
                                  appSettings.showDesktop );
    on_show_desktop_toggled( GTK_TOGGLE_BUTTON(show_desktop), dlg );
    g_signal_connect( show_desktop, "toggled",
                      G_CALLBACK(on_show_desktop_toggled), dlg );

    show_wallpaper = ptk_ui_xml_get_widget( dlg,  "show_wallpaper" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show_wallpaper ),
                                  appSettings.showWallpaper );
    wallpaper = ptk_ui_xml_get_widget( dlg,  "wallpaper" );
    if ( appSettings.wallpaper )
    {
        gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( wallpaper ),
                                       appSettings.wallpaper );
    }
    bg_color1 = ptk_ui_xml_get_widget( dlg,  "bg_color1" );
    text_color = ptk_ui_xml_get_widget( dlg,  "text_color" );
    gtk_color_button_set_color(GTK_COLOR_BUTTON(bg_color1),
                               &appSettings.desktopBg1);
    gtk_color_button_set_color(GTK_COLOR_BUTTON(text_color),
                               &appSettings.desktopText);

    ret = ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK );
    if ( ret )
    {
        filename_encoding = gtk_entry_get_text( GTK_ENTRY( encoding ) );
        if ( filename_encoding
                && g_ascii_strcasecmp ( filename_encoding, "UTF-8" ) )
        {
            strcpy( appSettings.encoding, filename_encoding );
            setenv( "G_FILENAME_ENCODING", appSettings.encoding, 1 );
        }
        else
        {
            appSettings.encoding[ 0 ] = '\0';
            unsetenv( "G_FILENAME_ENCODING" );
        }

        appSettings.openBookmarkMethod = gtk_combo_box_get_active(
                                             GTK_COMBO_BOX( bm_open_method ) ) + 1;

        appSettings.showThumbnail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_thumbnail ) );
        appSettings.maxThumbSize = ( ( int ) gtk_spin_button_get_value (
                                         GTK_SPIN_BUTTON( max_thumb_size ) ) ) << 10;

        ibig_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( big_icon_size ) );
        if ( ibig_icon >= 0 )
            appSettings.bigIconSize = big_icon_sizes[ ibig_icon ];
        ismall_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( small_icon_size ) );
        if ( ismall_icon >= 0 )
            appSettings.smallIconSize = small_icon_sizes[ ismall_icon ];

        g_free( appSettings.terminal );
        appSettings.terminal = gtk_combo_box_get_active_text( GTK_COMBO_BOX( terminal ) );
        if ( appSettings.terminal && !*appSettings.terminal )
        {
            g_free( appSettings.terminal );
            appSettings.terminal = NULL;
        }

        appSettings.showDesktop = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_desktop ) );
        appSettings.showWallpaper = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( show_wallpaper ) );
        g_free( appSettings.wallpaper );
        appSettings.wallpaper = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( wallpaper ) );

        gtk_color_button_get_color(GTK_COLOR_BUTTON(bg_color1),
                                   &appSettings.desktopBg1);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(text_color),
                                   &appSettings.desktopText);

    }
    gtk_widget_destroy( dlg );
    return ret;
}

