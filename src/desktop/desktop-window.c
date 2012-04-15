/*
 *      desktop-window.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <glib/gi18n.h>

#include "desktop-window.h"
#include "vfs-file-info.h"
#include "vfs-mime-type.h"

#include "glib-mem.h"
#include "working-area.h"

#include "ptk-file-misc.h"
#include "ptk-file-menu.h"
#include "ptk-utils.h"

#include "settings.h"
#include "main-window.h"
#include "pref-dialog.h"
#include "ptk-file-browser.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

/* for stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct _DesktopItem
{
    VFSFileInfo* fi;
    guint order;
    GdkRectangle box;   /* bounding rect */
    GdkRectangle icon_rect;
    GdkRectangle text_rect;

    /* Since pango doesn't support using "wrap" and "ellipsize" at the same time,
         let's  do some dirty hack here.  We draw these two lines separately, and
         we can ellipsize the second line. */
    int len1;   /* length for the first line of label text */
    int line_h1;    /* height of the first line */

    gboolean is_selected : 1;
    gboolean is_prelight : 1;
};

static void desktop_window_class_init           (DesktopWindowClass *klass);
static void desktop_window_init             (DesktopWindow *self);
static void desktop_window_finalize         (GObject *object);

static gboolean on_expose( GtkWidget* w, GdkEventExpose* evt );
static void on_size_allocate( GtkWidget* w, GtkAllocation* alloc );
static void on_size_request( GtkWidget* w, GtkRequisition* req );
static gboolean on_button_press( GtkWidget* w, GdkEventButton* evt );
static gboolean on_button_release( GtkWidget* w, GdkEventButton* evt );
static gboolean on_mouse_move( GtkWidget* w, GdkEventMotion* evt );
static gboolean on_key_press( GtkWidget* w, GdkEventKey* evt );
static void on_style_set( GtkWidget* w, GtkStyle* prev );
static void on_realize( GtkWidget* w );
static gboolean on_focus_in( GtkWidget* w, GdkEventFocus* evt );
static gboolean on_focus_out( GtkWidget* w, GdkEventFocus* evt );
/* static gboolean on_scroll( GtkWidget *w, GdkEventScroll *evt, gpointer user_data ); */

static void on_file_listed( VFSDir* dir,  gboolean is_cancelled, DesktopWindow* self );
static void on_file_created( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_file_deleted( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_file_changed( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_thumbnail_loaded( VFSDir* dir,  VFSFileInfo* fi, DesktopWindow* self );

static void on_sort_by_name ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_size ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_mtime ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_type ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_custom( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_ascending( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_descending( GtkMenuItem *menuitem, DesktopWindow* self );

static void on_paste( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_settings( GtkMenuItem *menuitem, DesktopWindow* self );

static void on_popup_new_folder_activate ( GtkMenuItem *menuitem, gpointer data );
static void on_popup_new_text_file_activate ( GtkMenuItem *menuitem, gpointer data );                           

static GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent, GdkEvent *event, gpointer data );
static void forward_event_to_rootwin( GdkScreen *gscreen, GdkEvent *event );

static void calc_item_size( DesktopWindow* self, DesktopItem* item );
static void layout_items( DesktopWindow* self );
static void paint_item( DesktopWindow* self, DesktopItem* item, GdkRectangle* expose_area );

/* FIXME: this is too dirty and here is some redundant code.
 *  We really need better and cleaner APIs for this */
static void open_folders( GList* folders );

static GList* sort_items( GList* items, DesktopWindow* win );
static GCompareDataFunc get_sort_func( DesktopWindow* win );
static int comp_item_by_name( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_size( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_mtime( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_type( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_custom( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );

static void redraw_item( DesktopWindow* win, DesktopItem* item );
static void desktop_item_free( DesktopItem* item );

/*
static GdkPixmap* get_root_pixmap( GdkWindow* root );
static gboolean set_root_pixmap(  GdkWindow* root , GdkPixmap* pix );
*/

static DesktopItem* hit_test( DesktopWindow* self, int x, int y );

/* static Atom ATOM_XROOTMAP_ID = 0; */
static Atom ATOM_NET_WORKAREA = 0;

/* Local data */
static GtkWindowClass *parent_class = NULL;

static GdkPixmap* pix = NULL;

static PtkMenuItemEntry icon_menu[] =
{
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Name" ), on_sort_by_name, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Size" ), on_sort_by_size, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Type" ), on_sort_by_type, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Modification Time" ), on_sort_by_mtime, 0, 0 ),
    /* PTK_RADIO_MENU_ITEM( N_( "Custom" ), on_sort_custom, 0, 0 ), */
    PTK_SEPARATOR_MENU_ITEM,
    PTK_RADIO_MENU_ITEM( N_( "Ascending" ), on_sort_ascending, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Descending" ), on_sort_descending, 0, 0 ),
    PTK_MENU_END
};

static PtkMenuItemEntry create_new_menu[] =
{
    PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_popup_new_folder_activate, 0, 0 ),
    PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_popup_new_text_file_activate, 0, 0 ),
    PTK_MENU_END
};

static PtkMenuItemEntry desktop_menu[] =
{
    PTK_POPUP_MENU( N_( "_Icons" ), icon_menu ),
    PTK_STOCK_MENU_ITEM( GTK_STOCK_PASTE, on_paste ),
    PTK_SEPARATOR_MENU_ITEM,
    PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", create_new_menu ),
    PTK_SEPARATOR_MENU_ITEM,
    PTK_IMG_MENU_ITEM( N_( "_Desktop Settings" ), GTK_STOCK_PREFERENCES, on_settings, GDK_Return, GDK_MOD1_MASK ),
    PTK_MENU_END
};

GType desktop_window_get_type(void)
{
    static GType self_type = 0;
    if (! self_type)
    {
        static const GTypeInfo self_info =
        {
            sizeof(DesktopWindowClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)desktop_window_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(DesktopWindow),
            0,
            (GInstanceInitFunc)desktop_window_init,
            NULL /* value_table */
        };

        self_type = g_type_register_static(GTK_TYPE_WINDOW, "DesktopWindow", &self_info, 0);    }

    return self_type;
}

static void desktop_window_class_init(DesktopWindowClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass* wc;
	typedef gboolean (*DeleteEvtHandler) (GtkWidget*, GdkEvent*);

    g_object_class = G_OBJECT_CLASS(klass);
    wc = GTK_WIDGET_CLASS(klass);

    g_object_class->finalize = desktop_window_finalize;

    wc->expose_event = on_expose;
    wc->size_allocate = on_size_allocate;
    wc->size_request = on_size_request;
    wc->button_press_event = on_button_press;
    wc->button_release_event = on_button_release;
    wc->motion_notify_event = on_mouse_move;
    wc->key_press_event = on_key_press;
    wc->style_set = on_style_set;
    wc->realize = on_realize;
    wc->focus_in_event = on_focus_in;
    wc->focus_out_event = on_focus_out;
    /* wc->scroll_event = on_scroll; */
    wc->delete_event = (DeleteEvtHandler) gtk_true;

    parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);

    /* ATOM_XROOTMAP_ID = XInternAtom( GDK_DISPLAY(),"_XROOTMAP_ID", False ); */
    ATOM_NET_WORKAREA = XInternAtom( GDK_DISPLAY(),"_NET_WORKAREA", False );
}

static void desktop_window_init(DesktopWindow *self)
{
    PangoContext* pc;
    PangoFontMetrics *metrics;
    int font_h;
    GdkWindow* root;

    /* sort by name by default */
//    self->sort_by = DW_SORT_BY_NAME;
//    self->sort_type = GTK_SORT_ASCENDING;
    self->sort_by = app_settings.desktop_sort_by;
    self->sort_type = app_settings.desktop_sort_type;

    self->icon_render = gtk_cell_renderer_pixbuf_new();
    g_object_set( self->icon_render, "follow-state", TRUE, NULL);
#if GTK_CHECK_VERSION( 2, 10, 0 )
    g_object_ref_sink(self->icon_render);
#else
    g_object_ref( self->icon_render );
    gtk_object_sink(self->icon_render);
#endif
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );
    self->pl = gtk_widget_create_pango_layout( (GtkWidget*)self, NULL );
    pango_layout_set_alignment( self->pl, PANGO_ALIGN_CENTER );
    pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );
    pango_layout_set_width( self->pl, 100 * PANGO_SCALE );

    metrics = pango_context_get_metrics(
                            pc, ((GtkWidget*)self)->style->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;

    self->label_w = 100;
    self->icon_size = 48;
    self->spacing = 1;
    self->x_pad = 6;
    self->y_pad = 6;
    self->y_margin = 6;
    self->x_margin = 6;

    self->dir = vfs_dir_get_by_path( vfs_get_desktop_dir() );
    if( vfs_dir_is_file_listed( self->dir ) )
        on_file_listed( self->dir, FALSE, self );
    g_signal_connect( self->dir, "file-listed", G_CALLBACK( on_file_listed ), self );
    g_signal_connect( self->dir, "file-created", G_CALLBACK( on_file_created ), self );
    g_signal_connect( self->dir, "file-deleted", G_CALLBACK( on_file_deleted ), self );
    g_signal_connect( self->dir, "file-changed", G_CALLBACK( on_file_changed ), self );
    g_signal_connect( self->dir, "thumbnail-loaded", G_CALLBACK( on_thumbnail_loaded ), self );

    GTK_WIDGET_SET_FLAGS( (GtkWidget*)self, GTK_CAN_FOCUS );

    gtk_widget_set_app_paintable(  (GtkWidget*)self, TRUE );
/*    gtk_widget_set_double_buffered(  (GtkWidget*)self, FALSE ); */
    gtk_widget_add_events( (GtkWidget*)self,
                                            GDK_POINTER_MOTION_MASK |
                                            GDK_BUTTON_PRESS_MASK |
                                            GDK_BUTTON_RELEASE_MASK |
                                            GDK_KEY_PRESS_MASK|
                                            GDK_PROPERTY_CHANGE_MASK );

    root = gdk_screen_get_root_window( gtk_widget_get_screen( (GtkWidget*)self ) );
    gdk_window_set_events( root, gdk_window_get_events( root )
                           | GDK_PROPERTY_CHANGE_MASK );
    gdk_window_add_filter( root, on_rootwin_event, self );
}


GtkWidget* desktop_window_new(void)
{
    GtkWindow* w = (GtkWindow*)g_object_new(DESKTOP_WINDOW_TYPE, NULL);
    w->type = GTK_WINDOW_TOPLEVEL;
    return (GtkWidget*)w;
}

void desktop_item_free( DesktopItem* item )
{
	vfs_file_info_unref( item->fi );
	g_slice_free( DesktopItem, item );
}

void desktop_window_finalize(GObject *object)
{
    DesktopWindow *self = (DesktopWindow*)object;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_DESKTOP_WINDOW(object));

    gdk_window_remove_filter(
                    gdk_screen_get_root_window( gtk_widget_get_screen( (GtkWidget*)object) ),
                    on_rootwin_event, self );

    if( self->background )
        g_object_unref( self->background );

    self = DESKTOP_WINDOW(object);
    g_object_unref( self->dir );

	g_list_foreach( self->items, (GFunc)desktop_item_free, NULL );
	g_list_free( self->items );

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

/*--------------- Signal handlers --------------*/

gboolean on_expose( GtkWidget* w, GdkEventExpose* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;
    GList* l;
    GdkRectangle intersect;

    if( G_UNLIKELY( ! GTK_WIDGET_VISIBLE (w) || ! GTK_WIDGET_MAPPED (w) ) )
        return TRUE;
/*
    gdk_draw_drawable( w->window, self->gc,
                        self->background,
                        evt->area.x, evt->area.y,
                        evt->area.x, evt->area.y,
                        evt->area.width, evt->area.height );
*/
/*
    gdk_gc_set_tile( self->gc, self->background );
    gdk_gc_set_fill( self->gc,  GDK_TILED );/*
    gdk_draw_rectangle( w->window, self->gc, TRUE,
                        evt->area.x, evt->area.y,
                        evt->area.width, evt->area.height );
*/

    for( l = self->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*)l->data;
        if( gdk_rectangle_intersect( &evt->area, &item->box, &intersect ) )
            paint_item( self, item, &intersect );
    }
    return TRUE;
}

void on_size_allocate( GtkWidget* w, GtkAllocation* alloc )
{
    GdkPixbuf* pix;
    DesktopWindow* self = (DesktopWindow*)w;
    GdkRectangle wa;

    get_working_area( gtk_widget_get_screen(w), &self->wa );
    layout_items( w );

    GTK_WIDGET_CLASS(parent_class)->size_allocate( w, alloc );
}

void desktop_window_set_pixmap( DesktopWindow* win, GdkPixmap* pix )
{
    if( win->background )
        g_object_unref( win->background );
    win->background = pix ? g_object_ref( pix ) : NULL;

    if( GTK_WIDGET_REALIZED( win ) )
        gdk_window_set_back_pixmap( ((GtkWidget*)win)->window, win->background, FALSE );
}

void desktop_window_set_bg_color( DesktopWindow* win, GdkColor* clr )
{
    if( clr )
    {
        win->bg = *clr;
        gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                            &win->bg );
        if( GTK_WIDGET_VISIBLE(win) )
            gtk_widget_queue_draw(  (GtkWidget*)win );
    }
}

void desktop_window_set_text_color( DesktopWindow* win, GdkColor* clr, GdkColor* shadow )
{
    if( clr || shadow )
    {
        if( clr )
        {
            win->fg = *clr;
            gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->fg );
        }
        if( shadow )
        {
            win->shadow = *shadow;
            gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->shadow );
        }
        if( GTK_WIDGET_VISIBLE(win) )
            gtk_widget_queue_draw(  (GtkWidget*)win );
    }
}

/*
 *  Set background of the desktop window.
 *  src_pix is the source pixbuf in original size (no scaling)
 *  This function will stretch or add border to this pixbuf accordiong to 'type'.
 *  If type = DW_BG_COLOR and src_pix = NULL, the background color is used to fill the window.
 */
void desktop_window_set_background( DesktopWindow* win, GdkPixbuf* src_pix, DWBgType type )
{
    GdkPixmap* pixmap = NULL;
    Display* xdisplay;
    Pixmap xpixmap = 0;
    Window xroot;

    if( src_pix )
    {
        int src_w = gdk_pixbuf_get_width(src_pix);
        int src_h = gdk_pixbuf_get_height(src_pix);
        int dest_w = gdk_screen_get_width( gtk_widget_get_screen((GtkWidget*)win) );
        int dest_h = gdk_screen_get_height( gtk_widget_get_screen((GtkWidget*)win) );
        GdkPixbuf* scaled = NULL;

        if( type == DW_BG_TILE )
        {
            pixmap = gdk_pixmap_new( ((GtkWidget*)win)->window, src_w, src_h, -1 );
            gdk_draw_pixbuf( pixmap, NULL, src_pix, 0, 0, 0, 0, src_w, src_h, GDK_RGB_DITHER_NORMAL, 0, 0 );
        }
        else
        {
            int src_x = 0, src_y = 0;
            int dest_x = 0, dest_y = 0;
            int w = 0, h = 0;

            pixmap = gdk_pixmap_new( ((GtkWidget*)win)->window, dest_w, dest_h, -1 );
            switch( type )
            {
            case DW_BG_STRETCH:
                if( src_w == dest_w && src_h == dest_h ) /* the same size, no scale is needed */
                    scaled = (GdkPixbuf*)g_object_ref( src_pix );
                else
                    scaled = gdk_pixbuf_scale_simple( src_pix, dest_w, dest_h, GDK_INTERP_BILINEAR );
                w = dest_w;
                h = dest_h;
                break;
            case DW_BG_FULL:
                if( src_w == dest_w && src_h == dest_h )
                    scaled = (GdkPixbuf*)g_object_ref( src_pix );
                else
                {
                    gdouble w_ratio = (float)dest_w / src_w;
                    gdouble h_ratio = (float)dest_h / src_h;
                    gdouble ratio = MIN( w_ratio, h_ratio );
                    if( ratio == 1.0 )
                        scaled = (GdkPixbuf*)g_object_ref( src_pix );
                    else
                        scaled = gdk_pixbuf_scale_simple( src_pix, (src_w * ratio), (src_h * ratio), GDK_INTERP_BILINEAR );
                }
                w = gdk_pixbuf_get_width( scaled );
                h = gdk_pixbuf_get_height( scaled );

                if( w > dest_w )
                    src_x = (w - dest_w) / 2;
                else if( w < dest_w )
                    dest_x = (dest_w - w) / 2;

                if( h > dest_h )
                    src_y = (h - dest_h) / 2;
                else if( h < dest_h )
                    dest_y = (dest_h - h) / 2;
                break;
            case DW_BG_CENTER:  /* no scale is needed */
                scaled = (GdkPixbuf*)g_object_ref( src_pix );

                if( src_w > dest_w )
                {
                    w = dest_w;
                    src_x = (src_w - dest_w) / 2;
                }
                else
                {
                    w = src_w;
                    dest_x = (dest_w - src_w) / 2;
                }
                if( src_h > dest_h )
                {
                    h = dest_h;
                    src_y = (src_h - dest_h) / 2;
                }
                else
                {
                    h = src_h;
                    dest_y = (dest_h - src_h) / 2;
                }
                break;
            }

            if( scaled )
            {
                if( w != dest_w || h != dest_h )
                {
                    GdkGC *gc = gdk_gc_new( ((GtkWidget*)win)->window );
                    gdk_gc_set_fill(gc, GDK_SOLID);
                    gdk_gc_set_foreground( gc, &win->bg);
                    gdk_draw_rectangle( pixmap, gc, TRUE, 0, 0, dest_w, dest_h );
                    g_object_unref(gc);
                }
                gdk_draw_pixbuf( pixmap, NULL, scaled, src_x, src_y, dest_x, dest_y, w, h,
                                                GDK_RGB_DITHER_NORMAL, 0, 0 );
                g_object_unref( scaled );
            }
            else
            {
                g_object_unref( pixmap );
                pixmap = NULL;
            }
        }
    }

    if( win->background )
        g_object_unref( win->background );
    win->background = pixmap;

    if( pixmap )
        gdk_window_set_back_pixmap( ((GtkWidget*)win)->window, pixmap, FALSE );
    else
        gdk_window_set_background( ((GtkWidget*)win)->window, &win->bg );

    gdk_window_clear( ((GtkWidget*)win)->window );
    gtk_widget_queue_draw( (GtkWidget*)win );

    /* set root map here */
    xdisplay = GDK_DISPLAY_XDISPLAY( gtk_widget_get_display( (GtkWidget*)win) );
    xroot = GDK_WINDOW_XID( gtk_widget_get_root_window( (GtkWidget*)win ) );

    XGrabServer (xdisplay);

    if( pixmap )
    {
        xpixmap = GDK_WINDOW_XWINDOW(pixmap);

        XChangeProperty( xdisplay,
                    xroot,
                    gdk_x11_get_xatom_by_name("_XROOTPMAP_ID"), XA_PIXMAP,
                    32, PropModeReplace,
                    (guchar *) &xpixmap, 1);

        XSetWindowBackgroundPixmap( xdisplay, xroot, xpixmap );
    }
    else
    {
        /* FIXME: Anyone knows how to handle this correctly??? */
    }
    XClearWindow( xdisplay, xroot );

    XUngrabServer( xdisplay );
    XFlush( xdisplay );
}

void desktop_window_set_icon_size( DesktopWindow* win, int size )
{
    GList* l;
    win->icon_size = size;
    layout_items( win );
    for( l = win->items; l; l = l->next )
    {
        VFSFileInfo* fi = ((DesktopItem*)l->data)->fi;
        char* path;
        /* reload the icons for special items if needed */
        if( (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY)  && ! fi->big_thumbnail)
        {
            path = g_build_filename( vfs_get_desktop_dir(), fi->name, NULL );
            vfs_file_info_load_special_info( fi, path );
            g_free( path );
        }
        else if(fi->flags & VFS_FILE_INFO_VIRTUAL)
        {
            /* Currently only "My Documents" is supported */
            if( fi->big_thumbnail )
                g_object_unref( fi->big_thumbnail );
            fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "gnome-fs-home", size, 0, NULL );
            if( ! fi->big_thumbnail )
                fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "folder-home", size, 0, NULL );
        }
        else if( vfs_file_info_is_image( fi ) && ! fi->big_thumbnail )
        {
            vfs_thumbnail_loader_request( win->dir, fi, TRUE );
        }
    }
}

void desktop_window_reload_icons( DesktopWindow* win )
{
/*
	GList* l;
	for( l = win->items; l; l = l->next )
	{
		DesktopItem* item = (DesktopItem*)l->data;

		if( item->icon )
			g_object_unref( item->icon );

		if( item->fi )
			item->icon =  vfs_file_info_get_big_icon( item->fi );
		else
			item->icon = NULL;
	}
*/
	layout_items( win );
}



void on_size_request( GtkWidget* w, GtkRequisition* req )
{
    GdkScreen* scr = gtk_widget_get_screen( w );
    req->width = gdk_screen_get_width( scr );
    req->height = gdk_screen_get_height( scr );
}

gboolean on_button_press( GtkWidget* w, GdkEventButton* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;
    DesktopItem *item, *clicked_item = NULL;
    GList* l;

    clicked_item = hit_test( w, (int)evt->x, (int)evt->y );

    if( evt->type == GDK_BUTTON_PRESS )
    {
        /* if this is a left click, and ctrl / shift is not pressed, deselect all. */
        if( ! (evt->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
        {
            /* don't cancel selection if right button is pressed */
            if( ! (evt->button == 3 && clicked_item && clicked_item->is_selected) )
            {
                for( l = self->items; l ;l = l->next )
                {
                    item = (DesktopItem*) l->data;
                    if( item->is_selected )
                    {
                        item->is_selected = FALSE;
                        redraw_item( self, item );
                    }
                }
            }
        }

        if( clicked_item )
        {
            if( evt->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK) )
                clicked_item->is_selected = ! clicked_item->is_selected;
            else
                clicked_item->is_selected = TRUE;

            if( self->focus && self->focus != item )
            {
                DesktopItem* old_focus = self->focus;
                if( old_focus )
                    redraw_item( w, old_focus );
            }
            self->focus = clicked_item;
            redraw_item( self, clicked_item );

            if( evt->button == 3 )  /* right click */
            {
                GList* sel = desktop_window_get_selected_items( self );
                if( sel )
                {
                    item = (DesktopItem*)sel->data;
                    GtkMenu* popup;
                    GList* l;
                    char* file_path = g_build_filename( vfs_get_desktop_dir(), item->fi->name, NULL );
                    /* FIXME: show popup menu for files */
                    for( l = sel; l; l = l->next )
                        l->data = vfs_file_info_ref( ((DesktopItem*)l->data)->fi );
                    popup = ptk_file_menu_new( file_path, item->fi, vfs_get_desktop_dir(), sel, NULL );
                    g_free( file_path );

                    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
                }
            }
            goto out;
        }
        else /* no item is clicked */
        {
            if( evt->button == 3 )  /* right click on the blank area */
            {
                if( ! app_settings.show_wm_menu ) /* if our desktop menu is used */
                {
                    GtkWidget *popup, *sort_by_items[ 4 ], *sort_type_items[ 2 ];
                    int i;
                    /* show the desktop menu */
                    for( i = 0; i < 4; ++i )
                        icon_menu[ i ].ret = &sort_by_items[ i ];
                    for( i = 0; i < 2; ++i )
                        icon_menu[ 5 + i ].ret = &sort_type_items[ i ];
                    popup = ptk_menu_new_from_data( &desktop_menu, self, NULL );
                    gtk_check_menu_item_set_active( (GtkCheckMenuItem*)sort_by_items[ self->sort_by ], TRUE );
                    gtk_check_menu_item_set_active( (GtkCheckMenuItem*)sort_type_items[ self->sort_type ], TRUE );
                    gtk_widget_show_all(popup);
                    g_signal_connect( popup, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL );

                    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
                    goto out;   /* don't forward the event to root win */
                }
            }
        }
    }
    else if( evt->type == GDK_2BUTTON_PRESS )
    {
        if( clicked_item && evt->button == 1)   /* left double click */
        {
            if( vfs_file_info_is_dir( clicked_item->fi ) )  /* this is a folder */
            {
                GList* sel_files = NULL;
                sel_files = g_list_prepend( sel_files, clicked_item->fi );
                open_folders( sel_files );
                g_list_free( sel_files );
            }
            else /* regular files */
            {
                GList* sel_files = NULL;
                sel_files = g_list_prepend( sel_files, clicked_item->fi );
                ptk_open_files_with_app( vfs_get_desktop_dir(), sel_files, NULL, NULL );
                g_list_free( sel_files );
            }
            goto out;
        }
    }
    /* forward the event to root window */
    forward_event_to_rootwin( gtk_widget_get_screen(w), evt );

out:
    if( ! GTK_WIDGET_HAS_FOCUS(w) )
    {
        /* g_debug( "we don't have the focus, grab it!" ); */
        gtk_widget_grab_focus( w );
    }
    return TRUE;
}

gboolean on_button_release( GtkWidget* w, GdkEventButton* evt )
{
    /* forward the event to root window */
    if( ! hit_test( w, (int)evt->x, (int)evt->y ) )
        forward_event_to_rootwin( gtk_widget_get_screen(w), evt );

    return TRUE;
}

gboolean on_mouse_move( GtkWidget* w, GdkEventMotion* evt )
{
    return TRUE;
}

gboolean on_key_press( GtkWidget* w, GdkEventKey* evt )
{
    GList* sels;
    DesktopWindow* self = (DesktopWindow*)w;
    int modifier = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) );

    sels = desktop_window_get_selected_files( self );

    if ( modifier == GDK_CONTROL_MASK )
    {
        switch ( evt->keyval )
        {
        case GDK_x:
            if( sels )
                ptk_clipboard_cut_or_copy_files( vfs_get_desktop_dir(), sels, FALSE );
            break;
        case GDK_c:
            if( sels )
                ptk_clipboard_cut_or_copy_files( vfs_get_desktop_dir(), sels, TRUE );
            break;
        case GDK_v:
            on_paste( NULL, self );
            break;
/*
        case GDK_i:
            ptk_file_browser_invert_selection( file_browser );
            break;
        case GDK_a:
            ptk_file_browser_select_all( file_browser );
            break;
*/
        }
    }
    else if ( modifier == GDK_MOD1_MASK )
    {
        switch ( evt->keyval )
        {
        case GDK_Return:
            if( sels )
                ptk_show_file_properties( NULL, vfs_get_desktop_dir(), sels );
            break;
        }
    }
    else if ( modifier == 0 )
    {
        switch ( evt->keyval )
        {
        case GDK_F2:
            if( sels )
                ptk_rename_file( NULL, vfs_get_desktop_dir(), (VFSFileInfo*)sels->data );
            break;
        case GDK_Delete:
            if( sels )
                ptk_delete_files( NULL, vfs_get_desktop_dir(), sels );
            break;
        }
    }

    if( sels )
        vfs_file_info_list_free( sels );

    return TRUE;
}

void on_style_set( GtkWidget* w, GtkStyle* prev )
{
    DesktopWindow* self = (DesktopWindow*)w;

    PangoContext* pc;
    PangoFontMetrics *metrics;
    int font_h;
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );

    metrics = pango_context_get_metrics(
                            pc, ((GtkWidget*)self)->style->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;
}

void on_realize( GtkWidget* w )
{
    guint32 val;
    DesktopWindow* self = (DesktopWindow*)w;

    GTK_WIDGET_CLASS(parent_class)->realize( w );
    gdk_window_set_type_hint( w->window,
                              GDK_WINDOW_TYPE_HINT_DESKTOP );

    gtk_window_set_skip_pager_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_skip_taskbar_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_resizable( (GtkWindow*)w, FALSE );

    /* This is borrowed from fbpanel */
#define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* skip "alt-tab" */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(w->window),
          XInternAtom(GDK_DISPLAY(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    if( ! self->gc )
        self->gc = gdk_gc_new( w->window );

//    if( self->background )
//        gdk_window_set_back_pixmap( w->window, self->background, FALSE );
}

gboolean on_focus_in( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    GTK_WIDGET_SET_FLAGS( w, GTK_HAS_FOCUS );
    if( self->focus )
        redraw_item( self, self->focus );
    return FALSE;
}

gboolean on_focus_out( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    if( self->focus )
    {
        GTK_WIDGET_UNSET_FLAGS( w, GTK_HAS_FOCUS );
        redraw_item( self, self->focus );
    }
    return FALSE;
}

/*
gboolean on_scroll( GtkWidget *w, GdkEventScroll *evt, gpointer user_data )
{
    forward_event_to_rootwin( gtk_widget_get_screen( w ), ( GdkEvent* ) evt );
    return TRUE;
}
*/


void on_sort_by_name ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_NAME, self->sort_type );
}

void on_sort_by_size ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_SIZE, self->sort_type );
}

void on_sort_by_mtime ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_MTIME, self->sort_type );
}

void on_sort_by_type ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_TYPE, self->sort_type );
}

void on_sort_custom( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_CUSTOM, self->sort_type );
}

void on_sort_ascending( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, self->sort_by, GTK_SORT_ASCENDING );
}

void on_sort_descending( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, self->sort_by, GTK_SORT_DESCENDING );
}

void on_paste( GtkMenuItem *menuitem, DesktopWindow* self )
{
    const gchar* dest_dir = vfs_get_desktop_dir();
    ptk_clipboard_paste_files( NULL, dest_dir );
}

void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               gpointer data )
{
    ptk_create_new_file( NULL,  vfs_get_desktop_dir(), TRUE, NULL );
}

void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  gpointer data )
{
    ptk_create_new_file( NULL,  vfs_get_desktop_dir(), FALSE, NULL );
}

void on_settings( GtkMenuItem *menuitem, DesktopWindow* self )
{
    fm_edit_preference( NULL, PREF_DESKTOP );
}


/* private methods */

void calc_item_size( DesktopWindow* self, DesktopItem* item )
{
    PangoLayoutLine* line;
    int line_h;

    item->box.width = self->item_w;
    item->box.height = self->y_pad * 2;
    item->box.height += self->icon_size;
    item->box.height += self->spacing;

    pango_layout_set_text( self->pl, item->fi->disp_name, -1 );
    pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );    /* wrap the text */
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_NONE );

    if( pango_layout_get_line_count(self->pl) >= 2 ) /* there are more than 2 lines */
    {
        /* we only allow displaying two lines, so let's get the second line */
/* Pango only provide version check macros in the latest versions...
 *  So there is no point in making this check.
 *  FIXME: this check should be done ourselves in configure.
  */
#if defined (PANGO_VERSION_CHECK)
#if PANGO_VERSION_CHECK( 1, 16, 0 )
        line = pango_layout_get_line_readonly( self->pl, 1 );
#else
        line = pango_layout_get_line( self->pl, 1 );
#endif
#else
        line = pango_layout_get_line( self->pl, 1 );
#endif
        item->len1 = line->start_index; /* this the position where the first line wraps */

        /* OK, now we layout these 2 lines separately */
        pango_layout_set_text( self->pl, item->fi->disp_name, item->len1 );
        pango_layout_get_pixel_size( self->pl, NULL, &line_h );
        item->text_rect.height = line_h;
    }
    else
    {
        item->text_rect.height = 0;
    }

    pango_layout_set_wrap( self->pl, 0 );    /* wrap the text */
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );

    pango_layout_set_text( self->pl, item->fi->disp_name + item->len1, -1 );
    pango_layout_get_pixel_size( self->pl, NULL, &line_h );
    item->text_rect.height += line_h;

    item->text_rect.width = 100;
    item->box.height += item->text_rect.height;

    item->icon_rect.width = item->icon_rect.height = self->icon_size;
}

void layout_items( DesktopWindow* self )
{
    GList* l;
    DesktopItem* item;
    GtkWidget* widget = (GtkWidget*)self;
    int x, y, w, y2;

    self->item_w = MAX( self->label_w, self->icon_size ) + self->x_pad * 2;

    x = self->wa.x + self->x_margin;
    y = self->wa.y + self->y_margin;

    pango_layout_set_width( self->pl, 100 * PANGO_SCALE );

    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;

        item->box.x = x;
        item->box.y = y;

        item->box.width = self->item_w;
        calc_item_size( self, item );

        y2 = self->wa.y + self->wa.height - self->y_margin; /* bottom */
        if( y + item->box.height > y2 ) /* bottom is reached */
        {
            y = self->wa.y + self->y_margin;
            item->box.y = y;
            y += item->box.height;
            x += self->item_w;  /* go to the next column */
            item->box.x = x;
        }
        else /* bottom is not reached */
        {
            y += item->box.height;  /* move to the next row */
        }

        item->icon_rect.x = item->box.x + (item->box.width - self->icon_size) / 2;
        item->icon_rect.y = item->box.y + self->y_pad;

        item->text_rect.x = item->box.x + self->x_pad;
        item->text_rect.y = item->box.y + self->y_pad + self->icon_size + self->spacing;
    }
    gtk_widget_queue_draw( self );
}

void on_file_listed( VFSDir* dir, gboolean is_cancelled, DesktopWindow* self )
{
    GList* l, *items = NULL;
    VFSFileInfo* fi;
    DesktopItem* item;

    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        fi = (VFSFileInfo*)l->data;
        if( fi->name[0] == '.' )    /* skip the hidden files */
            continue;
        item = g_slice_new0( DesktopItem );
        item->fi = vfs_file_info_ref( fi );
        items = g_list_prepend( items, item );
		/* item->icon = vfs_file_info_get_big_icon( fi ); */

        if( vfs_file_info_is_image( fi ) )
            vfs_thumbnail_loader_request( dir, fi, TRUE );
    }
    g_mutex_unlock( dir->mutex );

    self->items = NULL;
    self->items = g_list_sort_with_data( items, get_sort_func(self), self );

    /* Make an item for Home dir */
    fi = vfs_file_info_new();
    fi->disp_name = g_strdup( _("My Documents") );
    fi->mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
    fi->name = g_strdup( g_get_home_dir() );
    fi->mode |= S_IFDIR;
    fi->flags |= VFS_FILE_INFO_VIRTUAL; /* this is a virtual file which doesn't exist on the file system */
    fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "gnome-fs-home", self->icon_size, 0, NULL );
    if( ! fi->big_thumbnail )
        fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "folder-home", self->icon_size, 0, NULL );

    item = g_slice_new0( DesktopItem );
    item->fi = fi;
	/* item->icon = vfs_file_info_get_big_thumbnail( fi ); */

    self->items = g_list_prepend( self->items, item );

    layout_items( self );
}

void on_thumbnail_loaded( VFSDir* dir,  VFSFileInfo* fi, DesktopWindow* self )
{
    GList* l;
    GtkWidget* w = (GtkWidget*)self;

    for( l = self->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->fi == fi )
        {
/*
        	if( item->icon )
        		g_object_unref( item->icon );
        	item->icon = vfs_file_info_get_big_thumbnail( fi );
*/
            redraw_item( self, item );
            return;
        }
    }
}

void on_file_created( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;

    /* don't show hidden files */
    if( file->name[0] == '.' )
        return;

    /* prevent duplicated items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( strcmp( file->name, item->fi->name ) == 0 )
            return;
    }

    item = g_slice_new0( DesktopItem );
    item->fi = vfs_file_info_ref( file );
    /* item->icon = vfs_file_info_get_big_icon( file ); */

    self->items = g_list_insert_sorted_with_data( self->items, item,
                                                                            get_sort_func(self), self );

    /* FIXME: we shouldn't update the whole screen */
    /* FIXME: put this in idle handler with priority higher than redraw but lower than resize */
    layout_items( self );
}

void on_file_deleted( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;

    /* FIXME: special handling is needed here */
    if( ! file )
        return;

    /* don't deal with hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        self->items = g_list_delete_link( self->items, l );
        desktop_item_free( item );
        /* FIXME: we shouldn't update the whole screen */
        /* FIXME: put this in idle handler with priority higher than redraw but lower than resize */
        layout_items( self );
    }
}

void on_file_changed( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;
    GtkWidget* w = (GtkWidget*)self;

    /* don't touch hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        /*
   if( item->icon )
        	g_object_unref( item->icon );
		item->icon = vfs_file_info_get_big_icon( file );
		*/
        if( GTK_WIDGET_VISIBLE( w ) )
        {
            /* redraw the item */
            redraw_item( self, item );
        }
    }
}


/*-------------- Private methods -------------------*/

void paint_item( DesktopWindow* self, DesktopItem* item, GdkRectangle* expose_area )
{
    /* GdkPixbuf* icon = item->icon ? gdk_pixbuf_ref(item->icon) : NULL; */
    GdkPixbuf* icon;
    const char* text = item->fi->disp_name;
    GtkWidget* widget = (GtkWidget*)self;
    GdkDrawable* drawable = widget->window;
    GdkGC* tmp;
    GtkCellRendererState state = 0;
    GdkRectangle text_rect;
    int w, h;

    if( item->fi->big_thumbnail )
        icon = gdk_pixbuf_ref( item->fi->big_thumbnail );
    else
        icon = vfs_file_info_get_big_icon( item->fi );

    if( item->is_selected )
        state = GTK_CELL_RENDERER_SELECTED;

    g_object_set( self->icon_render, "pixbuf", icon, NULL );
    gtk_cell_renderer_render( self->icon_render, drawable, widget,
                                                       &item->icon_rect, &item->icon_rect, expose_area, state );

	if( icon )
		g_object_unref( icon );

    tmp = widget->style->fg_gc[0];

    text_rect = item->text_rect;

    pango_layout_set_wrap( self->pl, 0 );

    if( item->is_selected )
    {
        GdkRectangle intersect={0};

        if( gdk_rectangle_intersect( expose_area, &item->text_rect, &intersect ) )
            gdk_draw_rectangle( widget->window,
                        widget->style->bg_gc[GTK_STATE_SELECTED],
                        TRUE, intersect.x, intersect.y, intersect.width, intersect.height );
    }
    else
    {
        /* Do the drop shadow stuff...  This is a little bit dirty... */
        ++text_rect.x;
        ++text_rect.y;

        gdk_gc_set_foreground( self->gc, &self->shadow );

        if( item->len1 > 0 )
        {
            pango_layout_set_text( self->pl, text, item->len1 );
            pango_layout_get_pixel_size( self->pl, &w, &h );
            gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );
            text_rect.y += h;
        }
        pango_layout_set_text( self->pl, text + item->len1, -1 );
        pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
        gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );

        --text_rect.x;
        --text_rect.y;
    }

    if( self->focus == item && GTK_WIDGET_HAS_FOCUS(widget) )
    {
        gtk_paint_focus( widget->style, widget->window,
                        GTK_STATE_NORMAL,/*item->is_selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,*/
                        &item->text_rect, widget, "icon_view",
                        item->text_rect.x, item->text_rect.y,
                        item->text_rect.width, item->text_rect.height);
    }

    text_rect = item->text_rect;

    gdk_gc_set_foreground( self->gc, &self->fg );

    if( item->len1 > 0 )
    {
        pango_layout_set_text( self->pl, text, item->len1 );
        pango_layout_get_pixel_size( self->pl, &w, &h );
        gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );
        text_rect.y += h;
    }
    pango_layout_set_text( self->pl, text + item->len1, -1 );
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
    gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );

    widget->style->fg_gc[0] = tmp;
}

static gboolean is_point_in_rect( GdkRectangle* rect, int x, int y )
{
    return rect->x < x && x < (rect->x + rect->width) && y > rect->y && y < (rect->y + rect->height);
}

DesktopItem* hit_test( DesktopWindow* self, int x, int y )
{
    DesktopItem* item;
    GList* l;
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if( is_point_in_rect( &item->icon_rect, x, y )
         || is_point_in_rect( &item->text_rect, x, y ) )
            return item;
    }
    return NULL;
}

/* FIXME: this is too dirty and here is some redundant code.
 *  We really need better and cleaner APIs for this */
void open_folders( GList* folders )
{
    FMMainWindow* main_window = fm_main_window_new();
    FM_MAIN_WINDOW( main_window ) ->splitter_pos = app_settings.splitter_pos;
    gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                 app_settings.width,
                                 app_settings.height );
    gtk_widget_show( main_window );
    while( folders )
    {
        VFSFileInfo* fi = (VFSFileInfo*)folders->data;
        char* path;
        if( fi->flags & VFS_FILE_INFO_VIRTUAL )
        {
            /* if this is a special item, not a real file in desktop dir */
            if( fi->name[0] == '/' )   /* it's a real path */
            {
                path = g_strdup( fi->name );
            }
            else
            {
                folders = folders->next;
                continue;
            }
            /* FIXME/TODO: In the future we should handle mounting here. */
        }
        else
        {
            path = g_build_filename( vfs_get_desktop_dir(), fi->name, NULL );
        }
        fm_main_window_add_new_tab( FM_MAIN_WINDOW( main_window ), path,
                                    app_settings.show_side_pane, app_settings.side_pane_mode );

        g_free( path );
        folders = folders->next;
    }
}

GCompareDataFunc get_sort_func( DesktopWindow* win )
{
    GCompareDataFunc comp;

    switch( win->sort_by )
    {
        case DW_SORT_BY_NAME:
            comp = comp_item_by_name;
            break;
        case DW_SORT_BY_SIZE:
            comp = comp_item_by_size;
            break;
        case DW_SORT_BY_TYPE:
            comp = comp_item_by_type;
            break;
        case DW_SORT_BY_MTIME:
            comp = comp_item_by_mtime;
            break;
        case DW_SORT_CUSTOM:
            comp = comp_item_custom;
            break;
        default:
            comp = comp_item_by_name;
            return;
    }
    return comp;
}

/* return -1 if item1 is virtual, and item2 is not, and vice versa. return 0 if both are, or both aren't. */
#define COMP_VIRTUAL( item1, item2 )  \
  ( ( ((item2->fi->flags & VFS_FILE_INFO_VIRTUAL) ? 1 : 0) - ((item1->fi->flags & VFS_FILE_INFO_VIRTUAL) ? 1 : 0) ) )

int comp_item_by_name( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_size( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->size - item2->fi->size;
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_mtime( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->mtime - item2->fi->mtime;
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_type( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret = strcmp( item1->fi->mime_type->type, item2->fi->mime_type->type );

    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_custom( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win )
{
    return (item1->order - item2->order);
}

void redraw_item( DesktopWindow* win, DesktopItem* item )
{
    GdkRectangle rect = item->box;
    --rect.x;
    --rect.y;
    rect.width += 2;
    rect.height += 2;
    gdk_window_invalidate_rect( ((GtkWidget*)win)->window, &rect, FALSE );
}


/* ----------------- public APIs ------------------*/

void desktop_window_sort_items( DesktopWindow* win, DWSortType sort_by, GtkSortType sort_type )
{
    GList* items;
    GList* special_items;

    if( win->sort_type == sort_type && win->sort_by == sort_by )
        return;

    app_settings.desktop_sort_by = win->sort_by = sort_by;
    app_settings.desktop_sort_type = win->sort_type = sort_type;

    /* skip the special items since they always appears first */
    special_items = win->items;
    for( items = special_items; items; items = items->next )
    {
        DesktopItem* item = (DesktopItem*)items->data;
        if( ! (item->fi->flags & VFS_FILE_INFO_VIRTUAL) )
            break;
    }

    if( ! items )
        return;

    /* the previous item of the first non-special item is the last special item */
    if( items->prev )
    {
        items->prev->next = NULL;
        items->prev = NULL;
    }

    items = g_list_sort_with_data( items, get_sort_func(win), win );
    win->items = g_list_concat( special_items, items );

    layout_items( win );
}

GList* desktop_window_get_selected_items( DesktopWindow* win )
{
    GList* sel = NULL;
    GList* l;

    for( l = win->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->is_selected )
        {
            if( G_UNLIKELY( item == win->focus ) )
                sel = g_list_prepend( sel, item );
            else
                sel = g_list_append( sel, item );
        }
    }

    return sel;
}

GList* desktop_window_get_selected_files( DesktopWindow* win )
{
    GList* sel = desktop_window_get_selected_items( win );
    GList* l;

    l = sel;
    while( l )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->fi->flags & VFS_FILE_INFO_VIRTUAL )
        {
            /* don't include virtual items */
            GList* tmp = l;
            sel = g_list_delete_link( sel, l );
            l = tmp->next;
            g_list_free1( l );
        }
        else
        {
            l->data = vfs_file_info_ref( item->fi );
            l = l->next;
        }
    }
    return sel;
}


/*----------------- X11-related sutff ----------------*/

static
GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent,
                                   GdkEvent *event,
                                   gpointer data )
{
    XPropertyEvent * evt = ( XPropertyEvent* ) xevent;
    DesktopWindow* self = (DesktopWindow*)data;

    if ( evt->type == PropertyNotify )
    {
        if( evt->atom == ATOM_NET_WORKAREA )
        {
            /* working area is resized */
            get_working_area( gtk_widget_get_screen((GtkWidget*)self), &self->wa );
            layout_items( self );
        }
#if 0
        else if( evt->atom == ATOM_XROOTMAP_ID )
        {
            /* wallpaper was changed by other programs */
        }
#endif
    }
    return GDK_FILTER_TRANSLATE;
}

/* This function is taken from xfdesktop */
void forward_event_to_rootwin( GdkScreen *gscreen, GdkEvent *event )
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

#if 0
GdkPixmap* get_root_pixmap( GdkWindow* root )
{
    Pixmap root_pix = None;

    Atom type;
    int format;
    long bytes_after;
    Pixmap *data = NULL;
    long n_items;
    int result;

    result =  XGetWindowProperty(
                            GDK_WINDOW_XDISPLAY( root ),
                            GDK_WINDOW_XID( root ),
                            ATOM_XROOTMAP_ID,
                            0, 16L,
                            False, XA_PIXMAP,
                            &type, &format, &n_items,
                            &bytes_after, (unsigned char **)&data);

    if (result == Success && n_items)
        root_pix = *data;

    if (data)
        XFree(data);

    return root_pix ? gdk_pixmap_foreign_new( root_pix ) : NULL;
}

gboolean set_root_pixmap(  GdkWindow* root, GdkPixmap* pix )
{
    return TRUE;
}

#endif
