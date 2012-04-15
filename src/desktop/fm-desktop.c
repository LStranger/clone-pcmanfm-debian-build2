/*
*  fm-desktop.c: Desktop integration for PCManFM
*
*  Copyright (c) 2004 Brian Tarricone, <bjt23@cornell.edu>
*  Copyright (c) 2006 Hong Jen Yee (PCMan), <pcman.tw@gmail.com>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Library General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
*  Random portions taken from or inspired by the original xfdesktop for xfce4:
*     Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
*     Copyright (C) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
*  X event forwarding code:
*     Copyright (c) 2004 Nils Rennebarth
*
*  This file is modified form xfdesktop.c of XFCE 4.4
*  to be used in PCMan File Manager.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef DESKTOP_INTEGRATION

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gmodule.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "fm-desktop.h"
#include "working-area.h"
#include "ptk-file-browser.h"
#include "ptk-icon-view.h"
#include "main-window.h"
#include "settings.h"

static GtkWidget **desktops;
static gint n_screens;
static guint busy_cursor = 0;

static void resize_desktops();
static GdkPixmap* set_bg_pixmap( GdkScreen* screen,
                           PtkIconView* view,
                           GdkRectangle* working_area );

static void
event_forward_to_rootwin( GdkScreen *gscreen, GdkEvent *event )
{
    XButtonEvent xev, xev2;
    Display *dpy = GDK_DISPLAY_XDISPLAY( gdk_screen_get_display( gscreen ) );

    if ( event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE )
    {
        if ( event->type == GDK_BUTTON_PRESS )
        {
            xev.type = ButtonPress;
            /*
             * rox has an option to disable the next
             * instruction. it is called "blackbox_hack". Does
             * anyone know why exactly it is needed?
             */
            XUngrabPointer( dpy, event->button.time );
        }
        else
            xev.type = ButtonRelease;

        xev.button = event->button.button;
        xev.x = event->button.x;    /* Needed for icewm */
        xev.y = event->button.y;
        xev.x_root = event->button.x_root;
        xev.y_root = event->button.y_root;
        xev.state = event->button.state;

        xev2.type = 0;
    }
    else if ( event->type == GDK_SCROLL )
    {
        xev.type = ButtonPress;
        xev.button = event->scroll.direction + 4;
        xev.x = event->scroll.x;    /* Needed for icewm */
        xev.y = event->scroll.y;
        xev.x_root = event->scroll.x_root;
        xev.y_root = event->scroll.y_root;
        xev.state = event->scroll.state;

        xev2.type = ButtonRelease;
        xev2.button = xev.button;
    }
    else
        return ;
    xev.window = GDK_WINDOW_XWINDOW( gdk_screen_get_root_window( gscreen ) );
    xev.root = xev.window;
    xev.subwindow = None;
    xev.time = event->button.time;
    xev.same_screen = True;

    XSendEvent( dpy, xev.window, False, ButtonPressMask | ButtonReleaseMask,
                ( XEvent * ) & xev );
    if ( xev2.type == 0 )
        return ;

    /* send button release for scroll event */
    xev2.window = xev.window;
    xev2.root = xev.root;
    xev2.subwindow = xev.subwindow;
    xev2.time = xev.time;
    xev2.x = xev.x;
    xev2.y = xev.y;
    xev2.x_root = xev.x_root;
    xev2.y_root = xev.y_root;
    xev2.state = xev.state;
    xev2.same_screen = xev.same_screen;

    XSendEvent( dpy, xev2.window, False, ButtonPressMask | ButtonReleaseMask,
                ( XEvent * ) & xev2 );
}

static gboolean
scroll_cb( GtkWidget *w, GdkEventScroll *evt, gpointer user_data )
{
    event_forward_to_rootwin( gtk_widget_get_screen( w ), ( GdkEvent* ) evt );
    return TRUE;
}

static gboolean
button_cb( GtkWidget *w, GdkEventButton *evt, gpointer user_data )
{
    gint button = evt->button;
    gint state = evt->state;

    if ( evt->type == GDK_BUTTON_PRESS )
    {
        if ( button == 2 || ( button == 1 && ( state & GDK_SHIFT_MASK )
                              && ( state & GDK_CONTROL_MASK ) ) )
        {
            return FALSE;
        }
        else if ( button == 3 || ( button == 1 && ( state & GDK_SHIFT_MASK ) ) )
        {
            //            popup_desktop_menu(gscreen, button, evt->time);
            return FALSE;
        }
    }

    return FALSE;
}

static gboolean
reload_idle_cb( gpointer data )
{
    return FALSE;
}

#if 0
gboolean
client_message_received( GtkWidget *w, GdkEventClient *evt, gpointer user_data )
{
    if ( evt->data_format == 8 )
    {
        if ( !strcmp( RELOAD_MESSAGE, evt->data.b ) )
        {
            g_idle_add ( ( GSourceFunc ) reload_idle_cb, NULL );
            return TRUE;
        }
        else if ( !strcmp( MENU_MESSAGE, evt->data.b ) )
        {
            popup_desktop_menu( gtk_widget_get_screen( w ), 0, GDK_CURRENT_TIME );
            return TRUE;
        }
        else if ( !strcmp( WINDOWLIST_MESSAGE, evt->data.b ) )
        {
            popup_windowlist( gtk_widget_get_screen( w ), 0, GDK_CURRENT_TIME );
            return TRUE;
        }
        else if ( !strcmp( QUIT_MESSAGE, evt->data.b ) )
        {
            gtk_main_quit();
            return TRUE;
        }
    }

    return FALSE;
}

#endif

static void
sighandler_cb( int sig )
{
    switch ( sig )
    {
    case SIGUSR1:
        g_idle_add ( ( GSourceFunc ) reload_idle_cb, NULL );
        break;
    default:
        gtk_main_quit();
        break;
    }
}

static
GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent,
                                   GdkEvent *event,
                                   gpointer data )
{
    XPropertyEvent * evt = ( XPropertyEvent* ) xevent;
    /* If the size of working area changed */
    /* g_debug("%s\n", gdk_x11_get_xatom_name(evt->atom)); */
    if ( evt->type == PropertyNotify &&
            evt->atom == XInternAtom( evt->display,
                                      "_NET_WORKAREA", False ) )
    {
        resize_desktops();
    }
    return GDK_FILTER_TRANSLATE;
}

int fm_desktop_init()
{
    GdkDisplay * gdpy;
    gint i;

    gdpy = gdk_display_get_default();

    n_screens = gdk_display_get_n_screens( gdpy );
    desktops = g_new( GtkWidget *, n_screens );
    for ( i = 0; i < n_screens; i++ )
    {
        desktops[ i ] = fm_desktop_new( gdk_display_get_screen( gdpy, i ) );
        gtk_widget_add_events( desktops[ i ],
                               GDK_BUTTON_PRESS_MASK |
                               GDK_BUTTON_RELEASE_MASK |
                               GDK_SCROLL_MASK );
        g_signal_connect( G_OBJECT( desktops[ i ] ), "scroll-event",
                          G_CALLBACK( scroll_cb ), NULL );
        g_signal_connect_after( G_OBJECT( desktops[ i ] ),
                                "button-press-event",
                                G_CALLBACK( button_cb ), NULL );
        gtk_widget_show_all( desktops[ i ] );
        gdk_window_lower( desktops[ i ] ->window );
    }

    signal( SIGPIPE, SIG_IGN );

    signal( SIGHUP, sighandler_cb );
    signal( SIGINT, sighandler_cb );
    signal( SIGTERM, sighandler_cb );
    signal( SIGUSR1, sighandler_cb );

    return 0;
}

void fm_desktop_cleanup()
{
    int i;
    for ( i = 0; i < n_screens; i++ )
        gtk_widget_destroy( desktops[ i ] );
    g_free( desktops );
    if ( busy_cursor > 0 )
        g_source_remove( busy_cursor );
}

static gboolean remove_busy_cursor()
{
    int i;
    for ( i = 0; i < n_screens; i++ )
        gdk_window_set_cursor ( desktops[ i ] ->window, NULL );
    busy_cursor = 0;
    return FALSE;
}

static void show_busy_cursor()
{
    if ( busy_cursor > 0 )
        g_source_remove( busy_cursor );
    else
    {
        int i;
        for ( i = 0; i < n_screens; i++ )
        {
            GdkCursor* cursor;
            cursor = gdk_cursor_new_for_display( gtk_widget_get_display(desktops[i]), GDK_WATCH );
            gdk_window_set_cursor ( desktops[ i ] ->window, cursor );
            gdk_cursor_unref( cursor );
        }
    }
    busy_cursor = g_timeout_add( 1000,
                                 remove_busy_cursor, NULL );
}

static void on_open_item ( PtkFileBrowser* file_browser,
                           const char* path,
                           PtkOpenAction action,
                           gpointer user_data )
{
    FMMainWindow * main_window = NULL;
    show_busy_cursor();
    switch ( action )
    {
    case PTK_OPEN_NEW_TAB:
        main_window = fm_main_window_get_last_active();
    case PTK_OPEN_DIR:
    case PTK_OPEN_NEW_WINDOW:
        if ( !main_window )
            main_window = FM_MAIN_WINDOW(fm_main_window_new());
        gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                     appSettings.width,
                                     appSettings.height );
        fm_main_window_add_new_tab( main_window, path,
                                    appSettings.showSidePane,
                                    appSettings.sidePaneMode );
        gtk_window_present ( GTK_WINDOW( main_window ) );
        break;
    case PTK_OPEN_TERMINAL:
        fm_main_window_open_terminal( GTK_WINDOW( user_data ), path );
        break;
    default:
        break;
    }
}

static void remove_filter( GObject* desktop, gpointer screen )
{
    gdk_window_remove_filter(
        gdk_screen_get_root_window( GDK_SCREEN( screen ) ),
        on_rootwin_event, desktop );
}

static gboolean on_desktop_key_press( GtkWidget* desktop,
                                      GdkEventKey* evt,
                                      gpointer user_data )
{
    if( evt->keyval == GDK_F4 )
    {
        PtkFileBrowser* browser = (PtkFileBrowser*)user_data;
        fm_main_window_open_terminal( GTK_WINDOW(desktop),
                                      ptk_file_browser_get_cwd( browser ));
        return TRUE;
    }
    else if( evt->keyval == GDK_BackSpace )
    {
        /* Intercept the Backspace key to supress the default "Go Up" behavior of PtkFileBrowser */
        return TRUE;
    }
    return FALSE;
}

static gboolean on_desktop_expose( GtkWidget* desktop,
                                   GdkEventExpose* event,
                                   gpointer user_data )
{
    GtkWidget * align, *browser, *view;
    GdkPixmap* pix;
    align = gtk_bin_get_child( GTK_BIN( desktop ) );
    browser = gtk_bin_get_child( GTK_BIN( align ) );
    view = ptk_file_browser_get_folder_view( PTK_FILE_BROWSER(browser) );
    pix = ptk_icon_view_get_background( PTK_ICON_VIEW(view) );
    if ( pix )
    {
        gdk_draw_drawable( event->window,
                           desktop->style->fg_gc[ GTK_STATE_NORMAL ],
                           pix, event->area.x, event->area.y,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height );

    }
    return TRUE;
}

static gboolean on_icon_view_button_press( PtkIconView* view,
                                           GdkEventButton* evt,
                                           GtkWidget* desktop )
{
    GtkTreePath * tree_path;
    GtkWidget* popup = NULL;
    tree_path = ptk_icon_view_get_path_at_pos( view, evt->x, evt->y );
    if ( tree_path )
    {
        /* TODO: Do some special processig on mounted devices. */
        gtk_tree_path_free( tree_path );
    }
    else /* pressed on desktop */
    {
        if( evt->button == 3 ) /* right button */
        {
            /* g_debug("no item, btn = %d", evt->button); */
            /*
            popup = gtk_menu_new();
            GtkWidget* tmp = gtk_menu_item_new_with_label("Test");
            gtk_menu_shell_append( popup, tmp );
            gtk_widget_show_all( popup );
            */
        }
    }
    if( popup )
    {
        g_signal_connect( popup, "selection-done",
                          G_CALLBACK(gtk_widget_destroy), NULL );
        gtk_menu_popup( GTK_MENU(popup), NULL, NULL, NULL, NULL, evt->button, evt->time );
        g_signal_stop_emission_by_name( view, "button-press-event" );
        return TRUE;
    }
    return FALSE;
}

GtkWidget* fm_desktop_new( GdkScreen* screen )
{
    GtkWidget * desktop;
    PtkFileBrowser *browser;
    GtkWidget *view;
    GtkScrolledWindow *scroll;
    GdkWindow *root;
    GtkWidget* alignment;
    const char* desktop_dir;
    GdkRectangle area;
    int rpad, bpad;
    guint32 val;

    desktop = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_widget_set_app_paintable( desktop, TRUE );

    get_working_area( screen, &area );
    gtk_window_move( GTK_WINDOW( desktop ), 0, 0 );
    gtk_widget_set_size_request( desktop,
                                 gdk_screen_get_width( screen ),
                                 gdk_screen_get_height( screen ) );

    alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
    gtk_container_add( GTK_CONTAINER(desktop), alignment );

    browser = PTK_FILE_BROWSER(ptk_file_browser_new( alignment, PTK_FB_ICON_VIEW ));
    gtk_widget_set_app_paintable( browser, TRUE );
g_debug("HERE");
    g_signal_connect( browser, "open-item",
                      G_CALLBACK( on_open_item ), desktop );

/*
    g_signal_connect( browser, "before-chdir",
                      G_CALLBACK( on_before_chdir ), desktop );
*/

    ptk_file_browser_show_hidden_files( browser,
                                        appSettings.showHiddenFiles );
    ptk_file_browser_show_thumbnails( browser,
                                      appSettings.showThumbnail ? appSettings.maxThumbSize : 0 );
    desktop_dir = vfs_get_desktop_dir();

    /* FIXME: add histroy should be true because
    current working directory is stored in history list. */
    ptk_file_browser_chdir( browser, desktop_dir, PTK_FB_CHDIR_ADD_HISTORY );

    gtk_widget_set_size_request( GTK_WIDGET(browser),
                                 area.width, area.height );
    rpad = gdk_screen_get_width( screen );
    rpad -= ( area.x + area.width );
    bpad = gdk_screen_get_height( screen );
    bpad -= ( area.y + area.height );
    gtk_alignment_set_padding( GTK_ALIGNMENT(alignment),
                               area.y, bpad, area.x, rpad );
    gtk_container_add( GTK_CONTAINER(alignment), GTK_WIDGET(browser) );

    gtk_widget_show_all( alignment );
    gtk_widget_realize( desktop );
    gdk_window_set_type_hint( desktop->window,
                              GDK_WINDOW_TYPE_HINT_DESKTOP );

    view = ptk_file_browser_get_folder_view( browser );
    g_signal_connect( view, "button-press-event",
                      G_CALLBACK( on_icon_view_button_press ), desktop );
    scroll = GTK_SCROLLED_WINDOW( gtk_widget_get_parent( view ) );
    gtk_scrolled_window_set_policy( scroll,
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER );
    gtk_scrolled_window_set_shadow_type( scroll, GTK_SHADOW_NONE );

    root = gdk_screen_get_root_window( screen );
    gdk_window_set_events( root, gdk_window_get_events( root )
                           | GDK_PROPERTY_CHANGE_MASK );
    gdk_window_add_filter( root, on_rootwin_event, desktop );
    g_signal_connect( desktop, "destroy", G_CALLBACK(remove_filter), screen );
    g_signal_connect( desktop, "expose-event",
                      G_CALLBACK(on_desktop_expose), NULL );
    g_signal_connect( desktop, "key-press-event",
                      G_CALLBACK(on_desktop_key_press), browser );

    gtk_widget_set_double_buffered( desktop, FALSE );
    set_bg_pixmap( screen, PTK_ICON_VIEW(view), &area );
    gtk_window_set_skip_pager_hint( GTK_WINDOW(desktop), TRUE );
    gtk_window_set_skip_taskbar_hint( GTK_WINDOW(desktop), TRUE );

    /* This is borrowed from fbpanel */
#define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* skip "alt-tab" */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(desktop->window),
          XInternAtom(GDK_DISPLAY(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                        &appSettings.desktopText );
    gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                        &appSettings.desktopBg1 );
    gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                        &appSettings.desktopBg2 );

    gtk_widget_modify_text( view,
                            GTK_STATE_NORMAL,
                            &appSettings.desktopText );
    gtk_widget_modify_base( view,
                            GTK_STATE_NORMAL,
                            &appSettings.desktopBg1 );
    gdk_window_set_background( root, &appSettings.desktopBg1 );
    return desktop;
}

GdkPixmap* set_bg_pixmap( GdkScreen* screen,
                    PtkIconView* view,
                    GdkRectangle* working_area )
{
    GdkWindow * root;
    GdkPixmap* pix = NULL;
    GdkPixbuf* img;

    root = gdk_screen_get_root_window( screen );
    if ( appSettings.showWallpaper )
    {
        int screenw, screenh;
        int imgw, imgh, x = 0, y = 0;
        screenw = gdk_screen_get_width( screen );
        screenh = gdk_screen_get_height( screen );
        img = gdk_pixbuf_new_from_file_at_scale(
                  appSettings.wallpaper,
                  screenw, screenh,
                  TRUE, NULL );
        if ( img )
        {
            GdkGC * gc;
            pix = gdk_pixmap_new( root, screenw, screenh, -1 );
            imgw = gdk_pixbuf_get_width( img );
            imgh = gdk_pixbuf_get_height( img );
            if ( imgw == screenw )
            {
                /* center vertically */
                y = ( screenh - imgh ) / 2;
            }
            else
            {
                /* center horizontally */
                x = ( screenw - imgw ) / 2;
            }
            /* FIXME: fill the blank area with bg color.*/
            gc = gdk_gc_new( pix );
            gdk_gc_set_rgb_fg_color( gc, &appSettings.desktopBg1 );
            gdk_gc_set_fill( gc, GDK_SOLID );
            /* fill the whole pixmap is not efficient at all!!! */
            gdk_draw_rectangle( pix, gc, TRUE,
                                0, 0, screenw, screenh );
            g_object_unref( G_OBJECT( gc ) );
            gdk_draw_pixbuf( pix, NULL, img, 0, 0, x, y,
                             imgw, imgh,
                             GDK_RGB_DITHER_NONE, 0, 0 );
            gdk_pixbuf_unref( img );
        }
    }

    ptk_icon_view_set_background( PTK_ICON_VIEW( view ), pix,
                                  working_area->x, working_area->y );
    gdk_window_set_back_pixmap( root, pix, FALSE );
    if ( pix )
        g_object_unref( G_OBJECT( pix ) );

    gdk_window_clear( root );
    return pix;
}

void resize_desktops()
{
    int i;
    GdkRectangle area;
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen;

    for ( i = 0; i < n_screens; ++i )
    {
        GtkWidget *view, *alignment, *browser;
        int bpad, rpad;

        screen = gdk_display_get_screen( display, i );
        get_working_area( screen, &area );
        gtk_window_move( GTK_WINDOW( desktops[ i ] ), 0, 0 );
        gtk_widget_set_size_request( desktops[ i ],
                                     gdk_screen_get_width( screen ),
                                     gdk_screen_get_height( screen ) );
        alignment = gtk_bin_get_child( GTK_BIN( desktops[ i ] ) );
        browser = gtk_bin_get_child( GTK_BIN( alignment ) );
        view = ptk_file_browser_get_folder_view( PTK_FILE_BROWSER( browser ) );
        gtk_widget_set_size_request( browser,
                                     area.width, area.height );
        rpad = gdk_screen_get_width( screen );
        rpad -= ( area.x + area.width );
        bpad = gdk_screen_get_height( screen );
        bpad -= ( area.y + area.height );
        gtk_alignment_set_padding( GTK_ALIGNMENT(alignment),
                                   area.y, bpad, area.x, rpad );

        set_bg_pixmap( screen, PTK_ICON_VIEW(view), &area );
    }
}

void fm_desktop_update_wallpaper()
{
    resize_desktops();
}

void fm_desktop_update_colors()
{
    int i;
    for ( i = 0; i < n_screens; ++i )
    {
        GdkScreen* screen;
        GtkWidget *view, *alignment, *browser;
        screen = gdk_display_get_screen( gdk_display_get_default(), i );
        alignment = gtk_bin_get_child( GTK_BIN( desktops[ i ] ) );
        browser = gtk_bin_get_child( GTK_BIN( alignment ) );
        view = ptk_file_browser_get_folder_view( PTK_FILE_BROWSER( browser ) );
        gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                            &appSettings.desktopText );
        gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                            &appSettings.desktopBg1 );
        gdk_rgb_find_color( gtk_widget_get_colormap( view ),
                            &appSettings.desktopBg2 );
        gdk_window_set_background(
            gdk_screen_get_root_window( screen ),
            &appSettings.desktopBg1 );
        gtk_widget_modify_text( view,
                                GTK_STATE_NORMAL,
                                &appSettings.desktopText );
        gtk_widget_modify_base( view,
                                GTK_STATE_NORMAL,
                                &appSettings.desktopBg1 );
    }
    if ( appSettings.showWallpaper )
        fm_desktop_update_wallpaper();
}

static PtkFileBrowser* fm_desktop_get_browser( GtkWidget* desktop )
{
    GtkWidget * align;
    align = gtk_bin_get_child( ( GtkBin* ) desktop );
    return ( PtkFileBrowser* ) gtk_bin_get_child( ( GtkBin* ) align );
}

void fm_desktop_update_thumbnails()
{
    int i;
    PtkFileBrowser* browser;
    for ( i = 0; i < n_screens; ++i )
    {
        browser = fm_desktop_get_browser( desktops[ i ] );
        ptk_file_browser_show_thumbnails( browser,
                                          appSettings.showThumbnail ? appSettings.maxThumbSize : 0 );
    }
}

void fm_desktop_update_view()
{
    int i;
    PtkFileBrowser* browser;
    for ( i = 0; i < n_screens; ++i )
    {
        browser = fm_desktop_get_browser( desktops[ i ] );
        ptk_file_browser_update_display( browser );
    }
}

#else /* ! DESKTOP_INTEGRATION */

int fm_desktop_init() { return 0; }
void fm_desktop_cleanup() { }
void fm_desktop_update_wallpaper() { }
void fm_desktop_update_colors() { }
void fm_desktop_update_thumbnails() { }
void fm_desktop_update_view() { }
#endif

