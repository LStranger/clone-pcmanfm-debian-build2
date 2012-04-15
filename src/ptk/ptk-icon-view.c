/* $Id: ptk-icon-view.c 405 2006-10-10 12:40:26Z pcmanx $ */
/*-
 * Copyright (c) 2004-2006  os-cillation e.K.
 * Copyright (c) 2002,2004  Anders Carlsson <andersca@gnu.org>
 * Copyright (c) 2006       Hong Jen Yee <pcman.tw@gmail.com>
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 * 2006-01-22 Modified by Hong Jen Yee to be used in PCMan File Manager
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *
 * Brief history of PtkIconView
 *
 * This class is originally taken from ExoIconView from XFCE,
 * which is derived from GtkIconView from GTK+ team.
 * (See the copyright notice at the beggining of this file)
 *
 * And then, modified by Hong Jen Yee (PCMan) to be PtkIconView, which
 * is used in PCMan File Manager, a lightwight file manager.
 * Some bugs of original GtkIconView/PtkIconView has been fixed, and
 * interactive search is added by Hong Jen Yee (ported from GtkTreeView).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib/gi18n.h>
#define I_(x) (x)
#include <gdk/gdkkeysyms.h>

#include "ptk-icon-view.h"
#include "ptk-text-renderer.h"

#include "exo-marshal.h"
#include "exo-string.h"

#if defined(G_PARAM_STATIC_NAME) && defined(G_PARAM_STATIC_NICK) && defined(G_PARAM_STATIC_BLURB)
#define PTK_PARAM_READABLE  (G_PARAM_READABLE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#define PTK_PARAM_WRITABLE  (G_PARAM_WRITABLE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#define PTK_PARAM_READWRITE (G_PARAM_READWRITE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#else
#define PTK_PARAM_READABLE  (G_PARAM_READABLE)
#define PTK_PARAM_WRITABLE  (G_PARAM_WRITABLE)
#define PTK_PARAM_READWRITE (G_PARAM_READWRITE)
#endif


#define SCROLL_EDGE_SIZE 15

#define PTK_ICON_VIEW_SEARCH_DIALOG_TIMEOUT 5000


/* Property identifiers */
enum
{
    PROP_0,
    PROP_PIXBUF_COLUMN,
    PROP_TEXT_COLUMN,
    PROP_MARKUP_COLUMN,
    PROP_SELECTION_MODE,
    PROP_ORIENTATION,
    PROP_MODEL,
    PROP_COLUMNS,
    PROP_ITEM_WIDTH,
    PROP_SPACING,
    PROP_ROW_SPACING,
    PROP_COLUMN_SPACING,
    PROP_MARGIN,
    PROP_REORDERABLE,
    PROP_ENABLE_SEARCH,
    PROP_SEARCH_COLUMN,
};

/* Signal identifiers */
enum
{
    ITEM_ACTIVATED,
    SELECTION_CHANGED,
    SELECT_ALL,
    UNSELECT_ALL,
    SELECT_CURSOR_ITEM,
    TOGGLE_CURSOR_ITEM,
    MOVE_CURSOR,
    ACTIVATE_CURSOR_ITEM,
    LAST_SIGNAL
};

/* Icon view flags */
typedef enum
{
    PTK_ICON_VIEW_DRAW_KEYFOCUS = ( 1l << 0 ),      /* whether to draw keyboard focus */
    PTK_ICON_VIEW_ITERS_PERSIST = ( 1l << 1 ),      /* whether current model provides persistent iterators */
} PtkIconViewFlags;

#define PTK_ICON_VIEW_SET_FLAG(icon_view, flag)   G_STMT_START{ (PTK_ICON_VIEW (icon_view)->priv->flags |= flag); }G_STMT_END
#define PTK_ICON_VIEW_UNSET_FLAG(icon_view, flag) G_STMT_START{ (PTK_ICON_VIEW (icon_view)->priv->flags &= ~(flag)); }G_STMT_END
#define PTK_ICON_VIEW_FLAG_SET(icon_view, flag)   ((PTK_ICON_VIEW (icon_view)->priv->flags & (flag)) == (flag))



typedef struct _PtkIconViewCellInfo PtkIconViewCellInfo;
typedef struct _PtkIconViewChild PtkIconViewChild;
typedef struct _PtkIconViewItem PtkIconViewItem;



#define PTK_ICON_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PTK_TYPE_ICON_VIEW, PtkIconViewPrivate))
#define PTK_ICON_VIEW_CELL_INFO(obj)   ((PtkIconViewCellInfo *) (obj))
#define PTK_ICON_VIEW_CHILD(obj)       ((PtkIconViewChild *) (obj))
#define PTK_ICON_VIEW_ITEM(obj)        ((PtkIconViewItem *) (obj))



static void ptk_icon_view_class_init ( PtkIconViewClass *klass );
static void ptk_icon_view_cell_layout_init ( GtkCellLayoutIface *iface );
static void ptk_icon_view_init ( PtkIconView *icon_view );
static void ptk_icon_view_dispose ( GObject *object );
static void ptk_icon_view_finalize ( GObject *object );
static void ptk_icon_view_get_property ( GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec );
static void ptk_icon_view_set_property ( GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec );
static void ptk_icon_view_realize ( GtkWidget *widget );
static void ptk_icon_view_unrealize ( GtkWidget *widget );
static void ptk_icon_view_size_request ( GtkWidget *widget,
                                         GtkRequisition *requisition );
static void ptk_icon_view_size_allocate ( GtkWidget *widget,
                                          GtkAllocation *allocation );
static void ptk_icon_view_style_set ( GtkWidget *widget,
                                      GtkStyle *previous_style );
static gboolean ptk_icon_view_expose_event ( GtkWidget *widget,
                                             GdkEventExpose *event );
static gboolean ptk_icon_view_key_press ( GtkWidget *widget,
                                          GdkEventKey *event );
static gboolean ptk_icon_view_key_release ( GtkWidget *widget,
                                            GdkEventKey *event );
static gboolean ptk_icon_view_motion_notify_event ( GtkWidget *widget,
                                                    GdkEventMotion *event );
static gboolean ptk_icon_view_button_press_event ( GtkWidget *widget,
                                                   GdkEventButton *event );
static gboolean ptk_icon_view_button_release_event ( GtkWidget *widget,
                                                     GdkEventButton *event );
static AtkObject *ptk_icon_view_get_accessible ( GtkWidget *widget );
static void ptk_icon_view_remove ( GtkContainer *container,
                                   GtkWidget *widget );
static void ptk_icon_view_forall ( GtkContainer *container,
                                   gboolean include_internals,
                                   GtkCallback callback,
                                   gpointer callback_data );
static void ptk_icon_view_set_adjustments ( PtkIconView *icon_view,
                                            GtkAdjustment *hadj,
                                            GtkAdjustment *vadj );
static void ptk_icon_view_real_select_all ( PtkIconView *icon_view );
static void ptk_icon_view_real_unselect_all ( PtkIconView *icon_view );
static void ptk_icon_view_real_select_cursor_item ( PtkIconView *icon_view );
static void ptk_icon_view_real_toggle_cursor_item ( PtkIconView *icon_view );
static gboolean ptk_icon_view_real_activate_cursor_item ( PtkIconView *icon_view );
static void ptk_icon_view_adjustment_changed ( GtkAdjustment *adjustment,
                                               PtkIconView *icon_view );
static void ptk_icon_view_layout ( PtkIconView *icon_view );
static void ptk_icon_view_paint_item ( PtkIconView *icon_view,
                                       PtkIconViewItem *item,
                                       GdkRectangle *area,
                                       GdkDrawable *drawable,
                                       gint x,
                                       gint y,
                                       gboolean draw_focus );
static void ptk_icon_view_queue_draw_path ( PtkIconView *icon_view,
                                            GtkTreePath *path );
static void ptk_icon_view_queue_draw_item ( PtkIconView *icon_view,
                                            PtkIconViewItem *item );
static void ptk_icon_view_queue_layout ( PtkIconView *icon_view );
static void ptk_icon_view_set_cursor_item ( PtkIconView *icon_view,
                                            PtkIconViewItem *item,
                                            gint cursor_cell );
static void ptk_icon_view_start_rubberbanding ( PtkIconView *icon_view,
                                                gint x,
                                                gint y );
static void ptk_icon_view_stop_rubberbanding ( PtkIconView *icon_view );
static void ptk_icon_view_update_rubberband_selection ( PtkIconView *icon_view );
static gboolean ptk_icon_view_item_hit_test ( PtkIconView *icon_view,
                                              PtkIconViewItem *item,
                                              gint x,
                                              gint y,
                                              gint width,
                                              gint height );
static gboolean ptk_icon_view_unselect_all_internal ( PtkIconView *icon_view );
static void ptk_icon_view_calculate_item_size ( PtkIconView *icon_view,
                                                PtkIconViewItem *item );
static void ptk_icon_view_calculate_item_size2 ( PtkIconView *icon_view,
                                                 PtkIconViewItem *item,
                                                 gint *max_height );
static void ptk_icon_view_update_rubberband ( gpointer data );
static void ptk_icon_view_item_invalidate_size ( PtkIconViewItem *item );
static void ptk_icon_view_invalidate_sizes ( PtkIconView *icon_view );
static void ptk_icon_view_add_move_binding ( GtkBindingSet *binding_set,
                                             guint keyval,
                                             guint modmask,
                                             GtkMovementStep step,
                                             gint count );
static gboolean ptk_icon_view_real_move_cursor ( PtkIconView *icon_view,
                                                 GtkMovementStep step,
                                                 gint count );
static void ptk_icon_view_move_cursor_up_down ( PtkIconView *icon_view,
                                                gint count );
static void ptk_icon_view_move_cursor_page_up_down ( PtkIconView *icon_view,
                                                     gint count );
static void ptk_icon_view_move_cursor_left_right ( PtkIconView *icon_view,
                                                   gint count );
static void ptk_icon_view_move_cursor_start_end ( PtkIconView *icon_view,
                                                  gint count );
static void ptk_icon_view_scroll_to_item ( PtkIconView *icon_view,
                                           PtkIconViewItem *item );
static void ptk_icon_view_select_item ( PtkIconView *icon_view,
                                        PtkIconViewItem *item );
static void ptk_icon_view_unselect_item ( PtkIconView *icon_view,
                                          PtkIconViewItem *item );
static gboolean ptk_icon_view_select_all_between ( PtkIconView *icon_view,
                                                   PtkIconViewItem *anchor,
                                                   PtkIconViewItem *cursor );
static PtkIconViewItem * ptk_icon_view_get_item_at_coords ( const PtkIconView *icon_view,
                                                            gint x,
                                                            gint y,
                                                            gboolean only_in_cell,
                                                            PtkIconViewCellInfo **cell_at_pos );
static void ptk_icon_view_get_cell_area ( PtkIconView *icon_view,
                                          PtkIconViewItem *item,
                                          PtkIconViewCellInfo *cell_info,
                                          GdkRectangle *cell_area );
static PtkIconViewCellInfo *ptk_icon_view_get_cell_info ( PtkIconView *icon_view,
                                                          GtkCellRenderer *renderer );
static void ptk_icon_view_set_cell_data ( const PtkIconView *icon_view,
                                          PtkIconViewItem *item );
static void ptk_icon_view_cell_layout_pack_start ( GtkCellLayout *layout,
                                                   GtkCellRenderer *renderer,
                                                   gboolean expand );
static void ptk_icon_view_cell_layout_pack_end ( GtkCellLayout *layout,
                                                 GtkCellRenderer *renderer,
                                                 gboolean expand );
static void ptk_icon_view_cell_layout_add_attribute ( GtkCellLayout *layout,
                                                      GtkCellRenderer *renderer,
                                                      const gchar *attribute,
                                                      gint column );
static void ptk_icon_view_cell_layout_clear ( GtkCellLayout *layout );
static void ptk_icon_view_cell_layout_clear_attributes ( GtkCellLayout *layout,
                                                         GtkCellRenderer *renderer );
static void ptk_icon_view_cell_layout_set_cell_data_func ( GtkCellLayout *layout,
                                                           GtkCellRenderer *cell,
                                                           GtkCellLayoutDataFunc func,
                                                           gpointer func_data,
                                                           GDestroyNotify destroy );
static void ptk_icon_view_cell_layout_reorder ( GtkCellLayout *layout,
                                                GtkCellRenderer *cell,
                                                gint position );
static void ptk_icon_view_item_activate_cell ( PtkIconView *icon_view,
                                               PtkIconViewItem *item,
                                               PtkIconViewCellInfo *cell_info,
                                               GdkEvent *event );
static void ptk_icon_view_put ( PtkIconView *icon_view,
                                GtkWidget *widget,
                                PtkIconViewItem *item,
                                gint cell );
static void ptk_icon_view_remove_widget ( GtkCellEditable *editable,
                                          PtkIconView *icon_view );
static void ptk_icon_view_start_editing ( PtkIconView *icon_view,
                                          PtkIconViewItem *item,
                                          PtkIconViewCellInfo *cell_info,
                                          GdkEvent *event );
static void ptk_icon_view_stop_editing ( PtkIconView *icon_view,
                                         gboolean cancel_editing );

/* Source side drag signals */
static void ptk_icon_view_drag_begin ( GtkWidget *widget,
                                       GdkDragContext *context );
static void ptk_icon_view_drag_end ( GtkWidget *widget,
                                     GdkDragContext *context );
static void ptk_icon_view_drag_data_get ( GtkWidget *widget,
                                          GdkDragContext *context,
                                          GtkSelectionData *selection_data,
                                          guint info,
                                          guint time );
static void ptk_icon_view_drag_data_delete ( GtkWidget *widget,
                                             GdkDragContext *context );

/* Target side drag signals */
static void ptk_icon_view_drag_leave ( GtkWidget *widget,
                                       GdkDragContext *context,
                                       guint time );
static gboolean ptk_icon_view_drag_motion ( GtkWidget *widget,
                                            GdkDragContext *context,
                                            gint x,
                                            gint y,
                                            guint time );
static gboolean ptk_icon_view_drag_drop ( GtkWidget *widget,
                                          GdkDragContext *context,
                                          gint x,
                                          gint y,
                                          guint time );
static void ptk_icon_view_drag_data_received ( GtkWidget *widget,
                                               GdkDragContext *context,
                                               gint x,
                                               gint y,
                                               GtkSelectionData *selection_data,
                                               guint info,
                                               guint time );
static gboolean ptk_icon_view_maybe_begin_drag ( PtkIconView *icon_view,
                                                 GdkEventMotion *event );

static void remove_scroll_timeout ( PtkIconView *icon_view );



/* interactive search */
static void ptk_icon_view_ensure_interactive_directory ( PtkIconView *icon_view );
static void send_focus_change ( GtkWidget *widget, gboolean in );

static void ptk_icon_view_search_dialog_hide ( GtkWidget *search_dialog,
                                               PtkIconView *icon_view );
static void ptk_icon_view_search_position_func ( PtkIconView *icon_view,
                                                 GtkWidget *search_dialog );
static void ptk_icon_view_search_disable_popdown ( GtkEntry *entry,
                                                   GtkMenu *menu,
                                                   gpointer data );
static void ptk_icon_view_search_preedit_changed ( GtkIMContext *im_context,
                                                   PtkIconView *icon_view );
static void ptk_icon_view_search_activate ( GtkEntry *entry,
                                            PtkIconView *icon_view );
static gboolean ptk_icon_view_real_search_enable_popdown( gpointer data );
static void ptk_icon_view_search_enable_popdown ( GtkWidget *widget,
                                                  gpointer data );
static gboolean ptk_icon_view_search_delete_event ( GtkWidget *widget,
                                                    GdkEventAny *event,
                                                    PtkIconView *icon_view );
static gboolean ptk_icon_view_search_button_press_event ( GtkWidget *widget,
                                                          GdkEventButton *event,
                                                          PtkIconView *icon_view );
static gboolean ptk_icon_view_search_scroll_event ( GtkWidget *entry,
                                                    GdkEventScroll *event,
                                                    PtkIconView *icon_view );
static gboolean ptk_icon_view_search_key_press_event ( GtkWidget *entry,
                                                       GdkEventKey *event,
                                                       PtkIconView *icon_view );
static void ptk_icon_view_search_move ( GtkWidget *window,
                                        PtkIconView *icon_view,
                                        gboolean up );
static gboolean ptk_icon_view_search_equal_func ( GtkTreeModel *model,
                                                  gint column,
                                                  const gchar *key,
                                                  GtkTreeIter *iter,
                                                  gpointer search_data );
static gboolean ptk_icon_view_search_iter ( PtkIconView *icon_view,
                                            GtkTreeIter *iter,
                                            const gchar *text,
                                            gint *count,
                                            gint n );
static void ptk_icon_view_search_init ( GtkWidget *entry,
                                        PtkIconView *icon_view );


struct _PtkIconViewCellInfo
{
    GtkCellRenderer *cell;
guint expand :
    1;
guint pack :
    1;
guint editing :
    1;
    gint position;
    GSList *attributes;
    GtkCellLayoutDataFunc func;
    gpointer func_data;
    GDestroyNotify destroy;
};

struct _PtkIconViewChild
{
    PtkIconViewItem *item;
    GtkWidget *widget;
    gint cell;
};

struct _PtkIconViewItem
{
    GtkTreeIter iter;
    gint index;

    gint row, col;

    /* Bounding box */
    GdkRectangle area;

    /* Individual cells.
     * box[i] is the actual area occupied by cell i,
     * before, after are used to calculate the cell
     * area relative to the box.
     * See ptk_icon_view_get_cell_area().
     */
    gint n_cells;
    GdkRectangle *box;
    gint *before;
    gint *after;

guint selected :
    1;
guint selected_before_rubberbanding :
    1;
};

typedef void ( *PtkIconViewSearchDialogPositionFunc ) ( PtkIconView *icon_view,
                                                        GtkWidget *search_dialog );

struct _PtkIconViewPrivate
{
    gint width, height;
    GtkSelectionMode selection_mode;
    GdkWindow *bin_window;
    GList *children;
    GtkTreeModel *model;
#if ! GLIB_CHECK_VERSION(2, 10, 0)
    GMemChunk *items_chunk;
#endif

    GList *items;

    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;

    gint layout_idle_id;

    gboolean doing_rubberband;
    gint rubberband_x1, rubberband_y1;
    gint rubberband_x2, rubberband_y2;
    GdkGC *rubberband_border_gc;
    GdkGC *rubberband_fill_gc;

    gint scroll_timeout_id;
    gint scroll_value_diff;
    gint event_last_x, event_last_y;

    PtkIconViewItem *anchor_item;
    PtkIconViewItem *cursor_item;
    PtkIconViewItem *edited_item;
    GtkCellEditable *editable;
    PtkIconViewItem *prelit_item;

    PtkIconViewItem *last_single_clicked;

    GList *cell_list;
    guint n_cells;

    gint cursor_cell;

    GtkOrientation orientation;

    gint columns;
    gint item_width;
    gint spacing;
    gint row_spacing;
    gint column_spacing;
    gint margin;

    gint text_column;
    gint markup_column;
    gint pixbuf_column;

    gint pixbuf_cell;
    gint text_cell;

    /* Drag-and-drop. */
    GdkModifierType start_button_mask;
    gint pressed_button;
    gint press_start_x;
    gint press_start_y;

    GtkTargetList *source_targets;
    GdkDragAction source_actions;

    GtkTargetList *dest_targets;
    GdkDragAction dest_actions;

    GtkTreeRowReference *dest_item;
    PtkIconViewDropPosition dest_pos;

guint source_set :
    1;
guint dest_set :
    1;
guint reorderable :
    1;
guint empty_view_drop :
    1;

guint ctrl_pressed :
    1;
guint shift_pressed :
    1;

    /* interactive search */
guint enable_search :
    1;
guint disable_popdown :
    1;
guint hover_selection :
    1;
guint hover_expand :
    1;
guint imcontext_changed :
    1;

    gint selected_iter;
    gint search_column;
    PtkIconViewSearchDialogPositionFunc search_dialog_position_func;
    PtkIconViewSearchEqualFunc search_equal_func;
    gpointer search_user_data;
    GtkDestroyNotify search_destroy;
    GtkWidget *search_window;
    GtkWidget *search_entry;
    guint search_entry_changed_id;
    guint typeselect_flush_timeout;

    /* PtkIconViewFlags */
    guint flags;

    /* background pixmap */
    GdkPixmap* pixmap;
    int pixmap_x_offset;
    int pixmap_y_offset;
};



static GObjectClass *ptk_icon_view_parent_class;
static guint icon_view_signals[ LAST_SIGNAL ];



GType
ptk_icon_view_get_type ( void )
{
    static GType type = G_TYPE_INVALID;

    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( PtkIconViewClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) ptk_icon_view_class_init,
                NULL,
                NULL,
                sizeof ( PtkIconView ),
                0,
                ( GInstanceInitFunc ) ptk_icon_view_init,
                NULL,
            };

        static const GInterfaceInfo cell_layout_info =
            {
                ( GInterfaceInitFunc ) ptk_icon_view_cell_layout_init,
                NULL,
                NULL,
            };

        type = g_type_register_static ( GTK_TYPE_CONTAINER, I_( "PtkIconView" ), &info, 0 );
        g_type_add_interface_static ( type, GTK_TYPE_CELL_LAYOUT, &cell_layout_info );
    }

    return type;
}



static void
ptk_icon_view_class_init ( PtkIconViewClass *klass )
{
    GtkContainerClass * gtkcontainer_class;
    GtkWidgetClass *gtkwidget_class;
    GtkBindingSet *gtkbinding_set;
    GObjectClass *gobject_class;

    gtkbinding_set = gtk_binding_set_by_class ( klass );

    g_type_class_add_private ( klass, sizeof ( PtkIconViewPrivate ) );

    ptk_icon_view_parent_class = g_type_class_peek_parent ( klass );

    gobject_class = G_OBJECT_CLASS ( klass );
    gobject_class->dispose = ptk_icon_view_dispose;
    gobject_class->finalize = ptk_icon_view_finalize;
    gobject_class->set_property = ptk_icon_view_set_property;
    gobject_class->get_property = ptk_icon_view_get_property;

    gtkwidget_class = GTK_WIDGET_CLASS ( klass );
    gtkwidget_class->realize = ptk_icon_view_realize;
    gtkwidget_class->unrealize = ptk_icon_view_unrealize;
    gtkwidget_class->size_request = ptk_icon_view_size_request;
    gtkwidget_class->size_allocate = ptk_icon_view_size_allocate;
    gtkwidget_class->style_set = ptk_icon_view_style_set;
    gtkwidget_class->expose_event = ptk_icon_view_expose_event;
    gtkwidget_class->key_press_event = ptk_icon_view_key_press;
    gtkwidget_class->key_release_event = ptk_icon_view_key_release;
    gtkwidget_class->motion_notify_event = ptk_icon_view_motion_notify_event;
    gtkwidget_class->button_press_event = ptk_icon_view_button_press_event;
    gtkwidget_class->button_release_event = ptk_icon_view_button_release_event;
    gtkwidget_class->drag_begin = ptk_icon_view_drag_begin;
    gtkwidget_class->drag_end = ptk_icon_view_drag_end;
    gtkwidget_class->drag_data_get = ptk_icon_view_drag_data_get;
    gtkwidget_class->drag_data_delete = ptk_icon_view_drag_data_delete;
    gtkwidget_class->drag_leave = ptk_icon_view_drag_leave;
    gtkwidget_class->drag_motion = ptk_icon_view_drag_motion;
    gtkwidget_class->drag_drop = ptk_icon_view_drag_drop;
    gtkwidget_class->drag_data_received = ptk_icon_view_drag_data_received;
    gtkwidget_class->get_accessible = ptk_icon_view_get_accessible;

    gtkcontainer_class = GTK_CONTAINER_CLASS ( klass );
    gtkcontainer_class->remove
    = ptk_icon_view_remove;
    gtkcontainer_class->forall = ptk_icon_view_forall;

    klass->set_scroll_adjustments = ptk_icon_view_set_adjustments;
    klass->select_all = ptk_icon_view_real_select_all;
    klass->unselect_all = ptk_icon_view_real_unselect_all;
    klass->select_cursor_item = ptk_icon_view_real_select_cursor_item;
    klass->toggle_cursor_item = ptk_icon_view_real_toggle_cursor_item;
    klass->activate_cursor_item = ptk_icon_view_real_activate_cursor_item;
    klass->move_cursor = ptk_icon_view_real_move_cursor;

    /**
     * PtkIconView::column-spacing:
     *
     * The column-spacing property specifies the space which is inserted between
     * the columns of the icon view.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_COLUMN_SPACING,
                                      g_param_spec_int ( "column-spacing",
                                                         _( "Column Spacing" ),
                                                         _( "Space which is inserted between grid column" ),
                                                         0, G_MAXINT, 6,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::columns:
     *
     * The columns property contains the number of the columns in which the
     * items should be displayed. If it is -1, the number of columns will
     * be chosen automatically to fill the available area.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_COLUMNS,
                                      g_param_spec_int ( "columns",
                                                         _( "Number of columns" ),
                                                         _( "Number of columns to display" ),
                                                         -1, G_MAXINT, -1,
                                                         PTK_PARAM_READWRITE ) );


    /**
     * PtkIconView::item-width:
     *
     * The item-width property specifies the width to use for each item.
     * If it is set to -1, the icon view will automatically determine a
     * suitable item size.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_ITEM_WIDTH,
                                      g_param_spec_int ( "item-width",
                                                         _( "Width for each item" ),
                                                         _( "The width used for each item" ),
                                                         -1, G_MAXINT, -1,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::margin:
     *
     * The margin property specifies the space which is inserted
     * at the edges of the icon view.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_MARGIN,
                                      g_param_spec_int ( "margin",
                                                         _( "Margin" ),
                                                         _( "Space which is inserted at the edges of the icon view" ),
                                                         0, G_MAXINT, 6,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::markup-column:
     *
     * The ::markup-column property contains the number of the model column
     * containing markup information to be displayed. The markup column must be
     * of type #G_TYPE_STRING. If this property and the :text-column property
     * are both set to column numbers, it overrides the text column.
     * If both are set to -1, no texts are displayed.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_MARKUP_COLUMN,
                                      g_param_spec_int ( "markup-column",
                                                         _( "Markup column" ),
                                                         _( "Model column used to retrieve the text if using Pango markup" ),
                                                         -1, G_MAXINT, -1,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::model:
     *
     * The ::model property contains the #GtkTreeModel, which should be
     * display by this icon view. Setting this property to %NULL turns
     * off the display of anything.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_MODEL,
                                      g_param_spec_object ( "model",
                                                            _( "Icon View Model" ),
                                                            _( "The model for the icon view" ),
                                                            GTK_TYPE_TREE_MODEL,
                                                            PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::orientation:
     *
     * The orientation property specifies how the cells (i.e. the icon and
     * the text) of the item are positioned relative to each other.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_ORIENTATION,
                                      g_param_spec_enum ( "orientation",
                                                          _( "Orientation" ),
                                                          _( "How the text and icon of each item are positioned relative to each other" ),
                                                          GTK_TYPE_ORIENTATION,
                                                          GTK_ORIENTATION_VERTICAL,
                                                          PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::pixbuf-column:
     *
     * The ::pixbuf-column property contains the number of the model column
     * containing the pixbufs which are displayed. The pixbuf column must be
     * of type #GDK_TYPE_PIXBUF. Setting this property to -1 turns off the
     * display of pixbufs.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_PIXBUF_COLUMN,
                                      g_param_spec_int ( "pixbuf-column",
                                                         _( "Pixbuf column" ),
                                                         _( "Model column used to retrieve the icon pixbuf from" ),
                                                         -1, G_MAXINT, -1,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::reorderable:
     *
     * The reorderable property specifies if the items can be reordered
     * by Drag and Drop.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_REORDERABLE,
                                      g_param_spec_boolean ( "reorderable",
                                                             _( "Reorderable" ),
                                                             _( "View is reorderable" ),
                                                             FALSE,
                                                             PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::row-spacing:
     *
     * The row-spacing property specifies the space which is inserted between
     * the rows of the icon view.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_ROW_SPACING,
                                      g_param_spec_int ( "row-spacing",
                                                         _( "Row Spacing" ),
                                                         _( "Space which is inserted between grid rows" ),
                                                         0, G_MAXINT, 6,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::selection-mode:
     *
     * The ::selection-mode property specifies the selection mode of
     * icon view. If the mode is #GTK_SELECTION_MULTIPLE, rubberband selection
     * is enabled, for the other modes, only keyboard selection is possible.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_SELECTION_MODE,
                                      g_param_spec_enum ( "selection-mode",
                                                          _( "Selection mode" ),
                                                          _( "The selection mode" ),
                                                          GTK_TYPE_SELECTION_MODE,
                                                          GTK_SELECTION_SINGLE,
                                                          PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::spacing:
     *
     * The spacing property specifies the space which is inserted between
     * the cells (i.e. the icon and the text) of an item.
     *
     * Since: 0.3.1
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_SPACING,
                                      g_param_spec_int ( "spacing",
                                                         _( "Spacing" ),
                                                         _( "Space which is inserted between cells of an item" ),
                                                         0, G_MAXINT, 0,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::text-column:
     *
     * The ::text-column property contains the number of the model column
     * containing the texts which are displayed. The text column must be
     * of type #G_TYPE_STRING. If this property and the :markup-column
     * property are both set to -1, no texts are displayed.
     **/
    g_object_class_install_property ( gobject_class,
                                      PROP_TEXT_COLUMN,
                                      g_param_spec_int ( "text-column",
                                                         _( "Text column" ),
                                                         _( "Model column used to retrieve the text from" ),
                                                         -1, G_MAXINT, -1,
                                                         PTK_PARAM_READWRITE ) );


    g_object_class_install_property ( gobject_class,
                                      PROP_ENABLE_SEARCH,
                                      g_param_spec_boolean ( "enable-search",
                                                             _( "Enable Search" ),
                                                             _( "View allows user to search through columns interactively" ),
                                                             TRUE,
                                                             PTK_PARAM_READWRITE ) );

    g_object_class_install_property ( gobject_class,
                                      PROP_SEARCH_COLUMN,
                                      g_param_spec_int ( "search-column",
                                                         _( "Search Column" ),
                                                         _( "Model column to search through when searching through code" ),
                                                         -1,
                                                         G_MAXINT,
                                                         -1,
                                                         PTK_PARAM_READWRITE ) );

    /**
     * PtkIconView::selection-box-color:
     * Color of the selection box.
     **/
    gtk_widget_class_install_style_property ( gtkwidget_class,
                                              g_param_spec_boxed ( "selection-box-color",
                                                                   _( "Selection Box Color" ),
                                                                   _( "Color of the selection box" ),
                                                                   GDK_TYPE_COLOR,
                                                                   PTK_PARAM_READABLE ) );

    /**
     * PtkIconView::selection-box-alpha:
     * Opacity of the selection box.
     **/
    gtk_widget_class_install_style_property ( gtkwidget_class,
                                              g_param_spec_uchar ( "selection-box-alpha",
                                                                   _( "Selection Box Alpha" ),
                                                                   _( "Opacity of the selection box" ),
                                                                   0, 0xff,
                                                                   0x40,
                                                                   PTK_PARAM_READABLE ) );

    /**
     * PtkIconView::item-activated:
     * @icon_view : a #PtkIconView.
     * @path      :
     **/
    icon_view_signals[ ITEM_ACTIVATED ] =
        g_signal_new ( I_( "item-activated" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkIconViewClass, item_activated ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__BOXED,
                       G_TYPE_NONE, 1,
                       GTK_TYPE_TREE_PATH );

    /**
     * PtkIconView::selection-changed:
     * @icon_view : a #PtkIconView.
     **/
    icon_view_signals[ SELECTION_CHANGED ] =
        g_signal_new ( I_( "selection-changed" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( PtkIconViewClass, selection_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );

    /**
     * PtkIconView::set-scroll-adjustments:
     * @icon_view   : a #PtkIconView.
     * @hadjustment :
     * @vadjustment :
     **/
    gtkwidget_class->set_scroll_adjustments_signal =
        g_signal_new ( I_( "set-scroll-adjustments" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkIconViewClass, set_scroll_adjustments ),
                       NULL, NULL,
                       _exo_marshal_VOID__OBJECT_OBJECT,
                       G_TYPE_NONE, 2,
                       GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT );

    /**
     * PtkIconView::select-all:
     * @icon_view : a #PtkIconView.
     **/
    icon_view_signals[ SELECT_ALL ] =
        g_signal_new ( I_( "select-all" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, select_all ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );

    /**
     * PtkIconView::unselect-all:
     * @icon_view : a #PtkIconView.
     **/
    icon_view_signals[ UNSELECT_ALL ] =
        g_signal_new ( I_( "unselect-all" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, unselect_all ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );

    /**
     * PtkIconView::select-cursor-item:
     * @icon_view : a #PtkIconView.
     **/
    icon_view_signals[ SELECT_CURSOR_ITEM ] =
        g_signal_new ( I_( "select-cursor-item" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, select_cursor_item ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );

    /**
     * PtkIconView::toggle-cursor-item:
     * @icon_view : a #PtkIconView.
     **/
    icon_view_signals[ TOGGLE_CURSOR_ITEM ] =
        g_signal_new ( I_( "toggle-cursor-item" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, toggle_cursor_item ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0 );

    /**
     * PtkIconView::activate-cursor-item:
     * @icon_view : a #PtkIconView.
     *
     * Return value:
     **/
    icon_view_signals[ ACTIVATE_CURSOR_ITEM ] =
        g_signal_new ( I_( "activate-cursor-item" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, activate_cursor_item ),
                       NULL, NULL,
                       _exo_marshal_BOOLEAN__VOID,
                       G_TYPE_BOOLEAN, 0 );

    /**
     * PtkIconView::move-cursor:
     * @icon_view : a #PtkIconView.
     * @step      :
     * @count     :
     *
     * Return value:
     **/
    icon_view_signals[ MOVE_CURSOR ] =
        g_signal_new ( I_( "move-cursor" ),
                       G_TYPE_FROM_CLASS ( gobject_class ),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET ( PtkIconViewClass, move_cursor ),
                       NULL, NULL,
                       _exo_marshal_BOOLEAN__ENUM_INT,
                       G_TYPE_BOOLEAN, 2,
                       GTK_TYPE_MOVEMENT_STEP,
                       G_TYPE_INT );

    /* Key bindings */
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_a, GDK_CONTROL_MASK, "select-all", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "unselect-all", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_space, GDK_CONTROL_MASK, "toggle-cursor-item", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_space, 0, "activate-cursor-item", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_Return, 0, "activate-cursor-item", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_ISO_Enter, 0, "activate-cursor-item", 0 );
    gtk_binding_entry_add_signal ( gtkbinding_set, GDK_KP_Enter, 0, "activate-cursor-item", 0 );

    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Up, 0, GTK_MOVEMENT_DISPLAY_LINES, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Down, 0, GTK_MOVEMENT_DISPLAY_LINES, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_p, GDK_CONTROL_MASK, GTK_MOVEMENT_DISPLAY_LINES, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_n, GDK_CONTROL_MASK, GTK_MOVEMENT_DISPLAY_LINES, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Home, 0, GTK_MOVEMENT_BUFFER_ENDS, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_End, 0, GTK_MOVEMENT_BUFFER_ENDS, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Page_Up, 0, GTK_MOVEMENT_PAGES, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Page_Up, 0, GTK_MOVEMENT_PAGES, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Page_Down, 0, GTK_MOVEMENT_PAGES, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Page_Down, 0, GTK_MOVEMENT_PAGES, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Right, 0, GTK_MOVEMENT_VISUAL_POSITIONS, 1 );
    ptk_icon_view_add_move_binding ( gtkbinding_set, GDK_KP_Left, 0, GTK_MOVEMENT_VISUAL_POSITIONS, -1 );
}



static void
ptk_icon_view_cell_layout_init ( GtkCellLayoutIface *iface )
{
    iface->pack_start = ptk_icon_view_cell_layout_pack_start;
    iface->pack_end = ptk_icon_view_cell_layout_pack_end;
    iface->clear = ptk_icon_view_cell_layout_clear;
    iface->add_attribute = ptk_icon_view_cell_layout_add_attribute;
    iface->set_cell_data_func = ptk_icon_view_cell_layout_set_cell_data_func;
    iface->clear_attributes = ptk_icon_view_cell_layout_clear_attributes;
    iface->reorder = ptk_icon_view_cell_layout_reorder;
}



static void
ptk_icon_view_init ( PtkIconView *icon_view )
{
    icon_view->priv = PTK_ICON_VIEW_GET_PRIVATE ( icon_view );

#if ! GLIB_CHECK_VERSION( 2, 10, 0 )
    icon_view->priv->items_chunk = g_mem_chunk_create ( PtkIconViewItem,
                                                        512, G_ALLOC_ONLY );
#endif

    icon_view->priv->width = 0;
    icon_view->priv->height = 0;
    icon_view->priv->selection_mode = GTK_SELECTION_SINGLE;
    icon_view->priv->pressed_button = -1;
    icon_view->priv->press_start_x = -1;
    icon_view->priv->press_start_y = -1;
    icon_view->priv->text_column = -1;
    icon_view->priv->markup_column = -1;
    icon_view->priv->pixbuf_column = -1;
    icon_view->priv->text_cell = -1;
    icon_view->priv->pixbuf_cell = -1;

    GTK_WIDGET_SET_FLAGS ( icon_view, GTK_CAN_FOCUS );

    ptk_icon_view_set_adjustments ( icon_view, NULL, NULL );

    icon_view->priv->cell_list = NULL;
    icon_view->priv->n_cells = 0;
    icon_view->priv->cursor_cell = -1;

    icon_view->priv->orientation = GTK_ORIENTATION_VERTICAL;

    icon_view->priv->columns = -1;
    icon_view->priv->item_width = -1;
    icon_view->priv->spacing = 0;
    icon_view->priv->row_spacing = 6;
    icon_view->priv->column_spacing = 6;
    icon_view->priv->margin = 6;

    icon_view->priv->layout_idle_id = -1;

    icon_view->priv->flags = PTK_ICON_VIEW_DRAW_KEYFOCUS;

    /* Interactive Search */
    icon_view->priv->enable_search = TRUE;
    icon_view->priv->search_column = -1;
    icon_view->priv->search_dialog_position_func = ptk_icon_view_search_position_func;
    icon_view->priv->search_equal_func = ptk_icon_view_search_equal_func;
}



static void
ptk_icon_view_dispose ( GObject *object )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( object );

    /* reset the drag dest item */
    ptk_icon_view_set_drag_dest_item ( icon_view, NULL, PTK_ICON_VIEW_NO_DROP );

    /* reset the model (also stops any active editing) */
    ptk_icon_view_set_model ( icon_view, NULL );

    /* drop the scroll timer */
    remove_scroll_timeout ( icon_view );

    ( *G_OBJECT_CLASS ( ptk_icon_view_parent_class ) ->dispose ) ( object );
}



static void
ptk_icon_view_finalize ( GObject *object )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( object );

    /* drop the scroll adjustments */
    g_object_unref ( G_OBJECT ( icon_view->priv->hadjustment ) );
    g_object_unref ( G_OBJECT ( icon_view->priv->vadjustment ) );

    /* drop the cell renderers */
    ptk_icon_view_cell_layout_clear ( GTK_CELL_LAYOUT ( icon_view ) );

#if ! GLIB_CHECK_VERSION(2, 10, 0)
    /* drop the items chunk */
    g_mem_chunk_destroy ( icon_view->priv->items_chunk );
#endif

    /* kill the layout idle source (it's important to have this last!) */
    if ( G_UNLIKELY ( icon_view->priv->layout_idle_id >= 0 ) )
        g_source_remove ( icon_view->priv->layout_idle_id );

    /* unref background pixmap if it exists */
    if ( icon_view->priv->pixmap )
        g_object_unref( G_OBJECT( icon_view->priv->pixmap ) );

    ( *G_OBJECT_CLASS ( ptk_icon_view_parent_class ) ->finalize ) ( object );
}


static void
ptk_icon_view_get_property ( GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( object );

    switch ( prop_id )
    {
    case PROP_COLUMN_SPACING:
        g_value_set_int ( value, icon_view->priv->column_spacing );
        break;

    case PROP_COLUMNS:
        g_value_set_int ( value, icon_view->priv->columns );
        break;

    case PROP_ITEM_WIDTH:
        g_value_set_int ( value, icon_view->priv->item_width );
        break;

    case PROP_MARGIN:
        g_value_set_int ( value, icon_view->priv->margin );
        break;

    case PROP_MARKUP_COLUMN:
        g_value_set_int ( value, icon_view->priv->markup_column );
        break;

    case PROP_MODEL:
        g_value_set_object ( value, icon_view->priv->model );
        break;

    case PROP_ORIENTATION:
        g_value_set_enum ( value, icon_view->priv->orientation );
        break;

    case PROP_PIXBUF_COLUMN:
        g_value_set_int ( value, icon_view->priv->pixbuf_column );
        break;

    case PROP_REORDERABLE:
        g_value_set_boolean ( value, icon_view->priv->reorderable );
        break;

    case PROP_ROW_SPACING:
        g_value_set_int ( value, icon_view->priv->row_spacing );
        break;

    case PROP_SELECTION_MODE:
        g_value_set_enum ( value, icon_view->priv->selection_mode );
        break;

    case PROP_SPACING:
        g_value_set_int ( value, icon_view->priv->spacing );
        break;

    case PROP_TEXT_COLUMN:
        g_value_set_int ( value, icon_view->priv->text_column );
        break;

    case PROP_ENABLE_SEARCH:
        g_value_set_boolean ( value, icon_view->priv->enable_search );
        break;
    case PROP_SEARCH_COLUMN:
        g_value_set_int ( value, icon_view->priv->search_column );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, prop_id, pspec );
        break;
    }
}



static void
ptk_icon_view_set_property ( GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( object );

    switch ( prop_id )
    {
    case PROP_COLUMN_SPACING:
        ptk_icon_view_set_column_spacing ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_COLUMNS:
        ptk_icon_view_set_columns ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_ITEM_WIDTH:
        ptk_icon_view_set_item_width ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_MARGIN:
        ptk_icon_view_set_margin ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_MARKUP_COLUMN:
        ptk_icon_view_set_markup_column ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_MODEL:
        ptk_icon_view_set_model ( icon_view, g_value_get_object ( value ) );
        break;

    case PROP_ORIENTATION:
        ptk_icon_view_set_orientation ( icon_view, g_value_get_enum ( value ) );
        break;

    case PROP_PIXBUF_COLUMN:
        ptk_icon_view_set_pixbuf_column ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_REORDERABLE:
        ptk_icon_view_set_reorderable ( icon_view, g_value_get_boolean ( value ) );
        break;

    case PROP_ROW_SPACING:
        ptk_icon_view_set_row_spacing ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_SELECTION_MODE:
        ptk_icon_view_set_selection_mode ( icon_view, g_value_get_enum ( value ) );
        break;

    case PROP_SPACING:
        ptk_icon_view_set_spacing ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_TEXT_COLUMN:
        ptk_icon_view_set_text_column ( icon_view, g_value_get_int ( value ) );
        break;

    case PROP_ENABLE_SEARCH:
        ptk_icon_view_set_enable_search ( icon_view, g_value_get_boolean ( value ) );
        break;

    case PROP_SEARCH_COLUMN:
        ptk_icon_view_set_search_column ( icon_view, g_value_get_int ( value ) );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, prop_id, pspec );
        break;
    }
}



static void
ptk_icon_view_realize ( GtkWidget *widget )
{
    GdkWindowAttr attributes;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );
    gint attributes_mask;

    GTK_WIDGET_SET_FLAGS ( widget, GTK_REALIZED );

    /* Allocate the clipping window */
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual ( widget );
    attributes.colormap = gtk_widget_get_colormap ( widget );
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new ( gtk_widget_get_parent_window ( widget ), &attributes, attributes_mask );
    gdk_window_set_user_data ( widget->window, widget );

    /* Allocate the icons window */
    attributes.x = 0;
    attributes.y = 0;
    attributes.width = MAX ( icon_view->priv->width, widget->allocation.width );
    attributes.height = MAX ( icon_view->priv->height, widget->allocation.height );
    attributes.event_mask = GDK_EXPOSURE_MASK
                            | GDK_SCROLL_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_KEY_PRESS_MASK
                            | GDK_KEY_RELEASE_MASK
                            | gtk_widget_get_events ( widget );
    icon_view->priv->bin_window = gdk_window_new ( widget->window, &attributes, attributes_mask );
    gdk_window_set_user_data ( icon_view->priv->bin_window, widget );

    /* Attach style/background */
    widget->style = gtk_style_attach ( widget->style, widget->window );
    gdk_window_set_background ( icon_view->priv->bin_window, &widget->style->base[ widget->state ] );
    gdk_window_set_background ( widget->window, &widget->style->base[ widget->state ] );

    /* map the icons window */
    gdk_window_show ( icon_view->priv->bin_window );
}



static void
ptk_icon_view_unrealize ( GtkWidget *widget )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( widget );

    /* drop the icons window */
    gdk_window_set_user_data ( icon_view->priv->bin_window, NULL );
    gdk_window_destroy ( icon_view->priv->bin_window );
    icon_view->priv->bin_window = NULL;

    /* let GtkWidget destroy children and widget->window */
    if ( GTK_WIDGET_CLASS ( ptk_icon_view_parent_class ) ->unrealize )
        ( *GTK_WIDGET_CLASS ( ptk_icon_view_parent_class ) ->unrealize ) ( widget );
}



static void
ptk_icon_view_size_request ( GtkWidget *widget,
                             GtkRequisition *requisition )
{
    PtkIconViewChild * child;
    GtkRequisition child_requisition;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );
    GList *lp;

    /* well, this is easy */
    requisition->width = icon_view->priv->width;
    requisition->height = icon_view->priv->height;

    /* handle the child widgets */
    for ( lp = icon_view->priv->children; lp != NULL; lp = lp->next )
    {
        child = lp->data;
        if ( GTK_WIDGET_VISIBLE ( child->widget ) )
            gtk_widget_size_request ( child->widget, &child_requisition );
    }
}



static void
ptk_icon_view_allocate_children ( PtkIconView *icon_view )
{
    const PtkIconViewChild * child;
    GtkAllocation allocation;
    const GList *lp;
    gint focus_line_width;
    gint focus_padding;

    for ( lp = icon_view->priv->children; lp != NULL; lp = lp->next )
    {
        child = PTK_ICON_VIEW_CHILD ( lp->data );

        /* totally ignore our child's requisition */
        if ( child->cell < 0 )
            allocation = child->item->area;
        else
            allocation = child->item->box[ child->cell ];

        /* increase the item area by focus width/padding */
        gtk_widget_style_get ( GTK_WIDGET ( icon_view ), "focus-line-width", &focus_line_width, "focus-padding", &focus_padding, NULL );
        allocation.x = MAX ( 0, allocation.x - ( focus_line_width + focus_padding ) );
        allocation.y = MAX ( 0, allocation.y - ( focus_line_width + focus_padding ) );
        allocation.width = MIN ( icon_view->priv->width - allocation.x, allocation.width + 2 * ( focus_line_width + focus_padding ) );
        allocation.height = MIN ( icon_view->priv->height - allocation.y, allocation.height + 2 * ( focus_line_width + focus_padding ) );

        /* allocate the area to the child */
        gtk_widget_size_allocate ( child->widget, &allocation );
    }
}



static void
ptk_icon_view_size_allocate ( GtkWidget *widget,
                              GtkAllocation *allocation )
{
    GtkAdjustment * hadjustment;
    GtkAdjustment *vadjustment;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );

    /* apply the new size allocation */
    widget->allocation = *allocation;

    /* move/resize the clipping window, the icons window
     * will be handled by ptk_icon_view_layout().
     */
    if ( GTK_WIDGET_REALIZED ( widget ) )
        gdk_window_move_resize ( widget->window, allocation->x, allocation->y, allocation->width, allocation->height );

    /* layout the items */
    ptk_icon_view_layout ( icon_view );

    /* allocate space to the widgets (editing) */
    ptk_icon_view_allocate_children ( icon_view );

    /* update the horizontal scroll adjustment accordingly */
    hadjustment = icon_view->priv->hadjustment;
    hadjustment->page_size = allocation->width;
    hadjustment->page_increment = allocation->width * 0.9;
    hadjustment->step_increment = allocation->width * 0.1;
    hadjustment->lower = 0;
    hadjustment->upper = MAX ( allocation->width, icon_view->priv->width );
    if ( hadjustment->value > hadjustment->upper - hadjustment->page_size )
        gtk_adjustment_set_value ( hadjustment, MAX ( 0, hadjustment->upper - hadjustment->page_size ) );
    gtk_adjustment_changed ( hadjustment );

    /* update the vertical scroll adjustment accordingly */
    vadjustment = icon_view->priv->vadjustment;
    vadjustment->page_size = allocation->height;
    vadjustment->page_increment = allocation->height * 0.9;
    vadjustment->step_increment = allocation->height * 0.1;
    vadjustment->lower = 0;
    vadjustment->upper = MAX ( allocation->height, icon_view->priv->height );
    if ( vadjustment->value > vadjustment->upper - vadjustment->page_size )
        gtk_adjustment_set_value ( vadjustment, MAX ( 0, vadjustment->upper - vadjustment->page_size ) );
    gtk_adjustment_changed ( vadjustment );
}



static void
ptk_icon_view_style_set ( GtkWidget *widget,
                          GtkStyle *previous_style )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( widget );

    /* let GtkWidget do its work */
    ( *GTK_WIDGET_CLASS ( ptk_icon_view_parent_class ) ->style_set ) ( widget, previous_style );

    /* apply the new style for the bin_window if we're realized */
    if ( GTK_WIDGET_REALIZED ( widget ) )
        gdk_window_set_background ( icon_view->priv->bin_window, &widget->style->base[ widget->state ] );
}



static gboolean
ptk_icon_view_expose_event ( GtkWidget *widget,
                             GdkEventExpose *event )
{
    PtkIconViewDropPosition dest_pos;
    PtkIconViewItem *dest_item = NULL;
    PtkIconViewItem *item;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );
    GtkTreePath *path;
    GdkRectangle rubber_rect;
    const GList *lp;
    gint dest_index = -1;

    /* verify that the expose happened on the icon window */
    if ( G_UNLIKELY ( event->window != icon_view->priv->bin_window ) )
        return FALSE;

    /* Paint background pixmap if it exists */
    if ( icon_view->priv->pixmap )
    {
        gdk_draw_drawable( event->window, widget->style->fg_gc[ GTK_STATE_NORMAL ],
                           icon_view->priv->pixmap, 
                           icon_view->priv->pixmap_x_offset + event->area.x,
                           icon_view->priv->pixmap_y_offset + event->area.y,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height );
    }
    /* don't handle expose if the layout isn't done yet; the layout
     * method will schedule a redraw when done.
     */
    if ( G_UNLIKELY ( icon_view->priv->layout_idle_id >= 0 ) )
        return FALSE;

    /* check if we need to draw a drag indicator */
    ptk_icon_view_get_drag_dest_item ( icon_view, &path, &dest_pos );
    if ( G_UNLIKELY ( path != NULL ) )
    {
        dest_index = gtk_tree_path_get_indices ( path ) [ 0 ];
        gtk_tree_path_free ( path );
    }

    /* paint the rubberband background */
    if ( G_UNLIKELY ( icon_view->priv->doing_rubberband ) )
    {
        /* calculate the rubberband area */
        rubber_rect.x = MIN ( icon_view->priv->rubberband_x1, icon_view->priv->rubberband_x2 );
        rubber_rect.y = MIN ( icon_view->priv->rubberband_y1, icon_view->priv->rubberband_y2 );
        rubber_rect.width = ABS ( icon_view->priv->rubberband_x1 - icon_view->priv->rubberband_x2 ) + 1;
        rubber_rect.height = ABS ( icon_view->priv->rubberband_y1 - icon_view->priv->rubberband_y2 ) + 1;

        /* we take advantage of double-buffering here and use only a single
         * draw_rectangle() operation w/o having to take care of clipping.
         */
        gdk_draw_rectangle ( event->window, icon_view->priv->rubberband_fill_gc, TRUE,
                             rubber_rect.x, rubber_rect.y, rubber_rect.width, rubber_rect.height );
    }

    /* paint all items that are affected by the expose event */
    for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
    {
        /* check if this item is in the visible area */
        item = PTK_ICON_VIEW_ITEM ( lp->data );
        if ( item->area.y > event->area.y + event->area.height )
            break;
        else if ( item->area.y + item->area.height < event->area.y )
            continue;

        /* check if this item needs an update */
        if ( G_LIKELY ( gdk_region_rect_in ( event->region, &item->area ) != GDK_OVERLAP_RECTANGLE_OUT ) )
        {
            ptk_icon_view_paint_item ( icon_view, item, &event->area, event->window, item->area.x, item->area.y, TRUE );
            if ( G_UNLIKELY ( dest_index == item->index ) )
                dest_item = item;
        }
    }

    /* draw the drag indicator */
    if ( G_UNLIKELY ( dest_item != NULL ) )
    {
        switch ( dest_pos )
        {
        case PTK_ICON_VIEW_DROP_INTO:
            gtk_paint_focus ( widget->style, icon_view->priv->bin_window,
                              GTK_WIDGET_STATE ( widget ), NULL, widget,
                              "iconview-drop-indicator",
                              dest_item->area.x, dest_item->area.y,
                              dest_item->area.width, dest_item->area.height );
            break;

        case PTK_ICON_VIEW_DROP_ABOVE:
            gtk_paint_focus ( widget->style, icon_view->priv->bin_window,
                              GTK_WIDGET_STATE ( widget ), NULL, widget,
                              "iconview-drop-indicator",
                              dest_item->area.x, dest_item->area.y - 1,
                              dest_item->area.width, 2 );
            break;

        case PTK_ICON_VIEW_DROP_LEFT:
            gtk_paint_focus ( widget->style, icon_view->priv->bin_window,
                              GTK_WIDGET_STATE ( widget ), NULL, widget,
                              "iconview-drop-indicator",
                              dest_item->area.x - 1, dest_item->area.y,
                              2, dest_item->area.height );
            break;

        case PTK_ICON_VIEW_DROP_BELOW:
            gtk_paint_focus ( widget->style, icon_view->priv->bin_window,
                              GTK_WIDGET_STATE ( widget ), NULL, widget,
                              "iconview-drop-indicator",
                              dest_item->area.x, dest_item->area.y + dest_item->area.height - 1,
                              dest_item->area.width, 2 );
            break;

        case PTK_ICON_VIEW_DROP_RIGHT:
            gtk_paint_focus ( widget->style, icon_view->priv->bin_window,
                              GTK_WIDGET_STATE ( widget ), NULL, widget,
                              "iconview-drop-indicator",
                              dest_item->area.x + dest_item->area.width - 1, dest_item->area.y,
                              2, dest_item->area.height );

        case PTK_ICON_VIEW_NO_DROP:
            break;

        default:
            g_assert_not_reached ();
        }
    }

    /* draw the rubberband border */
    if ( G_UNLIKELY ( icon_view->priv->doing_rubberband ) )
    {
        /* draw the border */
        gdk_draw_rectangle ( event->window, icon_view->priv->rubberband_border_gc, FALSE,
                             rubber_rect.x, rubber_rect.y, rubber_rect.width - 1, rubber_rect.height - 1 );
    }

    /* let the GtkContainer forward the expose event to all children */
    ( *GTK_WIDGET_CLASS ( ptk_icon_view_parent_class ) ->expose_event ) ( widget, event );

    return FALSE;
}



static gboolean
rubberband_scroll_timeout ( gpointer user_data )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( user_data );
    gdouble value;

    GDK_THREADS_ENTER ();

    value = MIN ( icon_view->priv->vadjustment->value + icon_view->priv->scroll_value_diff,
                  icon_view->priv->vadjustment->upper - icon_view->priv->vadjustment->page_size );

    gtk_adjustment_set_value ( icon_view->priv->vadjustment, value );

    ptk_icon_view_update_rubberband ( icon_view );

    GDK_THREADS_LEAVE ();

    return TRUE;
}



static gboolean
ptk_icon_view_motion_notify_event ( GtkWidget *widget,
                                    GdkEventMotion *event )
{
    PtkIconViewItem * item;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );
    gint abs_y;

    ptk_icon_view_maybe_begin_drag ( icon_view, event );

    if ( icon_view->priv->doing_rubberband )
    {
        ptk_icon_view_update_rubberband ( widget );

        abs_y = event->y - icon_view->priv->height *
                ( icon_view->priv->vadjustment->value /
                  ( icon_view->priv->vadjustment->upper -
                    icon_view->priv->vadjustment->lower ) );

        if ( abs_y < 0 || abs_y > widget->allocation.height )
        {
            if ( abs_y < 0 )
                icon_view->priv->scroll_value_diff = abs_y;
            else
                icon_view->priv->scroll_value_diff = abs_y - widget->allocation.height;

            icon_view->priv->event_last_x = event->x;
            icon_view->priv->event_last_y = event->y;

            if ( icon_view->priv->scroll_timeout_id == 0 )
                icon_view->priv->scroll_timeout_id = g_timeout_add ( 30, rubberband_scroll_timeout,
                                                                     icon_view );
        }
        else
        {
            remove_scroll_timeout ( icon_view );
        }
    }
    /* We don't need prelit_item currently.
    else
    {
      item = ptk_icon_view_get_item_at_coords (icon_view, event->x, event->y, TRUE, NULL);
      if (item != icon_view->priv->prelit_item)
        {
          if (G_LIKELY (icon_view->priv->prelit_item != NULL))
            ptk_icon_view_queue_draw_item (icon_view, icon_view->priv->prelit_item);
          icon_view->priv->prelit_item = item;
          if (G_LIKELY (item != NULL))
            ptk_icon_view_queue_draw_item (icon_view, item);
        }
    }
    */ 
    return TRUE;
}



static void
ptk_icon_view_remove ( GtkContainer *container,
                       GtkWidget *widget )
{
    PtkIconViewChild * child;
    PtkIconView *icon_view = PTK_ICON_VIEW ( container );
    GList *lp;

    for ( lp = icon_view->priv->children; lp != NULL; lp = lp->next )
    {
        child = lp->data;
        if ( G_LIKELY ( child->widget == widget ) )
        {
            icon_view->priv->children = g_list_delete_link ( icon_view->priv->children, lp );
            gtk_widget_unparent ( widget );
            g_free ( child );
            return ;
        }
    }
}



static void
ptk_icon_view_forall ( GtkContainer *container,
                       gboolean include_internals,
                       GtkCallback callback,
                       gpointer callback_data )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( container );
    GList *lp;

    for ( lp = icon_view->priv->children; lp != NULL; lp = lp->next )
        ( *callback ) ( ( ( PtkIconViewChild * ) lp->data ) ->widget, callback_data );
}



static void
ptk_icon_view_item_activate_cell ( PtkIconView *icon_view,
                                   PtkIconViewItem *item,
                                   PtkIconViewCellInfo *info,
                                   GdkEvent *event )
{
    GtkCellRendererMode mode;
    GdkRectangle cell_area;
    GtkTreePath *path;
    gboolean visible;
    gchar *path_string;

    ptk_icon_view_set_cell_data ( icon_view, item );

    g_object_get ( G_OBJECT ( info->cell ), "visible", &visible, "mode", &mode, NULL );

    if ( G_UNLIKELY ( visible && mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE ) )
    {
        ptk_icon_view_get_cell_area ( icon_view, item, info, &cell_area );

        path = gtk_tree_path_new_from_indices ( item->index, -1 );
        path_string = gtk_tree_path_to_string ( path );
        gtk_tree_path_free ( path );

        gtk_cell_renderer_activate ( info->cell, event, GTK_WIDGET ( icon_view ), path_string, &cell_area, &cell_area, 0 );

        g_free ( path_string );
    }
}



static void
ptk_icon_view_put ( PtkIconView *icon_view,
                    GtkWidget *widget,
                    PtkIconViewItem *item,
                    gint cell )
{
    PtkIconViewChild * child;

    /* allocate the new child */
    child = g_new ( PtkIconViewChild, 1 );
    child->widget = widget;
    child->item = item;
    child->cell = cell;

    /* hook up the child */
    icon_view->priv->children = g_list_append ( icon_view->priv->children, child );

    /* setup the parent for the child */
    if ( GTK_WIDGET_REALIZED ( icon_view ) )
        gtk_widget_set_parent_window ( child->widget, icon_view->priv->bin_window );
    gtk_widget_set_parent ( widget, GTK_WIDGET ( icon_view ) );
}



static void
ptk_icon_view_remove_widget ( GtkCellEditable *editable,
                              PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    GList *lp;

    if ( G_LIKELY ( icon_view->priv->edited_item != NULL ) )
    {
        item = icon_view->priv->edited_item;
        icon_view->priv->edited_item = NULL;
        icon_view->priv->editable = NULL;

        for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
            ( ( PtkIconViewCellInfo * ) lp->data ) ->editing = FALSE;

        if ( GTK_WIDGET_HAS_FOCUS ( editable ) )
            gtk_widget_grab_focus ( GTK_WIDGET ( icon_view ) );

        g_signal_handlers_disconnect_by_func ( editable, ptk_icon_view_remove_widget, icon_view );
        gtk_container_remove ( GTK_CONTAINER ( icon_view ), GTK_WIDGET ( editable ) );

        ptk_icon_view_queue_draw_item ( icon_view, item );
    }
}



static void
ptk_icon_view_start_editing ( PtkIconView *icon_view,
                              PtkIconViewItem *item,
                              PtkIconViewCellInfo *info,
                              GdkEvent *event )
{
    GtkCellRendererMode mode;
    GtkCellEditable *editable;
    GdkRectangle cell_area;
    GtkTreePath *path;
    gboolean visible;
    gchar *path_string;

    /* setup cell data for the given item */
    ptk_icon_view_set_cell_data ( icon_view, item );

    /* check if the cell is visible and editable (given the updated cell data) */
    g_object_get ( info->cell, "visible", &visible, "mode", &mode, NULL );
    if ( G_LIKELY ( visible && mode == GTK_CELL_RENDERER_MODE_EDITABLE ) )
    {
        /* draw keyboard focus while editing */
        PTK_ICON_VIEW_SET_FLAG ( icon_view, PTK_ICON_VIEW_DRAW_KEYFOCUS );

        /* determine the cell area */
        ptk_icon_view_get_cell_area ( icon_view, item, info, &cell_area );

        /* determine the tree path */
        path = gtk_tree_path_new_from_indices ( item->index, -1 );
        path_string = gtk_tree_path_to_string ( path );
        gtk_tree_path_free ( path );

        /* allocate the editable from the cell renderer */
        editable = gtk_cell_renderer_start_editing ( info->cell, event, GTK_WIDGET ( icon_view ), path_string, &cell_area, &cell_area, 0 );

        /* ugly hack, but works */
        if ( g_object_class_find_property ( G_OBJECT_GET_CLASS ( editable ), "has-frame" ) != NULL )
            g_object_set ( editable, "has-frame", TRUE, NULL );

        /* setup the editing widget */
        icon_view->priv->edited_item = item;
        icon_view->priv->editable = editable;
        info->editing = TRUE;

        ptk_icon_view_put ( icon_view, GTK_WIDGET ( editable ), item, info->position );
        gtk_cell_editable_start_editing ( GTK_CELL_EDITABLE ( editable ), ( GdkEvent * ) event );
        gtk_widget_grab_focus ( GTK_WIDGET ( editable ) );
        g_signal_connect ( G_OBJECT ( editable ), "remove-widget", G_CALLBACK ( ptk_icon_view_remove_widget ), icon_view );

        /* cleanup */
        g_free ( path_string );
    }
}



static void
ptk_icon_view_stop_editing ( PtkIconView *icon_view,
                             gboolean cancel_editing )
{
    PtkIconViewItem * item;
    GtkCellRenderer *cell = NULL;
    GList *lp;

    if ( icon_view->priv->edited_item == NULL )
        return ;

    /*
     * This is very evil. We need to do this, because
     * gtk_cell_editable_editing_done may trigger ptk_icon_view_row_changed
     * later on. If ptk_icon_view_row_changed notices
     * icon_view->priv->edited_item != NULL, it'll call
     * ptk_icon_view_stop_editing again. Bad things will happen then.
     *
     * Please read that again if you intend to modify anything here.
     */

    item = icon_view->priv->edited_item;
    icon_view->priv->edited_item = NULL;

    for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
    {
        PtkIconViewCellInfo * info = lp->data;
        if ( info->editing )
        {
            cell = info->cell;
            break;
        }
    }

    if ( G_UNLIKELY ( cell == NULL ) )
        return ;

    gtk_cell_renderer_stop_editing ( cell, cancel_editing );
    if ( G_LIKELY ( !cancel_editing ) )
        gtk_cell_editable_editing_done ( icon_view->priv->editable );

    icon_view->priv->edited_item = item;

    gtk_cell_editable_remove_widget ( icon_view->priv->editable );
}



static gboolean
ptk_icon_view_button_press_event ( GtkWidget *widget,
                                   GdkEventButton *event )
{
    PtkIconViewCellInfo * info = NULL;
    GtkCellRendererMode mode;
    PtkIconViewItem *item;
    PtkIconView *icon_view;
    gboolean dirty = FALSE;
    gint cursor_cell;

    icon_view = PTK_ICON_VIEW ( widget );

    if ( event->window != icon_view->priv->bin_window )
        return FALSE;

    if ( G_UNLIKELY ( !GTK_WIDGET_HAS_FOCUS ( widget ) ) )
        gtk_widget_grab_focus ( widget );

    if ( event->button == 1 && event->type == GDK_BUTTON_PRESS )
    {
        item = ptk_icon_view_get_item_at_coords ( icon_view,
                                                  event->x, event->y,
                                                  TRUE,
                                                  &info );
        if ( item != NULL )
        {
            g_object_get ( info->cell, "mode", &mode, NULL );

            if ( mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE ||
                    mode == GTK_CELL_RENDERER_MODE_EDITABLE )
                cursor_cell = g_list_index ( icon_view->priv->cell_list, info );
            else
                cursor_cell = -1;

            ptk_icon_view_scroll_to_item ( icon_view, item );

            if ( icon_view->priv->selection_mode == GTK_SELECTION_NONE )
            {
                ptk_icon_view_set_cursor_item ( icon_view, item, cursor_cell );
            }
            else if ( icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
                      ( event->state & GDK_SHIFT_MASK ) )
            {
                ptk_icon_view_unselect_all_internal ( icon_view );

                ptk_icon_view_set_cursor_item ( icon_view, item, cursor_cell );
                if ( !icon_view->priv->anchor_item )
                    icon_view->priv->anchor_item = item;
                else
                    ptk_icon_view_select_all_between ( icon_view,
                                                       icon_view->priv->anchor_item,
                                                       item );
                dirty = TRUE;
            }
            else
            {
                if ( ( icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE ||
                        ( ( icon_view->priv->selection_mode == GTK_SELECTION_SINGLE ) && item->selected ) ) &&
                        ( event->state & GDK_CONTROL_MASK ) )
                {
                    item->selected = !item->selected;
                    ptk_icon_view_queue_draw_item ( icon_view, item );
                    dirty = TRUE;
                }
                else
                {
                    if ( !item->selected )
                    {
                        ptk_icon_view_unselect_all_internal ( icon_view );

                        item->selected = TRUE;
                        ptk_icon_view_queue_draw_item ( icon_view, item );
                        dirty = TRUE;
                    }
                }
                ptk_icon_view_set_cursor_item ( icon_view, item, cursor_cell );
                icon_view->priv->anchor_item = item;
            }

            /* Save press to possibly begin a drag */
            if ( icon_view->priv->pressed_button < 0 )
            {
                icon_view->priv->pressed_button = event->button;
                icon_view->priv->press_start_x = event->x;
                icon_view->priv->press_start_y = event->y;
            }

            if ( !icon_view->priv->last_single_clicked )
                icon_view->priv->last_single_clicked = item;

            /* cancel the current editing, if it exists */
            ptk_icon_view_stop_editing ( icon_view, TRUE );

            if ( mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE )
                ptk_icon_view_item_activate_cell ( icon_view, item, info,
                                                   ( GdkEvent * ) event );
            else if ( mode == GTK_CELL_RENDERER_MODE_EDITABLE )
                ptk_icon_view_start_editing ( icon_view, item, info,
                                              ( GdkEvent * ) event );
        }
        else
        {
            /* cancel the current editing, if it exists */
            ptk_icon_view_stop_editing ( icon_view, TRUE );

            if ( icon_view->priv->selection_mode != GTK_SELECTION_BROWSE &&
                    !( event->state & GDK_CONTROL_MASK ) )
            {
                dirty = ptk_icon_view_unselect_all_internal ( icon_view );
            }

            if ( icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE )
                ptk_icon_view_start_rubberbanding ( icon_view, event->x, event->y );
        }
    }
    else if ( event->button == 1 && event->type == GDK_2BUTTON_PRESS )
    {
        icon_view->priv->pressed_button = -1; /* Don't begin drag */
        item = ptk_icon_view_get_item_at_coords ( icon_view,
                                                  event->x, event->y,
                                                  TRUE,
                                                  NULL );

        if ( item && item == icon_view->priv->last_single_clicked )
        {
            GtkTreePath * path;

            path = gtk_tree_path_new_from_indices ( item->index, -1 );
            ptk_icon_view_item_activated ( icon_view, path );
            gtk_tree_path_free ( path );
        }

        icon_view->priv->last_single_clicked = NULL;
    }

    /* grab focus and stop drawing the keyboard focus indicator on single clicks */
    if ( G_LIKELY ( event->type != GDK_2BUTTON_PRESS && event->type != GDK_3BUTTON_PRESS ) )
    {
        if ( !GTK_WIDGET_HAS_FOCUS ( icon_view ) )
            gtk_widget_grab_focus ( GTK_WIDGET ( icon_view ) );
        PTK_ICON_VIEW_UNSET_FLAG ( icon_view, PTK_ICON_VIEW_DRAW_KEYFOCUS );
    }

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );

    return event->button == 1;
}



static gboolean
ptk_icon_view_button_release_event ( GtkWidget *widget,
                                     GdkEventButton *event )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( widget );

    if ( icon_view->priv->pressed_button == event->button )
        icon_view->priv->pressed_button = -1;

    ptk_icon_view_stop_rubberbanding ( icon_view );

    remove_scroll_timeout ( icon_view );

    return TRUE;
}



static void
ptk_icon_view_update_rubberband ( gpointer data )
{
    PtkIconView * icon_view;
    gint x, y;
    GdkRectangle old_area;
    GdkRectangle new_area;
    GdkRectangle common;
    GdkRegion *invalid_region;

    icon_view = PTK_ICON_VIEW ( data );

    gdk_window_get_pointer ( icon_view->priv->bin_window, &x, &y, NULL );

    x = MAX ( x, 0 );
    y = MAX ( y, 0 );

    old_area.x = MIN ( icon_view->priv->rubberband_x1,
                       icon_view->priv->rubberband_x2 );
    old_area.y = MIN ( icon_view->priv->rubberband_y1,
                       icon_view->priv->rubberband_y2 );
    old_area.width = ABS ( icon_view->priv->rubberband_x2 -
                           icon_view->priv->rubberband_x1 ) + 1;
    old_area.height = ABS ( icon_view->priv->rubberband_y2 -
                            icon_view->priv->rubberband_y1 ) + 1;

    new_area.x = MIN ( icon_view->priv->rubberband_x1, x );
    new_area.y = MIN ( icon_view->priv->rubberband_y1, y );
    new_area.width = ABS ( x - icon_view->priv->rubberband_x1 ) + 1;
    new_area.height = ABS ( y - icon_view->priv->rubberband_y1 ) + 1;

    invalid_region = gdk_region_rectangle ( &old_area );
    gdk_region_union_with_rect ( invalid_region, &new_area );

    gdk_rectangle_intersect ( &old_area, &new_area, &common );
    if ( common.width > 2 && common.height > 2 )
    {
        GdkRegion * common_region;

        /* make sure the border is invalidated */
        common.x += 1;
        common.y += 1;
        common.width -= 2;
        common.height -= 2;

        common_region = gdk_region_rectangle ( &common );

        gdk_region_subtract ( invalid_region, common_region );
        gdk_region_destroy ( common_region );
    }

    gdk_window_invalidate_region ( icon_view->priv->bin_window, invalid_region, TRUE );

    gdk_region_destroy ( invalid_region );

    icon_view->priv->rubberband_x2 = x;
    icon_view->priv->rubberband_y2 = y;

    ptk_icon_view_update_rubberband_selection ( icon_view );
}



static void
ptk_icon_view_start_rubberbanding ( PtkIconView *icon_view,
                                    gint x,
                                    gint y )
{
    const GdkColor * background_color;
    GdkColor *color;
    guchar alpha;
    gpointer drag_data;
    GList *items;

    /* be sure to disable any previously active rubberband */
    ptk_icon_view_stop_rubberbanding ( icon_view );

    for ( items = icon_view->priv->items; items; items = items->next )
    {
        PtkIconViewItem * item = items->data;
        item->selected_before_rubberbanding = item->selected;
    }

    icon_view->priv->rubberband_x1 = x;
    icon_view->priv->rubberband_y1 = y;
    icon_view->priv->rubberband_x2 = x;
    icon_view->priv->rubberband_y2 = y;

    icon_view->priv->doing_rubberband = TRUE;

    /* determine the border color */
    gtk_widget_style_get ( GTK_WIDGET ( icon_view ), "selection-box-color", &color, NULL );
    if ( G_LIKELY ( color == NULL ) )
        color = gdk_color_copy ( &GTK_WIDGET ( icon_view ) ->style->base[ GTK_STATE_SELECTED ] );

    /* allocate the border GC */
    icon_view->priv->rubberband_border_gc = gdk_gc_new ( icon_view->priv->bin_window );
    gdk_gc_set_rgb_fg_color ( icon_view->priv->rubberband_border_gc, color );
    gdk_color_free ( color );

    /* determine the fill color and alpha setting */
    gtk_widget_style_get ( GTK_WIDGET ( icon_view ), "selection-box-color", &color, "selection-box-alpha", &alpha, NULL );
    if ( G_LIKELY ( color == NULL ) )
        color = gdk_color_copy ( &GTK_WIDGET ( icon_view ) ->style->base[ GTK_STATE_SELECTED ] );

    /* calculate the fill color (based on the fill color, the alpha setting and the background color) */
    background_color = &GTK_WIDGET ( icon_view ) ->style->base[ GTK_STATE_NORMAL ];
    color->red = ( ( color->red * ( alpha / 255.0 ) ) + ( background_color->red * ( 255.0 - alpha / 255.0 ) ) );
    color->green = ( ( color->green * ( alpha / 255.0 ) ) + ( background_color->green * ( 255.0 - alpha / 255.0 ) ) );
    color->blue = ( ( color->blue * ( alpha / 255.0 ) ) + ( background_color->blue * ( 255.0 - alpha / 255.0 ) ) );

    /* allocate the GC to draw the rubberband background */
    icon_view->priv->rubberband_fill_gc = gdk_gc_new ( icon_view->priv->bin_window );
    gdk_gc_set_rgb_fg_color ( icon_view->priv->rubberband_fill_gc, color );
    gdk_color_free ( color );

    gtk_grab_add ( GTK_WIDGET ( icon_view ) );

    /* be sure to disable Gtk+ DnD callbacks, because else rubberbanding will be interrupted */
    drag_data = g_object_get_data ( G_OBJECT ( icon_view ), I_( "gtk-site-data" ) );
    if ( G_LIKELY ( drag_data != NULL ) )
    {
        g_signal_handlers_block_matched ( G_OBJECT ( icon_view ),
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL,
                                          drag_data );
    }
}



static void
ptk_icon_view_stop_rubberbanding ( PtkIconView *icon_view )
{
    gpointer drag_data;

    if ( G_LIKELY ( icon_view->priv->doing_rubberband ) )
    {
        icon_view->priv->doing_rubberband = FALSE;
        gtk_grab_remove ( GTK_WIDGET ( icon_view ) );
        gtk_widget_queue_draw ( GTK_WIDGET ( icon_view ) );

        /* drop the GCs for drawing the rubberband */
        g_object_unref ( G_OBJECT ( icon_view->priv->rubberband_border_gc ) );
        g_object_unref ( G_OBJECT ( icon_view->priv->rubberband_fill_gc ) );
        icon_view->priv->rubberband_border_gc = NULL;
        icon_view->priv->rubberband_fill_gc = NULL;

        /* re-enable Gtk+ DnD callbacks again */
        drag_data = g_object_get_data ( G_OBJECT ( icon_view ), I_( "gtk-site-data" ) );
        if ( G_LIKELY ( drag_data != NULL ) )
        {
            g_signal_handlers_unblock_matched ( G_OBJECT ( icon_view ),
                                                G_SIGNAL_MATCH_DATA,
                                                0, 0, NULL, NULL,
                                                drag_data );
        }
    }
}



static void
ptk_icon_view_update_rubberband_selection ( PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    gboolean selected;
    gboolean changed = FALSE;
    ;
    gboolean is_in;
    GList *lp;
    gint x, y;
    gint width;
    gint height;

    /* determine the new rubberband area */
    x = MIN ( icon_view->priv->rubberband_x1, icon_view->priv->rubberband_x2 );
    y = MIN ( icon_view->priv->rubberband_y1, icon_view->priv->rubberband_y2 );
    width = ABS ( icon_view->priv->rubberband_x1 - icon_view->priv->rubberband_x2 );
    height = ABS ( icon_view->priv->rubberband_y1 - icon_view->priv->rubberband_y2 );

    /* check all items */
    for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
    {
        item = PTK_ICON_VIEW_ITEM ( lp->data );

        is_in = ptk_icon_view_item_hit_test ( icon_view, item, x, y, width, height );

        selected = is_in ^ item->selected_before_rubberbanding;

        if ( G_UNLIKELY ( item->selected != selected ) )
        {
            changed = TRUE;
            item->selected = selected;
            ptk_icon_view_queue_draw_item ( icon_view, item );
        }
    }

    if ( G_LIKELY ( changed ) )
        g_signal_emit ( G_OBJECT ( icon_view ), icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static gboolean
ptk_icon_view_item_hit_test ( PtkIconView *icon_view,
                              PtkIconViewItem *item,
                              gint x,
                              gint y,
                              gint width,
                              gint height )
{
    GList * l;
    GdkRectangle box;

    for ( l = icon_view->priv->cell_list; l; l = l->next )
    {
        PtkIconViewCellInfo * info = ( PtkIconViewCellInfo * ) l->data;

        if ( !info->cell->visible )
            continue;

        /* FIXME: Don't know exactly why this check is needed.
                  Sometimes item->box may be NULL, and hence causes seg faults.
                  This seems to be a bug of the original icon view from libexo.
        */
        if( G_UNLIKELY( ! item->box ) )
            return FALSE;

        box = item->box[ info->position ];

        if ( MIN ( x + width, box.x + box.width ) - MAX ( x, box.x ) > 0 &&
                MIN ( y + height, box.y + box.height ) - MAX ( y, box.y ) > 0 )
            return TRUE;
    }

    return FALSE;
}



static gboolean
ptk_icon_view_unselect_all_internal ( PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    gboolean dirty = FALSE;
    GList *lp;

    if ( G_LIKELY ( icon_view->priv->selection_mode != GTK_SELECTION_NONE ) )
    {
        for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
        {
            item = PTK_ICON_VIEW_ITEM ( lp->data );
            if ( item->selected )
            {
                dirty = TRUE;
                item->selected = FALSE;
                ptk_icon_view_queue_draw_item ( icon_view, item );
            }
        }
    }

    return dirty;
}



static void
ptk_icon_view_set_adjustments ( PtkIconView *icon_view,
                                GtkAdjustment *hadj,
                                GtkAdjustment *vadj )
{
    gboolean need_adjust = FALSE;

    if ( hadj )
        g_return_if_fail ( GTK_IS_ADJUSTMENT ( hadj ) );
    else
        hadj = GTK_ADJUSTMENT ( gtk_adjustment_new ( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );
    if ( vadj )
        g_return_if_fail ( GTK_IS_ADJUSTMENT ( vadj ) );
    else
        vadj = GTK_ADJUSTMENT ( gtk_adjustment_new ( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );

    if ( icon_view->priv->hadjustment && ( icon_view->priv->hadjustment != hadj ) )
    {
        g_signal_handlers_disconnect_matched ( icon_view->priv->hadjustment, G_SIGNAL_MATCH_DATA,
                                               0, 0, NULL, NULL, icon_view );
        g_object_unref ( icon_view->priv->hadjustment );
    }

    if ( icon_view->priv->vadjustment && ( icon_view->priv->vadjustment != vadj ) )
    {
        g_signal_handlers_disconnect_matched ( icon_view->priv->vadjustment, G_SIGNAL_MATCH_DATA,
                                               0, 0, NULL, NULL, icon_view );
        g_object_unref ( icon_view->priv->vadjustment );
    }

    if ( icon_view->priv->hadjustment != hadj )
    {
        icon_view->priv->hadjustment = hadj;
        g_object_ref ( icon_view->priv->hadjustment );
        gtk_object_sink ( GTK_OBJECT ( icon_view->priv->hadjustment ) );

        g_signal_connect ( icon_view->priv->hadjustment, "value-changed",
                           G_CALLBACK ( ptk_icon_view_adjustment_changed ),
                           icon_view );
        need_adjust = TRUE;
    }

    if ( icon_view->priv->vadjustment != vadj )
    {
        icon_view->priv->vadjustment = vadj;
        g_object_ref ( icon_view->priv->vadjustment );
        gtk_object_sink ( GTK_OBJECT ( icon_view->priv->vadjustment ) );

        g_signal_connect ( icon_view->priv->vadjustment, "value-changed",
                           G_CALLBACK ( ptk_icon_view_adjustment_changed ),
                           icon_view );
        need_adjust = TRUE;
    }

    if ( need_adjust )
        ptk_icon_view_adjustment_changed ( NULL, icon_view );
}



static void
ptk_icon_view_real_select_all ( PtkIconView *icon_view )
{
    ptk_icon_view_select_all ( icon_view );
}



static void
ptk_icon_view_real_unselect_all ( PtkIconView *icon_view )
{
    ptk_icon_view_unselect_all ( icon_view );
}



static void
ptk_icon_view_real_select_cursor_item ( PtkIconView *icon_view )
{
    ptk_icon_view_unselect_all ( icon_view );

    if ( icon_view->priv->cursor_item != NULL )
        ptk_icon_view_select_item ( icon_view, icon_view->priv->cursor_item );
}



static gboolean
ptk_icon_view_real_activate_cursor_item ( PtkIconView *icon_view )
{
    GtkTreePath * path;
    GtkCellRendererMode mode;
    PtkIconViewCellInfo *info = NULL;

    if ( !icon_view->priv->cursor_item )
        return FALSE;

    info = g_list_nth_data ( icon_view->priv->cell_list,
                             icon_view->priv->cursor_cell );

    if ( info )
    {
        g_object_get ( info->cell, "mode", &mode, NULL );

        if ( mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE )
        {
            ptk_icon_view_item_activate_cell ( icon_view,
                                               icon_view->priv->cursor_item,
                                               info, NULL );
            return TRUE;
        }
        else if ( mode == GTK_CELL_RENDERER_MODE_EDITABLE )
        {
            ptk_icon_view_start_editing ( icon_view,
                                          icon_view->priv->cursor_item,
                                          info, NULL );
            return TRUE;
        }
    }

    path = gtk_tree_path_new_from_indices ( icon_view->priv->cursor_item->index, -1 );
    ptk_icon_view_item_activated ( icon_view, path );
    gtk_tree_path_free ( path );

    return TRUE;
}



static void
ptk_icon_view_real_toggle_cursor_item ( PtkIconView *icon_view )
{
    if ( G_LIKELY ( icon_view->priv->cursor_item != NULL ) )
    {
        switch ( icon_view->priv->selection_mode )
        {
        case GTK_SELECTION_NONE:
            break;

        case GTK_SELECTION_BROWSE:
            ptk_icon_view_select_item ( icon_view, icon_view->priv->cursor_item );
            break;

        case GTK_SELECTION_SINGLE:
            if ( icon_view->priv->cursor_item->selected )
                ptk_icon_view_unselect_item ( icon_view, icon_view->priv->cursor_item );
            else
                ptk_icon_view_select_item ( icon_view, icon_view->priv->cursor_item );
            break;

        case GTK_SELECTION_MULTIPLE:
            icon_view->priv->cursor_item->selected = !icon_view->priv->cursor_item->selected;
            g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
            ptk_icon_view_queue_draw_item ( icon_view, icon_view->priv->cursor_item );
            break;

        default:
            g_assert_not_reached ();
        }
    }
}



static void
ptk_icon_view_adjustment_changed ( GtkAdjustment *adjustment,
                                   PtkIconView *icon_view )
{
    if ( GTK_WIDGET_REALIZED ( icon_view ) )
    {
        gdk_window_move ( icon_view->priv->bin_window, -icon_view->priv->hadjustment->value, -icon_view->priv->vadjustment->value );

        if ( G_UNLIKELY ( icon_view->priv->doing_rubberband ) )
            ptk_icon_view_update_rubberband ( GTK_WIDGET ( icon_view ) );

        gdk_window_process_updates ( icon_view->priv->bin_window, TRUE );
    }
}



static GList *
ptk_icon_view_layout_single_row ( PtkIconView *icon_view,
                                  GList *first_item,
                                  gint item_width,
                                  gint row,
                                  gint *y,
                                  gint *maximum_width )
{
    PtkIconViewItem * item;
    gint focus_width;
    gint x, current_width;
    GList *items, *last_item;
    gint col;
    gint colspan;
    gint *max_height;
    gint i;
    gboolean rtl;

    rtl = gtk_widget_get_direction ( GTK_WIDGET ( icon_view ) ) == GTK_TEXT_DIR_RTL;

    /* alloca() is safe on gcc/i386 */
    max_height = g_newa ( gint, icon_view->priv->n_cells );
    for ( i = icon_view->priv->n_cells; --i >= 0; )
        max_height[ i ] = 0;

    x = 0;
    col = 0;
    items = first_item;
    current_width = 0;

    gtk_widget_style_get ( GTK_WIDGET ( icon_view ),
                           "focus-line-width", &focus_width,
                           NULL );

    x += icon_view->priv->margin + focus_width;
    current_width += 2 * ( icon_view->priv->margin + focus_width );

    for ( items = first_item; items != NULL; items = items->next )
    {
        item = PTK_ICON_VIEW_ITEM ( items->data );

        ptk_icon_view_calculate_item_size ( icon_view, item );
        colspan = 1 + ( item->area.width - 1 ) / ( item_width + icon_view->priv->column_spacing );

        item->area.width = colspan * item_width + ( colspan - 1 ) * icon_view->priv->column_spacing;

        current_width += item->area.width;

        if ( G_LIKELY ( items != first_item ) )
        {
            if ( ( icon_view->priv->columns <= 0 && current_width > GTK_WIDGET ( icon_view ) ->allocation.width ) ||
                    ( icon_view->priv->columns > 0 && col >= icon_view->priv->columns ) )
                break;
        }

        current_width += icon_view->priv->column_spacing + 2 * focus_width;

        item->area.y = *y + focus_width;
        item->area.x = rtl ? GTK_WIDGET ( icon_view ) ->allocation.width - item->area.width - x : x;

        x = current_width - ( icon_view->priv->margin + focus_width );

        for ( i = 0; i < icon_view->priv->n_cells; i++ )
            max_height[ i ] = MAX ( max_height[ i ], item->box[ i ].height );

        if ( current_width > *maximum_width )
            * maximum_width = current_width;

        item->row = row;
        item->col = col;

        col += colspan;
    }

    last_item = items;

    /* Now go through the row again and align the icons */
    for ( items = first_item; items != last_item; items = items->next )
    {
        item = PTK_ICON_VIEW_ITEM ( items->data );

        ptk_icon_view_calculate_item_size2 ( icon_view, item, max_height );

        /* We may want to readjust the new y coordinate. */
        if ( item->area.y + item->area.height + focus_width + icon_view->priv->row_spacing > *y )
            * y = item->area.y + item->area.height + focus_width + icon_view->priv->row_spacing;

        if ( G_UNLIKELY ( rtl ) )
            item->col = col - 1 - item->col;
    }


    return last_item;
}



static void
ptk_icon_view_set_adjustment_upper ( GtkAdjustment *adj,
                                     gdouble upper )
{
    if ( upper != adj->upper )
    {
        gdouble min = MAX ( 0.0, upper - adj->page_size );
        gboolean value_changed = FALSE;

        adj->upper = upper;

        if ( adj->value > min )
        {
            adj->value = min;
            value_changed = TRUE;
        }

        gtk_adjustment_changed ( adj );

        if ( value_changed )
            gtk_adjustment_value_changed ( adj );
    }
}



static void
ptk_icon_view_layout ( PtkIconView *icon_view )
{
    gint y = 0, maximum_width = 0;
    GList *icons;
    GtkWidget *widget = GTK_WIDGET ( icon_view );
    gint row;
    gint item_width;

    if ( icon_view->priv->model == NULL )
        return ;

    /* calculate item sizes on-demand */
    item_width = icon_view->priv->item_width;
    if ( item_width < 0 )
    {
        for ( icons = icon_view->priv->items; icons; icons = icons->next )
        {
            PtkIconViewItem * item = icons->data;
            ptk_icon_view_calculate_item_size ( icon_view, item );
            item_width = MAX ( item_width, item->area.width );
        }
    }

    icons = icon_view->priv->items;
    y += icon_view->priv->margin;
    row = 0;

    do
    {
        icons = ptk_icon_view_layout_single_row ( icon_view, icons,
                                                  item_width, row,
                                                  &y, &maximum_width );
        row++;
    }
    while ( icons != NULL );

    if ( maximum_width != icon_view->priv->width )
    {
        icon_view->priv->width = maximum_width;
    }
    y += icon_view->priv->margin;

    if ( y != icon_view->priv->height )
    {
        icon_view->priv->height = y;
    }

    ptk_icon_view_set_adjustment_upper ( icon_view->priv->hadjustment, icon_view->priv->width );
    ptk_icon_view_set_adjustment_upper ( icon_view->priv->vadjustment, icon_view->priv->height );

    if ( GTK_WIDGET_REALIZED ( icon_view ) )
    {
        gdk_window_resize ( icon_view->priv->bin_window,
                            MAX ( icon_view->priv->width, widget->allocation.width ),
                            MAX ( icon_view->priv->height, widget->allocation.height ) );
    }

    if ( icon_view->priv->layout_idle_id >= 0 )
        g_source_remove ( icon_view->priv->layout_idle_id );

    gtk_widget_queue_draw ( GTK_WIDGET ( icon_view ) );
}



static void
ptk_icon_view_get_cell_area ( PtkIconView *icon_view,
                              PtkIconViewItem *item,
                              PtkIconViewCellInfo *info,
                              GdkRectangle *cell_area )
{
    if ( icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL )
    {
        cell_area->x = item->box[ info->position ].x - item->before[ info->position ];
        cell_area->y = item->area.y;
        cell_area->width = item->box[ info->position ].width + item->before[ info->position ] + item->after[ info->position ];
        cell_area->height = item->area.height;
    }
    else
    {
        cell_area->x = item->area.x;
        cell_area->y = item->box[ info->position ].y - item->before[ info->position ];
        cell_area->width = item->area.width;
        cell_area->height = item->box[ info->position ].height + item->before[ info->position ] + item->after[ info->position ];
    }
}



static void
ptk_icon_view_calculate_item_size ( PtkIconView *icon_view,
                                    PtkIconViewItem *item )
{
    PtkIconViewCellInfo * info;
    GList *lp;
    gchar *buffer;

    if ( G_LIKELY ( item->area.width != -1 && item->area.height != -1 ) )
        return ;

    if ( G_UNLIKELY ( item->n_cells != icon_view->priv->n_cells ) )
    {
        /* apply the new cell size */
        item->n_cells = icon_view->priv->n_cells;

        /* release the memory chunk (if any) */
        g_free ( item->box );

        /* allocate a single memory chunk for box, after and before */
        buffer = g_malloc0 ( item->n_cells * ( sizeof ( GdkRectangle ) + 2 * sizeof ( gint ) ) );

        /* assign the memory */
        item->box = ( GdkRectangle * ) buffer;
        item->after = ( gint * ) ( buffer + item->n_cells * sizeof ( GdkRectangle ) );
        item->before = item->after + item->n_cells;
    }

    ptk_icon_view_set_cell_data ( icon_view, item );

    item->area.width = 0;
    item->area.height = 0;
    for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
    {
        info = PTK_ICON_VIEW_CELL_INFO ( lp->data );
        if ( G_UNLIKELY ( !info->cell->visible ) )
            continue;

        gtk_cell_renderer_get_size ( info->cell, GTK_WIDGET ( icon_view ),
                                     NULL, NULL, NULL,
                                     &item->box[ info->position ].width,
                                     &item->box[ info->position ].height );

        if ( icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL )
        {
            item->area.width += item->box[ info->position ].width + ( info->position > 0 ? icon_view->priv->spacing : 0 );
            item->area.height = MAX ( item->area.height, item->box[ info->position ].height );
        }
        else
        {
            item->area.width = MAX ( item->area.width, item->box[ info->position ].width );
            item->area.height += item->box[ info->position ].height + ( info->position > 0 ? icon_view->priv->spacing : 0 );
        }
    }
}



static void
ptk_icon_view_calculate_item_size2 ( PtkIconView *icon_view,
                                     PtkIconViewItem *item,
                                     gint *max_height )
{
    PtkIconViewCellInfo * info;
    GdkRectangle cell_area;
    GdkRectangle *box;
    gint spacing;
    GList *lp;
    gint i, k;
    gboolean rtl;

    rtl = ( gtk_widget_get_direction ( GTK_WIDGET ( icon_view ) ) == GTK_TEXT_DIR_RTL );

    spacing = icon_view->priv->spacing;

    item->area.height = 0;
    for ( i = 0; i < icon_view->priv->n_cells; i++ )
    {
        if ( icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL )
            item->area.height = MAX ( item->area.height, max_height[ i ] );
        else
            item->area.height += max_height[ i ] + ( i > 0 ? spacing : 0 );
    }

    cell_area.x = item->area.x;
    cell_area.y = item->area.y;

    for ( k = 0; k < 2; ++k )
    {
        for ( lp = icon_view->priv->cell_list, i = 0; lp != NULL; lp = lp->next, ++i )
        {
            info = PTK_ICON_VIEW_CELL_INFO ( lp->data );
            if ( G_UNLIKELY ( !info->cell->visible || info->pack == ( k ? GTK_PACK_START : GTK_PACK_END ) ) )
                continue;

            if ( icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL )
            {
                cell_area.width = item->box[ info->position ].width;
                cell_area.height = item->area.height;
            }
            else
            {
                cell_area.width = item->area.width;
                cell_area.height = max_height[ i ];
            }

            box = item->box + info->position;
            box->x = cell_area.x + ( rtl ? ( 1.0 - info->cell->xalign ) : info->cell->xalign ) * ( cell_area.width - box->width - ( 2 * info->cell->xpad ) );
            box->x = MAX ( box->x, 0 );
            box->y = cell_area.y + info->cell->yalign * ( cell_area.height - box->height - ( 2 * info->cell->ypad ) );
            box->y = MAX ( box->y, 0 );

            if ( icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL )
            {
                item->before[ info->position ] = item->box[ info->position ].x - cell_area.x;
                item->after[ info->position ] = cell_area.width - item->box[ info->position ].width - item->before[ info->position ];
                cell_area.x += cell_area.width + spacing;
            }
            else
            {
                if ( item->box[ info->position ].width > item->area.width )
                {
                    item->area.width = item->box[ info->position ].width;
                    cell_area.width = item->area.width;
                }
                item->before[ info->position ] = item->box[ info->position ].y - cell_area.y;
                item->after[ info->position ] = cell_area.height - item->box[ info->position ].height - item->before[ info->position ];
                cell_area.y += cell_area.height + spacing;
            }
        }
    }

    if ( G_UNLIKELY ( rtl && icon_view->priv->orientation == GTK_ORIENTATION_HORIZONTAL ) )
    {
        for ( i = 0; i < icon_view->priv->n_cells; i++ )
            item->box[ i ].x = item->area.x + item->area.width - ( item->box[ i ].x + item->box[ i ].width - item->area.x );
    }
}


static void
ptk_icon_view_invalidate_sizes ( PtkIconView *icon_view )
{
    g_list_foreach ( icon_view->priv->items, ( GFunc ) ptk_icon_view_item_invalidate_size, NULL );
    ptk_icon_view_queue_layout ( icon_view );
}


static void
ptk_icon_view_item_invalidate_size ( PtkIconViewItem *item )
{
    item->area.width = -1;
    item->area.height = -1;
}



static void
ptk_icon_view_paint_item ( PtkIconView *icon_view,
                           PtkIconViewItem *item,
                           GdkRectangle *area,
                           GdkDrawable *drawable,
                           gint x,
                           gint y,
                           gboolean draw_focus )
{
    GtkCellRendererState flags;
    PtkIconViewCellInfo *info;
    GtkStateType state;
    GdkRectangle cell_area, box;
    gboolean rtl;
    GList *lp;
    gint i;
    gint focus_pad, padding;

    if ( G_UNLIKELY ( icon_view->priv->model == NULL ) )
        return ;

    gtk_widget_style_get ( GTK_WIDGET ( icon_view ),
                           "focus-line-width", &padding,
                           "focus-padding", &focus_pad,
                           NULL );

    ptk_icon_view_set_cell_data ( icon_view, item );

    rtl = gtk_widget_get_direction ( GTK_WIDGET ( icon_view ) ) == GTK_TEXT_DIR_RTL;

    if ( item->selected )
    {
        flags = GTK_CELL_RENDERER_SELECTED;
        state = GTK_WIDGET_HAS_FOCUS ( icon_view ) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;
    }
    else
    {
        flags = 0;
        state = GTK_STATE_NORMAL;
    }

    if ( G_UNLIKELY ( icon_view->priv->prelit_item == item ) )
        flags |= GTK_CELL_RENDERER_PRELIT;
    if ( G_UNLIKELY ( PTK_ICON_VIEW_FLAG_SET ( icon_view, PTK_ICON_VIEW_DRAW_KEYFOCUS ) && icon_view->priv->cursor_item == item ) )
        flags |= GTK_CELL_RENDERER_FOCUSED;

    for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
    {
        info = PTK_ICON_VIEW_CELL_INFO ( lp->data );

        if ( G_UNLIKELY ( !info->cell->visible ) )
            continue;

        ptk_icon_view_get_cell_area ( icon_view, item, info, &cell_area );

        cell_area.x = x - item->area.x + cell_area.x;
        cell_area.y = y - item->area.y + cell_area.y;

        if ( PTK_IS_TEXT_RENDERER ( info->cell ) )
        {
            box = item->box[ info->position ];
#if 0

            gdk_draw_rectangle ( drawable,
                                 GTK_WIDGET ( icon_view ) ->style->base_gc[ state ],
                                 TRUE,
                                 x - item->area.x + box.x /* + padding + focus_pad*/,
                                 y - item->area.y + box.y /* + padding - focus_pad*/,
                                 box.width, box.height );
#endif
            /*
            * FIXME: There are some problems in size calculating of cell renderers.
            * Calling gtk_cell_renderer_get_size here is not needed. This should be done
            * somewhere in the program, but it didn't be done correctly.
            * I don't know what's the problem, but this should be fixed before release.
            * Calculating size here will cause serious performance hit.
            */
            gtk_cell_renderer_get_size( info->cell, GTK_WIDGET( icon_view ),
                                        &cell_area,
                                        &box.x, &box.y,
                                        &box.width, &box.height );
            box.x += cell_area.x;
            box.y += cell_area.y;

            gdk_draw_rectangle ( drawable,
                                 GTK_WIDGET ( icon_view ) ->style->base_gc[ state ],
                                 TRUE,
                                 box.x,
                                 box.y,
                                 box.width, box.height );
        }

        gtk_cell_renderer_render ( info->cell,
                                   drawable,
                                   GTK_WIDGET ( icon_view ),
                                   &cell_area, &cell_area, area, flags );
    }

    if ( draw_focus &&
            GTK_WIDGET_HAS_FOCUS ( icon_view ) &&
            item == icon_view->priv->cursor_item )
    {
        for ( lp = icon_view->priv->cell_list, i = 0; lp; lp = lp->next, i++ )
        {
            PtkIconViewCellInfo * info = ( PtkIconViewCellInfo * ) lp->data;

            if ( !info->cell->visible )
                continue;

            if ( icon_view->priv->cursor_cell < 0 &&
                    ( info->cell->mode != GTK_CELL_RENDERER_MODE_INERT ||
                      PTK_IS_TEXT_RENDERER ( info->cell ) ) )
                icon_view->priv->cursor_cell = i;

            if ( i == icon_view->priv->cursor_cell )
            {
                gtk_paint_focus ( GTK_WIDGET ( icon_view ) ->style,
                                  drawable,
                                  GTK_STATE_NORMAL,
                                  area,
                                  GTK_WIDGET ( icon_view ),
                                  "icon_view",
                                  /*
                                  x - item->area.x + box.x + padding,
                                  y - item->area.y + box.y - focus_pad,
                                  */
                                  box.x - padding,
                                  box.y - padding,
                                  box.width + 2 * padding,
                                  box.height + 2 * padding );
                break;
            }
        }
    }
}



static void
ptk_icon_view_queue_draw_path ( PtkIconView *icon_view,
                                GtkTreePath *path )
{
    PtkIconViewItem * item;

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices ( path ) [ 0 ] );
    if ( G_LIKELY ( item != NULL ) )
        ptk_icon_view_queue_draw_item ( icon_view, item );
}



static void
ptk_icon_view_queue_draw_item ( PtkIconView *icon_view,
                                PtkIconViewItem *item )
{
    GdkRectangle rect;
    gint focus_width;

    gtk_widget_style_get ( GTK_WIDGET ( icon_view ),
                           "focus-line-width", &focus_width,
                           NULL );

    rect.x = item->area.x - focus_width;
    rect.y = item->area.y - focus_width;
    rect.width = item->area.width + 2 * focus_width;
    rect.height = item->area.height + 2 * focus_width;

    if ( icon_view->priv->bin_window )
        gdk_window_invalidate_rect ( icon_view->priv->bin_window, &rect, TRUE );
}



static gboolean
layout_callback ( gpointer user_data )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( user_data );

    GDK_THREADS_ENTER ();
    ptk_icon_view_layout ( icon_view );
    GDK_THREADS_LEAVE();

    return FALSE;
}



static void
layout_destroy ( gpointer user_data )
{
    PTK_ICON_VIEW ( user_data ) ->priv->layout_idle_id = -1;
}



static void
ptk_icon_view_queue_layout ( PtkIconView *icon_view )
{
    if ( G_UNLIKELY ( icon_view->priv->layout_idle_id < 0 ) )
        icon_view->priv->layout_idle_id = g_idle_add_full ( G_PRIORITY_DEFAULT_IDLE, layout_callback, icon_view, layout_destroy );
}



static void
ptk_icon_view_set_cursor_item ( PtkIconView *icon_view,
                                PtkIconViewItem *item,
                                gint cursor_cell )
{
    AtkObject * obj;
    AtkObject *item_obj;

    if ( icon_view->priv->cursor_item == item &&
            ( cursor_cell < 0 || cursor_cell == icon_view->priv->cursor_cell ) )
        return ;

    if ( icon_view->priv->cursor_item != NULL )
        ptk_icon_view_queue_draw_item ( icon_view, icon_view->priv->cursor_item );

    icon_view->priv->cursor_item = item;
    if ( cursor_cell >= 0 )
        icon_view->priv->cursor_cell = cursor_cell;

    ptk_icon_view_queue_draw_item ( icon_view, item );

    /* Notify that accessible focus object has changed */
    obj = gtk_widget_get_accessible ( GTK_WIDGET ( icon_view ) );
    item_obj = atk_object_ref_accessible_child ( obj, item->index );

    if ( item_obj != NULL )
    {
        atk_focus_tracker_notify ( item_obj );
        g_object_unref ( item_obj );
    }
}



static PtkIconViewItem*
ptk_icon_view_get_item_at_coords ( const PtkIconView *icon_view,
                                   gint x,
                                   gint y,
                                   gboolean only_in_cell,
                                   PtkIconViewCellInfo **cell_at_pos )
{
    GList * items, *l;
    GdkRectangle box;

    for ( items = icon_view->priv->items; items; items = items->next )
    {
        PtkIconViewItem * item = items->data;
        if ( x >= item->area.x - icon_view->priv->row_spacing / 2 && x <= item->area.x + item->area.width + icon_view->priv->row_spacing / 2 &&
                y >= item->area.y - icon_view->priv->column_spacing / 2 && y <= item->area.y + item->area.height + icon_view->priv->column_spacing / 2 )
        {
            if ( only_in_cell || cell_at_pos )
            {
                ptk_icon_view_set_cell_data ( icon_view, item );

                for ( l = icon_view->priv->cell_list; l; l = l->next )
                {
                    PtkIconViewCellInfo * info = ( PtkIconViewCellInfo * ) l->data;

                    if ( !info->cell->visible )
                        continue;

                    box = item->box[ info->position ];
                    if ( ( x >= box.x && x <= box.x + box.width &&
                            y >= box.y && y <= box.y + box.height ) ||
                            ( x >= box.x &&
                              x <= box.x + box.width &&
                              y >= box.y &&
                              y <= box.y + box.height ) )
                    {
                        if ( cell_at_pos )
                            * cell_at_pos = info;

                        return item;
                    }
                }

                if ( only_in_cell )
                    return NULL;

                if ( cell_at_pos )
                    * cell_at_pos = NULL;
            }

            return item;
        }
    }

    return NULL;
}



static void
ptk_icon_view_select_item ( PtkIconView *icon_view,
                            PtkIconViewItem *item )
{
    if ( item->selected || icon_view->priv->selection_mode == GTK_SELECTION_NONE )
        return ;
    else if ( icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        ptk_icon_view_unselect_all_internal ( icon_view );

    item->selected = TRUE;

    ptk_icon_view_queue_draw_item ( icon_view, item );

    g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_unselect_item ( PtkIconView *icon_view,
                              PtkIconViewItem *item )
{
    if ( !item->selected )
        return ;

    if ( icon_view->priv->selection_mode == GTK_SELECTION_NONE ||
            icon_view->priv->selection_mode == GTK_SELECTION_BROWSE )
        return ;

    item->selected = FALSE;

    g_signal_emit ( G_OBJECT ( icon_view ), icon_view_signals[ SELECTION_CHANGED ], 0 );

    ptk_icon_view_queue_draw_item ( icon_view, item );
}



static void
ptk_icon_view_row_changed ( GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            PtkIconView *icon_view )
{
    PtkIconViewItem * item;

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );

    /* stop editing this item */
    if ( G_UNLIKELY ( item == icon_view->priv->edited_item ) )
        ptk_icon_view_stop_editing ( icon_view, TRUE );

    /* emit "selection-changed" if the item is selected */
    if ( G_UNLIKELY ( item->selected ) )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );

    /* recalculate layout */
    ptk_icon_view_item_invalidate_size ( item );
    ptk_icon_view_queue_layout ( icon_view );
}



static void
ptk_icon_view_row_inserted ( GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    GList *lp;
    gint index;

    index = gtk_tree_path_get_indices ( path ) [ 0 ];

    /* allocate the new item */
#if GLIB_CHECK_VERSION(2, 10, 0)
    item = g_slice_new0( PtkIconViewItem );
#else
    item = g_chunk_new0 ( PtkIconViewItem, icon_view->priv->items_chunk );
#endif
    item->iter = *iter;
    item->index = index;
    item->area.width = -1;
    item->area.height = -1;
    icon_view->priv->items = g_list_insert ( icon_view->priv->items, item, index );

    /* fix the indices of the following items */
    for ( lp = g_list_nth ( icon_view->priv->items, index + 1 ); lp != NULL; lp = lp->next )
        PTK_ICON_VIEW_ITEM ( lp->data ) ->index++;

    /* invalidate the prelited item */
    icon_view->priv->prelit_item = NULL;

    /* recalculate the layout */
    ptk_icon_view_queue_layout ( icon_view );
}



static void
ptk_icon_view_row_deleted ( GtkTreeModel *model,
                            GtkTreePath *path,
                            PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    gboolean changed = FALSE;
    GList *list;
    GList *next;

    /* determine the position and the item for the path */
    list = g_list_nth ( icon_view->priv->items, gtk_tree_path_get_indices ( path ) [ 0 ] );
    item = list->data;

    if ( G_UNLIKELY ( item == icon_view->priv->edited_item ) )
        ptk_icon_view_stop_editing ( icon_view, TRUE );

    if ( G_UNLIKELY ( item == icon_view->priv->anchor_item ) )
        icon_view->priv->anchor_item = NULL;

    if ( G_UNLIKELY ( item == icon_view->priv->cursor_item ) )
        icon_view->priv->cursor_item = NULL;

    if ( G_UNLIKELY ( item == icon_view->priv->prelit_item ) )
        icon_view->priv->prelit_item = NULL;

    /* check if the selection changed */
    if ( G_UNLIKELY ( item->selected ) )
        changed = TRUE;

    /* release the item resources */
    g_free ( item->box );

    /* update the indices of the following items */
    for ( next = list->next; next; next = next->next )
        PTK_ICON_VIEW_ITEM ( next->data ) ->index--;

    /* drop the item from the list */
#if GLIB_CHECK_VERSION(2, 10, 0)
    g_slice_free( PtkIconViewItem, list->data );
#endif
    icon_view->priv->items = g_list_delete_link ( icon_view->priv->items, list );

#if ! GLIB_CHECK_VERSION(2, 10, 0)
    /* reset the item chunk if this was the last item */
    if ( G_UNLIKELY ( icon_view->priv->items == NULL ) )
        g_mem_chunk_reset ( icon_view->priv->items_chunk );
#endif

    /* recalculate the layout */
    ptk_icon_view_queue_layout ( icon_view );

    /* if we removed a previous selected item, we need
     * to tell others that we have a new selection.
     */
    if ( G_UNLIKELY ( changed ) )
        g_signal_emit ( G_OBJECT ( icon_view ), icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_rows_reordered ( GtkTreeModel *model,
                               GtkTreePath *parent,
                               GtkTreeIter *iter,
                               gint *new_order,
                               PtkIconView *icon_view )
{
    int i;
    int length;
    GList *items = NULL, *list;
    PtkIconViewItem **item_array;
    gint *order;

    ptk_icon_view_stop_editing ( icon_view, TRUE );

    length = gtk_tree_model_iter_n_children ( model, NULL );

    /* alloca() is safe on gcc/i386 */
    item_array = g_newa ( PtkIconViewItem *, length );
    order = g_newa ( gint, length );

    for ( i = 0; i < length; i++ )
        order [ new_order[ i ] ] = i;

    for ( i = 0, list = icon_view->priv->items; list != NULL; list = list->next, i++ )
        item_array[ order[ i ] ] = list->data;

    for ( i = length - 1; i >= 0; i-- )
    {
        item_array[ i ] ->index = i;
        items = g_list_prepend ( items, item_array[ i ] );
    }

    g_list_free ( icon_view->priv->items );
    icon_view->priv->items = items;

    ptk_icon_view_queue_layout ( icon_view );
}



static void
ptk_icon_view_add_move_binding ( GtkBindingSet *binding_set,
                                 guint keyval,
                                 guint modmask,
                                 GtkMovementStep step,
                                 gint count )
{

    gtk_binding_entry_add_signal ( binding_set, keyval, modmask, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count );
    gtk_binding_entry_add_signal ( binding_set, keyval, GDK_SHIFT_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count );

    if ( ( modmask & GDK_CONTROL_MASK ) != GDK_CONTROL_MASK )
    {
        gtk_binding_entry_add_signal ( binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count );
        gtk_binding_entry_add_signal ( binding_set, keyval, GDK_CONTROL_MASK, "move-cursor", 2, G_TYPE_ENUM, step, G_TYPE_INT, count );
    }
}



static gboolean
ptk_icon_view_real_move_cursor ( PtkIconView *icon_view,
                                 GtkMovementStep step,
                                 gint count )
{
    GdkModifierType state;

    g_return_val_if_fail ( PTK_ICON_VIEW ( icon_view ), FALSE );
    g_return_val_if_fail ( step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
                           step == GTK_MOVEMENT_VISUAL_POSITIONS ||
                           step == GTK_MOVEMENT_DISPLAY_LINES ||
                           step == GTK_MOVEMENT_PAGES ||
                           step == GTK_MOVEMENT_BUFFER_ENDS, FALSE );

    if ( !GTK_WIDGET_HAS_FOCUS ( GTK_WIDGET ( icon_view ) ) )
        return FALSE;

    ptk_icon_view_stop_editing ( icon_view, FALSE );
    PTK_ICON_VIEW_SET_FLAG ( icon_view, PTK_ICON_VIEW_DRAW_KEYFOCUS );
    gtk_widget_grab_focus ( GTK_WIDGET ( icon_view ) );

    if ( gtk_get_current_event_state ( &state ) )
    {
        if ( ( state & GDK_CONTROL_MASK ) == GDK_CONTROL_MASK )
            icon_view->priv->ctrl_pressed = TRUE;
        if ( ( state & GDK_SHIFT_MASK ) == GDK_SHIFT_MASK )
            icon_view->priv->shift_pressed = TRUE;
    }
    /* else we assume not pressed */

    switch ( step )
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
        ptk_icon_view_move_cursor_left_right ( icon_view, count );
        break;
    case GTK_MOVEMENT_DISPLAY_LINES:
        ptk_icon_view_move_cursor_up_down ( icon_view, count );
        break;
    case GTK_MOVEMENT_PAGES:
        ptk_icon_view_move_cursor_page_up_down ( icon_view, count );
        break;
    case GTK_MOVEMENT_BUFFER_ENDS:
        ptk_icon_view_move_cursor_start_end ( icon_view, count );
        break;
    default:
        g_assert_not_reached ();
    }

    icon_view->priv->ctrl_pressed = FALSE;
    icon_view->priv->shift_pressed = FALSE;

    return TRUE;
}



static PtkIconViewItem*
find_item ( PtkIconView *icon_view,
            PtkIconViewItem *current,
            gint row_ofs,
            gint col_ofs )
{
    PtkIconViewItem * item;
    GList *lp;
    gint col;
    gint row;

    row = current->row + row_ofs;
    col = current->col + col_ofs;

    for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
    {
        item = PTK_ICON_VIEW_ITEM ( lp->data );
        if ( G_UNLIKELY ( item->row == row && item->col == col ) )
            return item;
    }

    return NULL;
}



static gint
find_cell ( PtkIconView *icon_view,
            PtkIconViewItem *item,
            gint cell,
            GtkOrientation orientation,
            gint step,
            gint *count )
{
    gint n_focusable;
    gint *focusable;
    gint first_text;
    gint current;
    gint i, k;
    GList *l;

    if ( icon_view->priv->orientation != orientation )
        return cell;

    ptk_icon_view_set_cell_data ( icon_view, item );

    focusable = g_new0 ( gint, icon_view->priv->n_cells );
    n_focusable = 0;

    first_text = 0;
    current = 0;
    for ( k = 0; k < 2; k++ )
        for ( l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++ )
        {
            PtkIconViewCellInfo * info = ( PtkIconViewCellInfo * ) l->data;

            if ( info->pack == ( k ? GTK_PACK_START : GTK_PACK_END ) )
                continue;

            if ( !info->cell->visible )
                continue;

            if ( PTK_IS_TEXT_RENDERER ( info->cell ) )
                first_text = i;

            if ( info->cell->mode != GTK_CELL_RENDERER_MODE_INERT )
            {
                if ( cell == i )
                    current = n_focusable;

                focusable[ n_focusable ] = i;

                n_focusable++;
            }
        }

    if ( n_focusable == 0 )
        focusable[ n_focusable++ ] = first_text;

    if ( cell < 0 )
    {
        current = step > 0 ? 0 : n_focusable - 1;
        cell = focusable[ current ];
    }

    if ( current + *count < 0 )
    {
        cell = -1;
        *count = current + *count;
    }
    else if ( current + *count > n_focusable - 1 )
    {
        cell = -1;
        *count = current + *count - ( n_focusable - 1 );
    }
    else
    {
        cell = focusable[ current + *count ];
        *count = 0;
    }

    g_free ( focusable );

    return cell;
}



static PtkIconViewItem *
find_item_page_up_down ( PtkIconView *icon_view,
                         PtkIconViewItem *current,
                         gint count )
{
    GList * item = g_list_find ( icon_view->priv->items, current );
    GList *next;
    gint col = current->col;
    gint y = current->area.y + count * icon_view->priv->vadjustment->page_size;

    if ( count > 0 )
    {
        for ( ; item != NULL; item = item->next )
        {
            for ( next = item->next; next; next = next->next )
                if ( PTK_ICON_VIEW_ITEM ( next->data ) ->col == col )
                    break;

            if ( next == NULL || PTK_ICON_VIEW_ITEM ( next->data ) ->area.y > y )
                break;
        }
    }
    else
    {
        for ( ; item != NULL; item = item->prev )
        {
            for ( next = item->prev; next; next = next->prev )
                if ( PTK_ICON_VIEW_ITEM ( next->data ) ->col == col )
                    break;

            if ( next == NULL || PTK_ICON_VIEW_ITEM ( next->data ) ->area.y < y )
                break;
        }
    }

    return ( item != NULL ) ? item->data : NULL;
}



static gboolean
ptk_icon_view_select_all_between ( PtkIconView *icon_view,
                                   PtkIconViewItem *anchor,
                                   PtkIconViewItem *cursor )
{
    GList * items;
    PtkIconViewItem *item;
    gint row1, row2, col1, col2;
    gboolean dirty = FALSE;

    if ( anchor->row < cursor->row )
    {
        row1 = anchor->row;
        row2 = cursor->row;
    }
    else
    {
        row1 = cursor->row;
        row2 = anchor->row;
    }

    if ( anchor->col < cursor->col )
    {
        col1 = anchor->col;
        col2 = cursor->col;
    }
    else
    {
        col1 = cursor->col;
        col2 = anchor->col;
    }

    for ( items = icon_view->priv->items; items; items = items->next )
    {
        item = items->data;

        if ( row1 <= item->row && item->row <= row2 &&
                col1 <= item->col && item->col <= col2 )
        {
            if ( !item->selected )
                dirty = TRUE;

            item->selected = TRUE;

            ptk_icon_view_queue_draw_item ( icon_view, item );
        }
    }

    return dirty;
}



static void
ptk_icon_view_move_cursor_up_down ( PtkIconView *icon_view,
                                    gint count )
{
    PtkIconViewItem * item;
    gint cell;
    gboolean dirty = FALSE;
    gint step;

    if ( !GTK_WIDGET_HAS_FOCUS ( icon_view ) )
        return ;

    if ( !icon_view->priv->cursor_item )
    {
        GList * list;

        if ( count > 0 )
            list = icon_view->priv->items;
        else
            list = g_list_last ( icon_view->priv->items );

        item = list ? list->data : NULL;
        cell = -1;
    }
    else
    {
        item = icon_view->priv->cursor_item;
        cell = icon_view->priv->cursor_cell;
        step = count > 0 ? 1 : -1;
        while ( item )
        {
            cell = find_cell ( icon_view, item, cell,
                               GTK_ORIENTATION_VERTICAL,
                               step, &count );
            if ( count == 0 )
                break;

            item = find_item ( icon_view, item, step, 0 );
            count = count - step;
        }
    }

    if ( !item )
        return ;

    if ( icon_view->priv->ctrl_pressed ||
            !icon_view->priv->shift_pressed ||
            !icon_view->priv->anchor_item ||
            icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        icon_view->priv->anchor_item = item;

    ptk_icon_view_set_cursor_item ( icon_view, item, cell );

    if ( !icon_view->priv->ctrl_pressed &&
            icon_view->priv->selection_mode != GTK_SELECTION_NONE )
    {
        dirty = ptk_icon_view_unselect_all_internal ( icon_view );
        dirty = ptk_icon_view_select_all_between ( icon_view,
                                                   icon_view->priv->anchor_item,
                                                   item ) || dirty;
    }

    ptk_icon_view_scroll_to_item ( icon_view, item );

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_move_cursor_page_up_down ( PtkIconView *icon_view,
                                         gint count )
{
    PtkIconViewItem * item;
    gboolean dirty = FALSE;

    if ( !GTK_WIDGET_HAS_FOCUS ( icon_view ) )
        return ;

    if ( !icon_view->priv->cursor_item )
    {
        GList * list;

        if ( count > 0 )
            list = icon_view->priv->items;
        else
            list = g_list_last ( icon_view->priv->items );

        item = list ? list->data : NULL;
    }
    else
        item = find_item_page_up_down ( icon_view,
                                        icon_view->priv->cursor_item,
                                        count );

    if ( !item )
        return ;

    if ( icon_view->priv->ctrl_pressed ||
            !icon_view->priv->shift_pressed ||
            !icon_view->priv->anchor_item ||
            icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        icon_view->priv->anchor_item = item;

    ptk_icon_view_set_cursor_item ( icon_view, item, -1 );

    if ( !icon_view->priv->ctrl_pressed &&
            icon_view->priv->selection_mode != GTK_SELECTION_NONE )
    {
        dirty = ptk_icon_view_unselect_all_internal ( icon_view );
        dirty = ptk_icon_view_select_all_between ( icon_view,
                                                   icon_view->priv->anchor_item,
                                                   item ) || dirty;
    }

    ptk_icon_view_scroll_to_item ( icon_view, item );

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_move_cursor_left_right ( PtkIconView *icon_view,
                                       gint count )
{
    PtkIconViewItem * item;
    gint cell = -1;
    gboolean dirty = FALSE;
    gint step;

    if ( !GTK_WIDGET_HAS_FOCUS ( icon_view ) )
        return ;

    if ( !icon_view->priv->cursor_item )
    {
        GList * list;

        if ( count > 0 )
            list = icon_view->priv->items;
        else
            list = g_list_last ( icon_view->priv->items );

        item = list ? list->data : NULL;
    }
    else
    {
        item = icon_view->priv->cursor_item;
        cell = icon_view->priv->cursor_cell;
        step = count > 0 ? 1 : -1;
        while ( item )
        {
            cell = find_cell ( icon_view, item, cell,
                               GTK_ORIENTATION_HORIZONTAL,
                               step, &count );
            if ( count == 0 )
                break;

            item = find_item ( icon_view, item, 0, step );
            count = count - step;
        }
    }

    if ( !item )
        return ;

    if ( icon_view->priv->ctrl_pressed ||
            !icon_view->priv->shift_pressed ||
            !icon_view->priv->anchor_item ||
            icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        icon_view->priv->anchor_item = item;

    ptk_icon_view_set_cursor_item ( icon_view, item, cell );

    if ( !icon_view->priv->ctrl_pressed &&
            icon_view->priv->selection_mode != GTK_SELECTION_NONE )
    {
        dirty = ptk_icon_view_unselect_all_internal ( icon_view );
        dirty = ptk_icon_view_select_all_between ( icon_view,
                                                   icon_view->priv->anchor_item,
                                                   item ) || dirty;
    }

    ptk_icon_view_scroll_to_item ( icon_view, item );

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_move_cursor_start_end ( PtkIconView *icon_view,
                                      gint count )
{
    PtkIconViewItem * item;
    GList *list;
    gboolean dirty = FALSE;

    if ( !GTK_WIDGET_HAS_FOCUS ( icon_view ) )
        return ;

    if ( count < 0 )
        list = icon_view->priv->items;
    else
        list = g_list_last ( icon_view->priv->items );

    item = list ? list->data : NULL;

    if ( !item )
        return ;

    if ( icon_view->priv->ctrl_pressed ||
            !icon_view->priv->shift_pressed ||
            !icon_view->priv->anchor_item ||
            icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        icon_view->priv->anchor_item = item;

    ptk_icon_view_set_cursor_item ( icon_view, item, -1 );

    if ( !icon_view->priv->ctrl_pressed &&
            icon_view->priv->selection_mode != GTK_SELECTION_NONE )
    {
        dirty = ptk_icon_view_unselect_all_internal ( icon_view );
        dirty = ptk_icon_view_select_all_between ( icon_view,
                                                   icon_view->priv->anchor_item,
                                                   item ) || dirty;
    }

    ptk_icon_view_scroll_to_item ( icon_view, item );

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



static void
ptk_icon_view_scroll_to_item ( PtkIconView *icon_view,
                               PtkIconViewItem *item )
{
    gint x, y, width, height;
    gint focus_width;

    gtk_widget_style_get ( GTK_WIDGET ( icon_view ),
                           "focus-line-width", &focus_width,
                           NULL );

    gdk_drawable_get_size ( GDK_DRAWABLE ( icon_view->priv->bin_window ),
                            &width, &height );
    gdk_window_get_position ( icon_view->priv->bin_window, &x, &y );

    if ( y + item->area.y - focus_width < 0 )
        gtk_adjustment_set_value ( icon_view->priv->vadjustment,
                                   icon_view->priv->vadjustment->value + y + item->area.y - focus_width );
    else if ( y + item->area.y + item->area.height + focus_width > GTK_WIDGET ( icon_view ) ->allocation.height )
        gtk_adjustment_set_value ( icon_view->priv->vadjustment,
                                   icon_view->priv->vadjustment->value + y + item->area.y + item->area.height
                                   + focus_width - GTK_WIDGET ( icon_view ) ->allocation.height );

    if ( x + item->area.x - focus_width < 0 )
    {
        gtk_adjustment_set_value ( icon_view->priv->hadjustment,
                                   icon_view->priv->hadjustment->value + x + item->area.x - focus_width );
    }
    else if ( x + item->area.x + item->area.width + focus_width > GTK_WIDGET ( icon_view ) ->allocation.width )
    {
        gtk_adjustment_set_value ( icon_view->priv->hadjustment,
                                   icon_view->priv->hadjustment->value + x + item->area.x + item->area.width
                                   + focus_width - GTK_WIDGET ( icon_view ) ->allocation.width );
    }

    gtk_adjustment_changed ( icon_view->priv->hadjustment );
    gtk_adjustment_changed ( icon_view->priv->vadjustment );
}



static PtkIconViewCellInfo *
ptk_icon_view_get_cell_info ( PtkIconView *icon_view,
                              GtkCellRenderer *renderer )
{
    GList * lp;

    for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
        if ( PTK_ICON_VIEW_CELL_INFO ( lp->data ) ->cell == renderer )
            return lp->data;

    return NULL;
}



static void
ptk_icon_view_set_cell_data ( const PtkIconView *icon_view,
                              PtkIconViewItem *item )
{
    PtkIconViewCellInfo * info;
    GtkTreePath *path;
    GtkTreeIter iter;
    GValue value = {0, };
    GSList *slp;
    GList *lp;

    if ( G_UNLIKELY ( !PTK_ICON_VIEW_FLAG_SET ( icon_view, PTK_ICON_VIEW_ITERS_PERSIST ) ) )
    {
        path = gtk_tree_path_new_from_indices ( item->index, -1 );
        gtk_tree_model_get_iter ( icon_view->priv->model, &iter, path );
        gtk_tree_path_free ( path );
    }
    else
    {
        iter = item->iter;
    }

    for ( lp = icon_view->priv->cell_list; lp != NULL; lp = lp->next )
    {
        info = PTK_ICON_VIEW_CELL_INFO ( lp->data );

        for ( slp = info->attributes; slp != NULL && slp->next != NULL; slp = slp->next->next )
        {
            gtk_tree_model_get_value ( icon_view->priv->model, &iter, GPOINTER_TO_INT ( slp->next->data ), &value );
            g_object_set_property ( G_OBJECT ( info->cell ), slp->data, &value );
            g_value_unset ( &value );
        }

        if ( G_UNLIKELY ( info->func != NULL ) )
            ( *info->func ) ( GTK_CELL_LAYOUT ( icon_view ), info->cell, icon_view->priv->model, &iter, info->func_data );
    }
}



static void
free_cell_attributes ( PtkIconViewCellInfo *info )
{
    GSList * lp;

    for ( lp = info->attributes; lp != NULL && lp->next != NULL; lp = lp->next->next )
        g_free ( lp->data );
    g_slist_free ( info->attributes );
    info->attributes = NULL;
}



static void
free_cell_info ( PtkIconViewCellInfo *info )
{
    if ( G_UNLIKELY ( info->destroy != NULL ) )
        ( *info->destroy ) ( info->func_data );

    g_object_unref ( G_OBJECT ( info->cell ) );
    free_cell_attributes ( info );
    g_free ( info );
}



static void
ptk_icon_view_cell_layout_pack_start ( GtkCellLayout *layout,
                                       GtkCellRenderer *renderer,
                                       gboolean expand )
{
    PtkIconViewCellInfo * info;
    PtkIconView *icon_view = PTK_ICON_VIEW ( layout );

    g_return_if_fail ( GTK_IS_CELL_RENDERER ( renderer ) );
    g_return_if_fail ( ptk_icon_view_get_cell_info ( icon_view, renderer ) == NULL );

    g_object_ref ( renderer );
    gtk_object_sink ( GTK_OBJECT ( renderer ) );

    info = g_new0 ( PtkIconViewCellInfo, 1 );
    info->cell = renderer;
    info->expand = expand ? TRUE : FALSE;
    info->pack = GTK_PACK_START;
    info->position = icon_view->priv->n_cells;

    icon_view->priv->cell_list = g_list_append ( icon_view->priv->cell_list, info );
    icon_view->priv->n_cells++;

    ptk_icon_view_invalidate_sizes ( icon_view );
}



static void
ptk_icon_view_cell_layout_pack_end ( GtkCellLayout *layout,
                                     GtkCellRenderer *renderer,
                                     gboolean expand )
{
    PtkIconViewCellInfo * info;
    PtkIconView *icon_view = PTK_ICON_VIEW ( layout );

    g_return_if_fail ( GTK_IS_CELL_RENDERER ( renderer ) );
    g_return_if_fail ( ptk_icon_view_get_cell_info ( icon_view, renderer ) == NULL );

    g_object_ref ( renderer );
    gtk_object_sink ( GTK_OBJECT ( renderer ) );

    info = g_new0 ( PtkIconViewCellInfo, 1 );
    info->cell = renderer;
    info->expand = expand ? TRUE : FALSE;
    info->pack = GTK_PACK_END;
    info->position = icon_view->priv->n_cells;

    icon_view->priv->cell_list = g_list_append ( icon_view->priv->cell_list, info );
    icon_view->priv->n_cells++;

    ptk_icon_view_invalidate_sizes ( icon_view );
}



static void
ptk_icon_view_cell_layout_add_attribute ( GtkCellLayout *layout,
                                          GtkCellRenderer *renderer,
                                          const gchar *attribute,
                                          gint column )
{
    PtkIconViewCellInfo * info;

    info = ptk_icon_view_get_cell_info ( PTK_ICON_VIEW ( layout ), renderer );
    if ( G_LIKELY ( info != NULL ) )
    {
        info->attributes = g_slist_prepend ( info->attributes, GINT_TO_POINTER ( column ) );
        info->attributes = g_slist_prepend ( info->attributes, g_strdup ( attribute ) );

        ptk_icon_view_invalidate_sizes ( PTK_ICON_VIEW ( layout ) );
    }
}



static void
ptk_icon_view_cell_layout_clear ( GtkCellLayout *layout )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( layout );

    g_list_foreach ( icon_view->priv->cell_list, ( GFunc ) free_cell_info, NULL );
    g_list_free ( icon_view->priv->cell_list );
    icon_view->priv->cell_list = NULL;
    icon_view->priv->n_cells = 0;

    ptk_icon_view_invalidate_sizes ( icon_view );
}



static void
ptk_icon_view_cell_layout_set_cell_data_func ( GtkCellLayout *layout,
                                               GtkCellRenderer *cell,
                                               GtkCellLayoutDataFunc func,
                                               gpointer func_data,
                                               GDestroyNotify destroy )
{
    PtkIconViewCellInfo * info;
    GDestroyNotify notify;

    info = ptk_icon_view_get_cell_info ( PTK_ICON_VIEW ( layout ), cell );
    if ( G_LIKELY ( info != NULL ) )
    {
        if ( G_UNLIKELY ( info->destroy != NULL ) )
        {
            notify = info->destroy;
            info->destroy = NULL;
            ( *notify ) ( info->func_data );
        }

        info->func = func;
        info->func_data = func_data;
        info->destroy = destroy;

        ptk_icon_view_invalidate_sizes ( PTK_ICON_VIEW ( layout ) );
    }
}



static void
ptk_icon_view_cell_layout_clear_attributes ( GtkCellLayout *layout,
                                             GtkCellRenderer *renderer )
{
    PtkIconViewCellInfo * info;

    info = ptk_icon_view_get_cell_info ( PTK_ICON_VIEW ( layout ), renderer );
    if ( G_LIKELY ( info != NULL ) )
    {
        free_cell_attributes ( info );

        ptk_icon_view_invalidate_sizes ( PTK_ICON_VIEW ( layout ) );
    }
}



static void
ptk_icon_view_cell_layout_reorder ( GtkCellLayout *layout,
                                    GtkCellRenderer *cell,
                                    gint position )
{
    PtkIconViewCellInfo * info;
    PtkIconView *icon_view = PTK_ICON_VIEW ( layout );
    GList *lp;
    gint n;

    icon_view = PTK_ICON_VIEW ( layout );

    info = ptk_icon_view_get_cell_info ( icon_view, cell );
    if ( G_LIKELY ( info != NULL ) )
    {
        lp = g_list_find ( icon_view->priv->cell_list, info );

        icon_view->priv->cell_list = g_list_remove_link ( icon_view->priv->cell_list, lp );
        icon_view->priv->cell_list = g_list_insert ( icon_view->priv->cell_list, info, position );

        for ( lp = icon_view->priv->cell_list, n = 0; lp != NULL; lp = lp->next, ++n )
            PTK_ICON_VIEW_CELL_INFO ( lp->data ) ->position = n;

        ptk_icon_view_invalidate_sizes ( icon_view );
    }
}



/**
 * ptk_icon_view_new:
 *
 * Creates a new #PtkIconView widget
 *
 * Return value: A newly created #PtkIconView widget
 **/
GtkWidget*
ptk_icon_view_new ( void )
{
    return g_object_new ( PTK_TYPE_ICON_VIEW, NULL );
}



/**
 * ptk_icon_view_new_with_model:
 * @model: The model.
 *
 * Creates a new #PtkIconView widget with the model @model.
 *
 * Return value: A newly created #PtkIconView widget.
 **/
GtkWidget*
ptk_icon_view_new_with_model ( GtkTreeModel *model )
{
    g_return_val_if_fail ( model == NULL || GTK_IS_TREE_MODEL ( model ), NULL );

    return g_object_new ( PTK_TYPE_ICON_VIEW,
                          "model", model,
                          NULL );
}



/**
 * ptk_icon_view_widget_to_icon_coords:
 * @icon_view : a #PtkIconView.
 * @wx        : widget x coordinate.
 * @wy        : widget y coordinate.
 * @ix        : return location for icon x coordinate or %NULL.
 * @iy        : return location for icon y coordinate or %NULL.
 *
 * Converts widget coordinates to coordinates for the icon window
 * (the full scrollable area of the icon view).
 **/
void
ptk_icon_view_widget_to_icon_coords ( const PtkIconView *icon_view,
                                      gint wx,
                                      gint wy,
                                      gint *ix,
                                      gint *iy )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( ix != NULL ) )
        * ix = wx + icon_view->priv->hadjustment->value;
    if ( G_LIKELY ( iy != NULL ) )
        * iy = wy + icon_view->priv->vadjustment->value;
}



/**
 * ptk_icon_view_icon_to_widget_coords:
 * @icon_view : a #PtkIconView.
 * @ix        : icon x coordinate.
 * @iy        : icon y coordinate.
 * @wx        : return location for widget x coordinate or %NULL.
 * @wy        : return location for widget y coordinate or %NULL.
 *
 * Converts icon view coordinates (coordinates in full scrollable
 * area of the icon view) to widget coordinates.
 **/
void
ptk_icon_view_icon_to_widget_coords ( const PtkIconView *icon_view,
                                      gint ix,
                                      gint iy,
                                      gint *wx,
                                      gint *wy )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( wx != NULL ) )
        * wx = ix - icon_view->priv->hadjustment->value;
    if ( G_LIKELY ( wy != NULL ) )
        * wy = iy - icon_view->priv->vadjustment->value;
}



/**
 * ptk_icon_view_get_path_at_pos:
 * @icon_view : A #PtkIconView.
 * @x         : The x position to be identified
 * @y         : The y position to be identified
 *
 * Finds the path at the point (@x, @y), relative to widget coordinates.
 * See ptk_icon_view_get_item_at_pos(), if you are also interested in
 * the cell at the specified position.
 *
 * Return value: The #GtkTreePath corresponding to the icon or %NULL
 *               if no icon exists at that position.
 **/
GtkTreePath*
ptk_icon_view_get_path_at_pos ( const PtkIconView *icon_view,
                                gint x,
                                gint y )
{
    PtkIconViewItem * item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), NULL );

    /* translate the widget coordinates to icon window coordinates */
    /*
    x += icon_view->priv->hadjustment->value;
    y += icon_view->priv->vadjustment->value;
    */
    item = ptk_icon_view_get_item_at_coords ( icon_view, x, y, TRUE, NULL );
    return ( item != NULL ) ? gtk_tree_path_new_from_indices ( item->index, -1 ) : NULL;
}



/**
 * ptk_icon_view_get_item_at_pos:
 * @icon_view: A #PtkIconView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 * @path: Return location for the path, or %NULL
 * @cell: Return location for the renderer responsible for the cell
 *   at (@x, @y), or %NULL
 *
 * Finds the path at the point (@x, @y), relative to widget coordinates.
 * In contrast to ptk_icon_view_get_path_at_pos(), this function also
 * obtains the cell at the specified position. The returned path should
 * be freed with gtk_tree_path_free().
 *
 * Return value: %TRUE if an item exists at the specified position
 *
 * Since: 0.3.1
 **/
gboolean
ptk_icon_view_get_item_at_pos ( const PtkIconView *icon_view,
                                gint x,
                                gint y,
                                GtkTreePath **path,
                                GtkCellRenderer **cell )
{
    PtkIconViewCellInfo * info;
    PtkIconViewItem *item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    item = ptk_icon_view_get_item_at_coords ( icon_view, x, y, TRUE, &info );

    if ( G_LIKELY ( path != NULL ) )
        * path = ( item != NULL ) ? gtk_tree_path_new_from_indices ( item->index, -1 ) : NULL;

    if ( G_LIKELY ( cell != NULL ) )
        * cell = ( info != NULL ) ? info->cell : NULL;

    return ( item != NULL );
}



/**
 * ptk_icon_view_get_visible_range:
 * @icon_view  : A #PtkIconView
 * @start_path : Return location for start of region, or %NULL
 * @end_path   : Return location for end of region, or %NULL
 *
 * Sets @start_path and @end_path to be the first and last visible path.
 * Note that there may be invisible paths in between.
 *
 * Both paths should be freed with gtk_tree_path_free() after use.
 *
 * Return value: %TRUE, if valid paths were placed in @start_path and @end_path
 *
 * Since: 0.3.1
 **/
gboolean
ptk_icon_view_get_visible_range ( const PtkIconView *icon_view,
                                  GtkTreePath **start_path,
                                  GtkTreePath **end_path )
{
    GList * icons;
    gint start_index = -1;
    gint end_index = -1;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    if ( icon_view->priv->hadjustment == NULL ||
            icon_view->priv->vadjustment == NULL )
        return FALSE;

    if ( start_path == NULL && end_path == NULL )
        return FALSE;

    for ( icons = icon_view->priv->items; icons; icons = icons->next )
    {
        PtkIconViewItem * item = icons->data;

        if ( ( item->area.x + item->area.width >= ( int ) icon_view->priv->hadjustment->value ) &&
                ( item->area.y + item->area.height >= ( int ) icon_view->priv->vadjustment->value ) &&
                ( item->area.x <= ( int ) ( icon_view->priv->hadjustment->value + icon_view->priv->hadjustment->page_size ) ) &&
                ( item->area.y <= ( int ) ( icon_view->priv->vadjustment->value + icon_view->priv->vadjustment->page_size ) ) )
        {
            if ( start_index == -1 )
                start_index = item->index;
            end_index = item->index;
        }
    }

    if ( start_path && start_index != -1 )
        * start_path = gtk_tree_path_new_from_indices ( start_index, -1 );
    if ( end_path && end_index != -1 )
        * end_path = gtk_tree_path_new_from_indices ( end_index, -1 );

    return start_index != -1;
}



/**
 * ptk_icon_view_selected_foreach:
 * @icon_view : A #PtkIconView.
 * @func      : The funcion to call for each selected icon.
 * @data      : User data to pass to the function.
 *
 * Calls a function for each selected icon. Note that the model or
 * selection cannot be modified from within this function.
 **/
void
ptk_icon_view_selected_foreach ( PtkIconView *icon_view,
                                 PtkIconViewForeachFunc func,
                                 gpointer data )
{
    GList * list;

    for ( list = icon_view->priv->items; list; list = list->next )
    {
        PtkIconViewItem * item = list->data;
        GtkTreePath *path = gtk_tree_path_new_from_indices ( item->index, -1 );

        if ( item->selected )
            ( * func ) ( icon_view, path, data );

        gtk_tree_path_free ( path );
    }
}



/**
 * ptk_icon_view_get_selection_mode:
 * @icon_view : A #PtkIconView.
 *
 * Gets the selection mode of the @icon_view.
 *
 * Return value: the current selection mode
 **/
GtkSelectionMode
ptk_icon_view_get_selection_mode ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), GTK_SELECTION_SINGLE );
    return icon_view->priv->selection_mode;
}



/**
 * ptk_icon_view_set_selection_mode:
 * @icon_view : A #PtkIconView.
 * @mode      : The selection mode
 *
 * Sets the selection mode of the @icon_view.
 **/
void
ptk_icon_view_set_selection_mode ( PtkIconView *icon_view,
                                   GtkSelectionMode mode )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( mode != icon_view->priv->selection_mode ) )
    {
        if ( mode == GTK_SELECTION_NONE || icon_view->priv->selection_mode == GTK_SELECTION_MULTIPLE )
            ptk_icon_view_unselect_all ( icon_view );

        icon_view->priv->selection_mode = mode;

        g_object_notify ( G_OBJECT ( icon_view ), "selection-mode" );
    }
}



/**
 * ptk_icon_view_get_model:
 * @icon_view : a #PtkIconView
 *
 * Returns the model the #PtkIconView is based on. Returns %NULL if the
 * model is unset.
 *
 * Return value: A #GtkTreeModel, or %NULL if none is currently being used.
 **/
GtkTreeModel*
ptk_icon_view_get_model ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), NULL );
    return icon_view->priv->model;
}



/**
 * ptk_icon_view_set_model:
 * @icon_view : A #PtkIconView.
 * @model     : The model.
 *
 * Sets the model for a #PtkIconView.
 * If the @icon_view already has a model set, it will remove
 * it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 **/
void
ptk_icon_view_set_model ( PtkIconView *icon_view,
                          GtkTreeModel *model )
{
    PtkIconViewItem * item;
    GtkTreeIter iter;
    GList *items = NULL;
    GList *lp;
    gint index = 0;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( model == NULL || GTK_IS_TREE_MODEL ( model ) );

    /* verify that we don't already use that model */
    if ( G_UNLIKELY ( icon_view->priv->model == model ) )
        return ;

    /* verify the new model */
    if ( G_LIKELY ( model != NULL ) )
    {
        g_return_if_fail ( gtk_tree_model_get_flags ( model ) & GTK_TREE_MODEL_LIST_ONLY );

        if ( G_UNLIKELY ( icon_view->priv->pixbuf_column != -1 ) )
            g_return_if_fail ( gtk_tree_model_get_column_type ( model, icon_view->priv->pixbuf_column ) == GDK_TYPE_PIXBUF );

        if ( G_UNLIKELY ( icon_view->priv->text_column != -1 ) )
            g_return_if_fail ( gtk_tree_model_get_column_type ( model, icon_view->priv->text_column ) == G_TYPE_STRING );

        if ( G_UNLIKELY ( icon_view->priv->markup_column != -1 ) )
            g_return_if_fail ( gtk_tree_model_get_column_type ( model, icon_view->priv->markup_column ) == G_TYPE_STRING );
    }

    /* be sure to cancel any pending editor */
    ptk_icon_view_stop_editing ( icon_view, TRUE );

    /* disconnect from the previous model */
    if ( G_LIKELY ( icon_view->priv->model != NULL ) )
    {
        /* disconnect signals handlers from the previous model */
        g_signal_handlers_disconnect_by_func ( G_OBJECT ( icon_view->priv->model ), ptk_icon_view_row_changed, icon_view );
        g_signal_handlers_disconnect_by_func ( G_OBJECT ( icon_view->priv->model ), ptk_icon_view_row_inserted, icon_view );
        g_signal_handlers_disconnect_by_func ( G_OBJECT ( icon_view->priv->model ), ptk_icon_view_row_deleted, icon_view );
        g_signal_handlers_disconnect_by_func ( G_OBJECT ( icon_view->priv->model ), ptk_icon_view_rows_reordered, icon_view );

        /* release our reference on the model */
        g_object_unref ( G_OBJECT ( icon_view->priv->model ) );

        /* drop all items belonging to the previous model */
        for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
        {
            g_free ( PTK_ICON_VIEW_ITEM ( lp->data ) ->box );
#if GLIB_CHECK_VERSION(2, 10, 0)
            g_slice_free( PtkIconViewItem, lp->data );
#endif
        }
        g_list_free ( icon_view->priv->items );
        icon_view->priv->items = NULL;

#if ! GLIB_CHECK_VERSION(2, 10, 0)
        /* reset the item memory chunk as there are no items left now */
        g_mem_chunk_reset ( icon_view->priv->items_chunk );
#endif
        /* reset statistics */
        icon_view->priv->anchor_item = NULL;
        icon_view->priv->cursor_item = NULL;
        icon_view->priv->prelit_item = NULL;
        icon_view->priv->last_single_clicked = NULL;
        icon_view->priv->width = 0;
        icon_view->priv->height = 0;
    }

    /* activate the new model */
    icon_view->priv->model = model;

    /* connect to the new model */
    if ( G_LIKELY ( model != NULL ) )
    {
        /* take a reference on the model */
        g_object_ref ( G_OBJECT ( model ) );

        /* connect signals */
        g_signal_connect ( G_OBJECT ( model ), "row-changed", G_CALLBACK ( ptk_icon_view_row_changed ), icon_view );
        g_signal_connect ( G_OBJECT ( model ), "row-inserted", G_CALLBACK ( ptk_icon_view_row_inserted ), icon_view );
        g_signal_connect ( G_OBJECT ( model ), "row-deleted", G_CALLBACK ( ptk_icon_view_row_deleted ), icon_view );
        g_signal_connect ( G_OBJECT ( model ), "rows-reordered", G_CALLBACK ( ptk_icon_view_rows_reordered ), icon_view );

        /* check if the new model supports persistent iterators */
        if ( gtk_tree_model_get_flags ( model ) & GTK_TREE_MODEL_ITERS_PERSIST )
            PTK_ICON_VIEW_SET_FLAG ( icon_view, PTK_ICON_VIEW_ITERS_PERSIST );
        else
            PTK_ICON_VIEW_UNSET_FLAG ( icon_view, PTK_ICON_VIEW_ITERS_PERSIST );

        /* build up the initial items list */
        if ( gtk_tree_model_get_iter_first ( model, &iter ) )
        {
            do
            {
#if GLIB_CHECK_VERSION(2, 10, 0)
                item = g_slice_new0( PtkIconViewItem );
#else
                item = g_chunk_new0 ( PtkIconViewItem, icon_view->priv->items_chunk );
#endif
                item->iter = iter;
                item->index = index++;
                item->area.width = -1;
                item->area.height = -1;
                items = g_list_prepend ( items, item );
            }
            while ( gtk_tree_model_iter_next ( model, &iter ) );
        }
        icon_view->priv->items = g_list_reverse ( items );

        /* layout the new items */
        if ( !GTK_WIDGET_REALIZED ( icon_view ) )
            ptk_icon_view_queue_layout ( icon_view );
    }

    /* notify listeners */
    g_object_notify ( G_OBJECT ( icon_view ), "model" );

    if ( GTK_WIDGET_REALIZED ( icon_view ) )
        gtk_widget_queue_resize ( GTK_WIDGET ( icon_view ) );
}



static void
update_text_cell ( PtkIconView *icon_view )
{
    PtkIconViewCellInfo * info;
    GList *l;
    gint i;

    if ( icon_view->priv->text_column == -1 &&
            icon_view->priv->markup_column == -1 )
    {
        if ( icon_view->priv->text_cell != -1 )
        {
            info = g_list_nth_data ( icon_view->priv->cell_list,
                                     icon_view->priv->text_cell );

            g_list_remove ( icon_view->priv->cell_list, info );

            free_cell_info ( info );

            icon_view->priv->n_cells--;
            icon_view->priv->text_cell = -1;
        }
    }
    else
    {
        if ( icon_view->priv->text_cell == -1 )
        {
            GtkCellRenderer * cell = ptk_text_renderer_new ();
            gtk_cell_layout_pack_end ( GTK_CELL_LAYOUT ( icon_view ), cell, FALSE );
            for ( l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++ )
            {
                info = l->data;
                if ( info->cell == cell )
                {
                    icon_view->priv->text_cell = i;
                    break;
                }
            }
        }

        info = g_list_nth_data ( icon_view->priv->cell_list,
                                 icon_view->priv->text_cell );

        if ( icon_view->priv->markup_column != -1 )
            gtk_cell_layout_set_attributes ( GTK_CELL_LAYOUT ( icon_view ),
                                             info->cell,
                                             "markup", icon_view->priv->markup_column,
                                             NULL );
        else
            gtk_cell_layout_set_attributes ( GTK_CELL_LAYOUT ( icon_view ),
                                             info->cell,
                                             "text", icon_view->priv->text_column,
                                             NULL );
    }
}

static void
update_pixbuf_cell ( PtkIconView *icon_view )
{
    PtkIconViewCellInfo * info;
    GList *l;
    gint i;

    if ( icon_view->priv->pixbuf_column == -1 )
    {
        if ( icon_view->priv->pixbuf_cell != -1 )
        {
            info = g_list_nth_data ( icon_view->priv->cell_list,
                                     icon_view->priv->pixbuf_cell );

            g_list_remove ( icon_view->priv->cell_list, info );

            free_cell_info ( info );

            icon_view->priv->n_cells--;
            icon_view->priv->pixbuf_cell = -1;
        }
    }
    else
    {
        if ( icon_view->priv->pixbuf_cell == -1 )
        {
            GtkCellRenderer * cell = gtk_cell_renderer_pixbuf_new ();

            gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( icon_view ), cell, FALSE );
            for ( l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++ )
            {
                info = l->data;
                if ( info->cell == cell )
                {
                    icon_view->priv->pixbuf_cell = i;
                    break;
                }
            }
        }

        info = g_list_nth_data ( icon_view->priv->cell_list,
                                 icon_view->priv->pixbuf_cell );

        gtk_cell_layout_set_attributes ( GTK_CELL_LAYOUT ( icon_view ),
                                         info->cell,
                                         "pixbuf", icon_view->priv->pixbuf_column,
                                         NULL );
    }
}



/**
 * ptk_icon_view_get_text_column:
 * @icon_view: A #PtkIconView.
 *
 * Returns the column with text for @icon_view.
 *
 * Returns: the text column, or -1 if it's unset.
 */
gint
ptk_icon_view_get_text_column ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->text_column;
}



/**
 * ptk_icon_view_set_text_column:
 * @icon_view: A #PtkIconView.
 * @column: A column in the currently used model.
 *
 * Sets the column with text for @icon_view to be @column. The text
 * column must be of type #G_TYPE_STRING.
 **/
void
ptk_icon_view_set_text_column ( PtkIconView *icon_view,
                                gint column )
{
    GType column_type;

    if ( G_UNLIKELY ( column == icon_view->priv->text_column ) )
        return ;

    if ( column == -1 )
    {
        icon_view->priv->text_column = -1;
    }
    else
    {
        if ( icon_view->priv->model != NULL )
        {
            column_type = gtk_tree_model_get_column_type ( icon_view->priv->model, column );
            g_return_if_fail ( column_type == G_TYPE_STRING );
        }

        icon_view->priv->text_column = column;
    }

    ptk_icon_view_stop_editing ( icon_view, TRUE );

    update_text_cell ( icon_view );

    ptk_icon_view_invalidate_sizes ( icon_view );

    g_object_notify ( G_OBJECT ( icon_view ), "text-column" );
}



/**
 * ptk_icon_view_get_markup_column:
 * @icon_view: A #PtkIconView.
 *
 * Returns the column with markup text for @icon_view.
 *
 * Returns: the markup column, or -1 if it's unset.
 */
gint
ptk_icon_view_get_markup_column ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->markup_column;
}



/**
 * ptk_icon_view_set_markup_column:
 * @icon_view : A #PtkIconView.
 * @column    : A column in the currently used model.
 *
 * Sets the column with markup information for @icon_view to be
 * @column. The markup column must be of type #G_TYPE_STRING.
 * If the markup column is set to something, it overrides
 * the text column set by ptk_icon_view_set_text_column().
 **/
void
ptk_icon_view_set_markup_column ( PtkIconView *icon_view,
                                  gint column )
{
    if ( G_UNLIKELY ( column == icon_view->priv->markup_column ) )
        return ;

    if ( column == -1 )
        icon_view->priv->markup_column = -1;
    else
    {
        if ( icon_view->priv->model != NULL )
        {
            GType column_type;

            column_type = gtk_tree_model_get_column_type ( icon_view->priv->model, column );

            g_return_if_fail ( column_type == G_TYPE_STRING );
        }

        icon_view->priv->markup_column = column;
    }

    ptk_icon_view_stop_editing ( icon_view, TRUE );

    update_text_cell ( icon_view );

    ptk_icon_view_invalidate_sizes ( icon_view );

    g_object_notify ( G_OBJECT ( icon_view ), "markup-column" );
}



/**
 * ptk_icon_view_get_pixbuf_column:
 * @icon_view : A #PtkIconView.
 *
 * Returns the column with pixbufs for @icon_view.
 *
 * Returns: the pixbuf column, or -1 if it's unset.
 */
gint
ptk_icon_view_get_pixbuf_column ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->pixbuf_column;
}



/**
 * ptk_icon_view_set_pixbuf_column:
 * @icon_view : A #PtkIconView.
 * @column    : A column in the currently used model.
 *
 * Sets the column with pixbufs for @icon_view to be @column. The pixbuf
 * column must be of type #GDK_TYPE_PIXBUF
 **/
void
ptk_icon_view_set_pixbuf_column ( PtkIconView *icon_view,
                                  gint column )
{
    GType column_type;

    if ( G_UNLIKELY ( column == icon_view->priv->pixbuf_column ) )
        return ;

    if ( column == -1 )
    {
        icon_view->priv->pixbuf_column = -1;
    }
    else
    {
        if ( icon_view->priv->model != NULL )
        {
            column_type = gtk_tree_model_get_column_type ( icon_view->priv->model, column );
            g_return_if_fail ( column_type == GDK_TYPE_PIXBUF );
        }

        icon_view->priv->pixbuf_column = column;
    }

    ptk_icon_view_stop_editing ( icon_view, TRUE );

    update_pixbuf_cell ( icon_view );

    ptk_icon_view_invalidate_sizes ( icon_view );

    g_object_notify ( G_OBJECT ( icon_view ), "pixbuf-column" );

}



/**
 * ptk_icon_view_select_path:
 * @icon_view : A #PtkIconView.
 * @path      : The #GtkTreePath to be selected.
 *
 * Selects the row at @path.
 **/
void
ptk_icon_view_select_path ( PtkIconView *icon_view,
                            GtkTreePath *path )
{
    PtkIconViewItem * item;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( icon_view->priv->model != NULL );
    g_return_if_fail ( gtk_tree_path_get_depth ( path ) > 0 );

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );
    if ( G_LIKELY ( item != NULL ) )
        ptk_icon_view_select_item ( icon_view, item );
}



/**
 * ptk_icon_view_unselect_path:
 * @icon_view : A #PtkIconView.
 * @path      : The #GtkTreePath to be unselected.
 *
 * Unselects the row at @path.
 **/
void
ptk_icon_view_unselect_path ( PtkIconView *icon_view,
                              GtkTreePath *path )
{
    PtkIconViewItem * item;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( icon_view->priv->model != NULL );
    g_return_if_fail ( gtk_tree_path_get_depth ( path ) > 0 );

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );
    if ( G_LIKELY ( item != NULL ) )
        ptk_icon_view_unselect_item ( icon_view, item );
}



/**
 * ptk_icon_view_get_selected_items:
 * @icon_view: A #PtkIconView.
 *
 * Creates a list of paths of all selected items. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReference<!-- -->s.
 * To do this, you can use gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * <informalexample><programlisting>
 * g_list_foreach (list, gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: A #GList containing a #GtkTreePath for each selected row.
 **/
GList *
ptk_icon_view_get_selected_items ( const PtkIconView *icon_view )
{
    PtkIconViewItem * item;
    GList *selected = NULL;
    GList *lp;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), NULL );

    for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
    {
        item = lp->data;
        if ( item->selected )
            selected = g_list_prepend ( selected, gtk_tree_path_new_from_indices ( item->index, -1 ) );
    }

    return selected;
}



/**
 * ptk_icon_view_select_all:
 * @icon_view : A #PtkIconView.
 *
 * Selects all the icons. @icon_view must has its selection mode set
 * to #GTK_SELECTION_MULTIPLE.
 **/
void
ptk_icon_view_select_all ( PtkIconView *icon_view )
{
    GList * items;
    gboolean dirty = FALSE;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( icon_view->priv->selection_mode != GTK_SELECTION_MULTIPLE )
        return ;

    for ( items = icon_view->priv->items; items; items = items->next )
    {
        PtkIconViewItem * item = items->data;

        if ( !item->selected )
        {
            dirty = TRUE;
            item->selected = TRUE;
            ptk_icon_view_queue_draw_item ( icon_view, item );
        }
    }

    if ( dirty )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



/**
 * ptk_icon_view_unselect_all:
 * @icon_view : A #PtkIconView.
 *
 * Unselects all the icons.
 **/
void
ptk_icon_view_unselect_all ( PtkIconView *icon_view )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_UNLIKELY ( icon_view->priv->selection_mode == GTK_SELECTION_BROWSE ) )
        return ;

    if ( ptk_icon_view_unselect_all_internal ( icon_view ) )
        g_signal_emit ( icon_view, icon_view_signals[ SELECTION_CHANGED ], 0 );
}



/**
 * ptk_icon_view_path_is_selected:
 * @icon_view: A #PtkIconView.
 * @path: A #GtkTreePath to check selection on.
 *
 * Returns %TRUE if the icon pointed to by @path is currently
 * selected. If @icon does not point to a valid location, %FALSE is returned.
 *
 * Return value: %TRUE if @path is selected.
 **/
gboolean
ptk_icon_view_path_is_selected ( const PtkIconView *icon_view,
                                 GtkTreePath *path )
{
    PtkIconViewItem * item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );
    g_return_val_if_fail ( icon_view->priv->model != NULL, FALSE );
    g_return_val_if_fail ( gtk_tree_path_get_depth ( path ) > 0, FALSE );

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );

    return ( item != NULL && item->selected );
}



/**
 * ptk_icon_view_item_activated:
 * @icon_view : a #PtkIconView
 * @path      : the #GtkTreePath to be activated
 *
 * Activates the item determined by @path.
 **/
void
ptk_icon_view_item_activated ( PtkIconView *icon_view,
                               GtkTreePath *path )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( gtk_tree_path_get_depth ( path ) > 0 );

    g_signal_emit ( icon_view, icon_view_signals[ ITEM_ACTIVATED ], 0, path );
}



/**
 * ptk_icon_view_get_cursor:
 * @icon_view : A #PtkIconView
 * @path      : Return location for the current cursor path, or %NULL
 * @cell      : Return location the current focus cell, or %NULL
 *
 * Fills in @path and @cell with the current cursor path and cell.
 * If the cursor isn't currently set, then *@path will be %NULL.
 * If no cell currently has focus, then *@cell will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free().
 *
 * Return value: %TRUE if the cursor is set.
 *
 * Since: 0.3.1
 **/
gboolean
ptk_icon_view_get_cursor ( const PtkIconView *icon_view,
                           GtkTreePath **path,
                           GtkCellRenderer **cell )
{
    PtkIconViewCellInfo * info;
    PtkIconViewItem *item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    item = icon_view->priv->cursor_item;
    info = ( icon_view->priv->cursor_cell < 0 ) ? NULL : g_list_nth_data ( icon_view->priv->cell_list, icon_view->priv->cursor_cell );

    if ( G_LIKELY ( path != NULL ) )
        * path = ( item != NULL ) ? gtk_tree_path_new_from_indices ( item->index, -1 ) : NULL;

    if ( G_LIKELY ( cell != NULL ) )
        * cell = ( info != NULL ) ? info->cell : NULL;

    return ( item != NULL );
}



/**
 * ptk_icon_view_set_cursor:
 * @icon_view     : a #PtkIconView
 * @path          : a #GtkTreePath
 * @cell          : a #GtkCellRenderer or %NULL
 * @start_editing : %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular item.
 * If @cell is not %NULL, then focus is given to the cell specified by
 * it. Additionally, if @start_editing is %TRUE, then editing should be
 * started in the specified cell.
 *
 * This function is often followed by <literal>gtk_widget_grab_focus
 * (icon_view)</literal> in order to give keyboard focus to the widget.
 * Please note that editing can only happen when the widget is realized.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_set_cursor ( PtkIconView *icon_view,
                           GtkTreePath *path,
                           GtkCellRenderer *cell,
                           gboolean start_editing )
{
    PtkIconViewItem * item;
    PtkIconViewCellInfo *info = NULL;
    GList *l;
    gint i, cell_pos;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( path != NULL );
    g_return_if_fail ( cell == NULL || GTK_IS_CELL_RENDERER ( cell ) );

    ptk_icon_view_stop_editing ( icon_view, TRUE );

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );
    if ( G_UNLIKELY ( item == NULL ) )
        return ;

    cell_pos = -1;
    for ( l = icon_view->priv->cell_list, i = 0; l; l = l->next, i++ )
    {
        info = l->data;

        if ( info->cell == cell )
        {
            cell_pos = i;
            break;
        }

        info = NULL;
    }

    ptk_icon_view_set_cursor_item ( icon_view, item, cell_pos );
    ptk_icon_view_scroll_to_item ( icon_view, item );

    if ( start_editing )
        ptk_icon_view_start_editing ( icon_view, item, info, NULL );
}



/**
 * ptk_icon_view_scroll_to_path:
 * @icon_view: A #PtkIconView.
 * @path: The path of the item to move to.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the item specified by @path.
 * @col_align: The horizontal alignment of the item specified by @column.
 *
 * Moves the alignments of @icon_view to the position specified by @path.
 * @row_align determines where the row is placed, and @col_align determines where
 * @column is placed.  Both are expected to be between 0.0 and 1.0.
 * 0.0 means left/top alignment, 1.0 means right/bottom alignment, 0.5 means center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the item onto the screen.
 * This means that the item will be scrolled to the edge closest to its current
 * position.  If the item is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on the
 * model.  If the model changes before the @icon_view is realized, the centered
 * path will be modified to reflect this change.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_scroll_to_path ( PtkIconView *icon_view,
                               GtkTreePath *path,
                               gboolean use_align,
                               gfloat row_align,
                               gfloat col_align )
{
    PtkIconViewItem * item;

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( gtk_tree_path_get_depth ( path ) > 0 );
    g_return_if_fail ( row_align >= 0.0 && row_align <= 1.0 );
    g_return_if_fail ( col_align >= 0.0 && col_align <= 1.0 );

    item = g_list_nth_data ( icon_view->priv->items, gtk_tree_path_get_indices( path ) [ 0 ] );
    if ( G_UNLIKELY ( item == NULL ) )
        return ;

    if ( use_align )
    {
        gint x, y;
        gint focus_width;
        gfloat offset, value;

        gtk_widget_style_get ( GTK_WIDGET ( icon_view ),
                               "focus-line-width", &focus_width,
                               NULL );

        gdk_window_get_position ( icon_view->priv->bin_window, &x, &y );

        offset = y + item->area.y - focus_width -
                 row_align * ( GTK_WIDGET ( icon_view ) ->allocation.height - item->area.height );
        value = CLAMP ( icon_view->priv->vadjustment->value + offset,
                        icon_view->priv->vadjustment->lower,
                        icon_view->priv->vadjustment->upper - icon_view->priv->vadjustment->page_size );
        gtk_adjustment_set_value ( icon_view->priv->vadjustment, value );

        offset = x + item->area.x - focus_width -
                 col_align * ( GTK_WIDGET ( icon_view ) ->allocation.width - item->area.width );
        value = CLAMP ( icon_view->priv->hadjustment->value + offset,
                        icon_view->priv->hadjustment->lower,
                        icon_view->priv->hadjustment->upper - icon_view->priv->hadjustment->page_size );
        gtk_adjustment_set_value ( icon_view->priv->hadjustment, value );

        gtk_adjustment_changed ( icon_view->priv->hadjustment );
        gtk_adjustment_changed ( icon_view->priv->vadjustment );
    }
    else
    {
        ptk_icon_view_scroll_to_item ( icon_view, item );
    }
}



/**
 * ptk_icon_view_get_orientation:
 * @icon_view : a #PtkIconView
 *
 * Returns the value of the ::orientation property which determines
 * whether the labels are drawn beside the icons instead of below.
 *
 * Return value: the relative position of texts and icons
 *
 * Since: 0.3.1
 **/
GtkOrientation
ptk_icon_view_get_orientation ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), GTK_ORIENTATION_VERTICAL );
    return icon_view->priv->orientation;
}



/**
 * ptk_icon_view_set_orientation:
 * @icon_view   : a #PtkIconView
 * @orientation : the relative position of texts and icons
 *
 * Sets the ::orientation property which determines whether the labels
 * are drawn beside the icons instead of below.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_set_orientation ( PtkIconView *icon_view,
                                GtkOrientation orientation )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->orientation != orientation ) )
    {
        icon_view->priv->orientation = orientation;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        update_text_cell ( icon_view );
        update_pixbuf_cell ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "orientation" );
    }
}



/**
 * ptk_icon_view_get_columns:
 * @icon_view: a #PtkIconView
 *
 * Returns the value of the ::columns property.
 *
 * Return value: the number of columns, or -1
 */
gint
ptk_icon_view_get_columns ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->columns;
}



/**
 * ptk_icon_view_set_columns:
 * @icon_view : a #PtkIconView
 * @columns   : the number of columns
 *
 * Sets the ::columns property which determines in how
 * many columns the icons are arranged. If @columns is
 * -1, the number of columns will be chosen automatically
 * to fill the available area.
 *
 * Since: 0.3.1
 */
void
ptk_icon_view_set_columns ( PtkIconView *icon_view,
                            gint columns )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->columns != columns ) )
    {
        icon_view->priv->columns = columns;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_queue_layout ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "columns" );
    }
}



/**
 * ptk_icon_view_get_item_width:
 * @icon_view: a #PtkIconView
 *
 * Returns the value of the ::item-width property.
 *
 * Return value: the width of a single item, or -1
 *
 * Since: 0.3.1
 */
gint
ptk_icon_view_get_item_width ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->item_width;
}



/**
 * ptk_icon_view_set_item_width:
 * @icon_view  : a #PtkIconView
 * @item_width : the width for each item
 *
 * Sets the ::item-width property which specifies the width
 * to use for each item. If it is set to -1, the icon view will
 * automatically determine a suitable item size.
 *
 * Since: 0.3.1
 */
void
ptk_icon_view_set_item_width ( PtkIconView *icon_view,
                               gint item_width )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( icon_view->priv->item_width != item_width )
    {
        icon_view->priv->item_width = item_width;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        update_text_cell ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "item-width" );
    }
}



/**
 * ptk_icon_view_get_spacing:
 * @icon_view: a #PtkIconView
 *
 * Returns the value of the ::spacing property.
 *
 * Return value: the space between cells
 *
 * Since: 0.3.1
 */
gint
ptk_icon_view_get_spacing ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->spacing;
}



/**
 * ptk_icon_view_set_spacing:
 * @icon_view : a #PtkIconView
 * @spacing   : the spacing
 *
 * Sets the ::spacing property which specifies the space
 * which is inserted between the cells (i.e. the icon and
 * the text) of an item.
 *
 * Since: 0.3.1
 */
void
ptk_icon_view_set_spacing ( PtkIconView *icon_view,
                            gint spacing )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->spacing != spacing ) )
    {
        icon_view->priv->spacing = spacing;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "spacing" );
    }
}



/**
 * ptk_icon_view_get_row_spacing:
 * @icon_view: a #PtkIconView
 *
 * Returns the value of the ::row-spacing property.
 *
 * Return value: the space between rows
 *
 * Since: 0.3.1
 */
gint
ptk_icon_view_get_row_spacing ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->row_spacing;
}



/**
 * ptk_icon_view_set_row_spacing:
 * @icon_view   : a #PtkIconView
 * @row_spacing : the row spacing
 *
 * Sets the ::row-spacing property which specifies the space
 * which is inserted between the rows of the icon view.
 *
 * Since: 0.3.1
 */
void
ptk_icon_view_set_row_spacing ( PtkIconView *icon_view,
                                gint row_spacing )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->row_spacing != row_spacing ) )
    {
        icon_view->priv->row_spacing = row_spacing;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "row-spacing" );
    }
}



/**
 * ptk_icon_view_get_column_spacing:
 * @icon_view: a #PtkIconView
 *
 * Returns the value of the ::column-spacing property.
 *
 * Return value: the space between columns
 *
 * Since: 0.3.1
 **/
gint
ptk_icon_view_get_column_spacing ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->column_spacing;
}



/**
 * ptk_icon_view_set_column_spacing:
 * @icon_view      : a #PtkIconView
 * @column_spacing : the column spacing
 *
 * Sets the ::column-spacing property which specifies the space
 * which is inserted between the columns of the icon view.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_set_column_spacing ( PtkIconView *icon_view,
                                   gint column_spacing )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->column_spacing != column_spacing ) )
    {
        icon_view->priv->column_spacing = column_spacing;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "column-spacing" );
    }
}



/**
 * ptk_icon_view_get_margin:
 * @icon_view : a #PtkIconView
 *
 * Returns the value of the ::margin property.
 *
 * Return value: the space at the borders
 *
 * Since: 0.3.1
 **/
gint
ptk_icon_view_get_margin ( const PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );
    return icon_view->priv->margin;
}



/**
 * ptk_icon_view_set_margin:
 * @icon_view : a #PtkIconView
 * @margin    : the margin
 *
 * Sets the ::margin property which specifies the space
 * which is inserted at the top, bottom, left and right
 * of the icon view.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_set_margin ( PtkIconView *icon_view,
                           gint margin )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( G_LIKELY ( icon_view->priv->margin != margin ) )
    {
        icon_view->priv->margin = margin;

        ptk_icon_view_stop_editing ( icon_view, TRUE );
        ptk_icon_view_invalidate_sizes ( icon_view );

        g_object_notify ( G_OBJECT ( icon_view ), "margin" );
    }
}



/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn't result from a drop.
 */
static void
set_status_pending ( GdkDragContext *context,
                     GdkDragAction suggested_action )
{
    g_object_set_data ( G_OBJECT ( context ),
                        I_( "ptk-icon-view-status-pending" ),
                        GINT_TO_POINTER ( suggested_action ) );
}

static GdkDragAction
get_status_pending ( GdkDragContext *context )
{
    return GPOINTER_TO_INT ( g_object_get_data ( G_OBJECT ( context ), I_( "ptk-icon-view-status-pending" ) ) );
}

static void
unset_reorderable ( PtkIconView *icon_view )
{
    if ( icon_view->priv->reorderable )
    {
        icon_view->priv->reorderable = FALSE;
        g_object_notify ( G_OBJECT ( icon_view ), "reorderable" );
    }
}

static void
clear_source_info ( PtkIconView *icon_view )
{
    if ( icon_view->priv->source_targets )
        gtk_target_list_unref ( icon_view->priv->source_targets );
    icon_view->priv->source_targets = NULL;

    icon_view->priv->source_set = FALSE;
}

static void
clear_dest_info ( PtkIconView *icon_view )
{
    if ( icon_view->priv->dest_targets )
        gtk_target_list_unref ( icon_view->priv->dest_targets );
    icon_view->priv->dest_targets = NULL;

    icon_view->priv->dest_set = FALSE;
}

static void
set_source_row ( GdkDragContext *context,
                 GtkTreeModel *model,
                 GtkTreePath *source_row )
{
    if ( source_row )
        g_object_set_data_full ( G_OBJECT ( context ),
                                 I_( "ptk-icon-view-source-row" ),
                                 gtk_tree_row_reference_new ( model, source_row ),
                                 ( GDestroyNotify ) gtk_tree_row_reference_free );
    else
        g_object_set_data_full ( G_OBJECT ( context ),
                                 I_( "ptk-icon-view-source-row" ),
                                 NULL, NULL );
}

static GtkTreePath*
get_source_row ( GdkDragContext *context )
{
    GtkTreeRowReference * ref;

    ref = g_object_get_data ( G_OBJECT ( context ), I_( "ptk-icon-view-source-row" ) );

    if ( ref )
        return gtk_tree_row_reference_get_path ( ref );
    else
        return NULL;
}

typedef struct
{
    GtkTreeRowReference *dest_row;
    gboolean empty_view_drop;
    gboolean drop_append_mode;
}
DestRow;

static void
dest_row_free ( gpointer data )
{
    DestRow * dr = ( DestRow * ) data;

    gtk_tree_row_reference_free ( dr->dest_row );
    g_free ( dr );
}

static void
set_dest_row ( GdkDragContext *context,
               GtkTreeModel *model,
               GtkTreePath *dest_row,
               gboolean empty_view_drop,
               gboolean drop_append_mode )
{
    DestRow * dr;

    if ( !dest_row )
    {
        g_object_set_data_full ( G_OBJECT ( context ),
                                 I_( "ptk-icon-view-dest-row" ),
                                 NULL, NULL );
        return ;
    }

    dr = g_new0 ( DestRow, 1 );

    dr->dest_row = gtk_tree_row_reference_new ( model, dest_row );
    dr->empty_view_drop = empty_view_drop;
    dr->drop_append_mode = drop_append_mode;
    g_object_set_data_full ( G_OBJECT ( context ),
                             I_( "ptk-icon-view-dest-row" ),
                             dr, ( GDestroyNotify ) dest_row_free );
}



static GtkTreePath*
get_dest_row ( GdkDragContext *context )
{
    DestRow * dr;

    dr = g_object_get_data ( G_OBJECT ( context ), I_( "ptk-icon-view-dest-row" ) );

    if ( dr )
    {
        GtkTreePath * path = NULL;

        if ( dr->dest_row )
            path = gtk_tree_row_reference_get_path ( dr->dest_row );
        else if ( dr->empty_view_drop )
            path = gtk_tree_path_new_from_indices ( 0, -1 );
        else
            path = NULL;

        if ( path && dr->drop_append_mode )
            gtk_tree_path_next ( path );

        return path;
    }
    else
        return NULL;
}



static gboolean
check_model_dnd ( GtkTreeModel *model,
                  GType required_iface,
                  const gchar *signal )
{
    if ( model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ( ( model ), required_iface ) )
    {
        g_warning ( "You must override the default '%s' handler "
                    "on PtkIconView when using models that don't support "
                    "the %s interface and enabling drag-and-drop. The simplest way to do this "
                    "is to connect to '%s' and call "
                    "g_signal_stop_emission_by_name() in your signal handler to prevent "
                    "the default handler from running. Look at the source code "
                    "for the default handler in gtkiconview.c to get an idea what "
                    "your handler should do. (gtkiconview.c is in the GTK+ source "
                    "code.) If you're using GTK+ from a language other than C, "
                    "there may be a more natural way to override default handlers, e.g. via derivation.",
                    signal, g_type_name ( required_iface ), signal );
        return FALSE;
    }
    else
        return TRUE;
}



static void
remove_scroll_timeout ( PtkIconView *icon_view )
{
    if ( icon_view->priv->scroll_timeout_id != 0 )
    {
        g_source_remove ( icon_view->priv->scroll_timeout_id );

        icon_view->priv->scroll_timeout_id = 0;
    }
}



static void
ptk_icon_view_autoscroll ( PtkIconView *icon_view )
{
    gint px, py, x, y, width, height;
    gint hoffset, voffset;
    gfloat value;

    gdk_window_get_pointer ( GTK_WIDGET ( icon_view ) ->window, &px, &py, NULL );
    gdk_window_get_geometry ( GTK_WIDGET ( icon_view ) ->window, &x, &y, &width, &height, NULL );

    /* see if we are near the edge. */
    voffset = py - ( y + 2 * SCROLL_EDGE_SIZE );
    if ( voffset > 0 )
        voffset = MAX ( py - ( y + height - 2 * SCROLL_EDGE_SIZE ), 0 );

    hoffset = px - ( x + 2 * SCROLL_EDGE_SIZE );
    if ( hoffset > 0 )
        hoffset = MAX ( px - ( x + width - 2 * SCROLL_EDGE_SIZE ), 0 );

    if ( voffset != 0 )
    {
        value = CLAMP ( icon_view->priv->vadjustment->value + voffset,
                        icon_view->priv->vadjustment->lower,
                        icon_view->priv->vadjustment->upper - icon_view->priv->vadjustment->page_size );
        gtk_adjustment_set_value ( icon_view->priv->vadjustment, value );
    }
    if ( hoffset != 0 )
    {
        value = CLAMP ( icon_view->priv->hadjustment->value + hoffset,
                        icon_view->priv->hadjustment->lower,
                        icon_view->priv->hadjustment->upper - icon_view->priv->hadjustment->page_size );
        gtk_adjustment_set_value ( icon_view->priv->hadjustment, value );
    }
}


static gboolean
drag_scroll_timeout ( gpointer data )
{
    PtkIconView * icon_view = PTK_ICON_VIEW ( data );

    GDK_THREADS_ENTER ();

    ptk_icon_view_autoscroll ( icon_view );

    GDK_THREADS_LEAVE ();

    return TRUE;
}


static gboolean
set_destination ( PtkIconView *icon_view,
                  GdkDragContext *context,
                  gint x,
                  gint y,
                  GdkDragAction *suggested_action,
                  GdkAtom *target )
{
    GtkWidget * widget;
    GtkTreePath *path = NULL;
    PtkIconViewDropPosition pos;
    PtkIconViewDropPosition old_pos;
    GtkTreePath *old_dest_path = NULL;
    gboolean can_drop = FALSE;

    widget = GTK_WIDGET ( icon_view );

    *suggested_action = 0;
    *target = GDK_NONE;

    if ( !icon_view->priv->dest_set )
    {
        /* someone unset us as a drag dest, note that if
         * we return FALSE drag_leave isn't called
         */

        ptk_icon_view_set_drag_dest_item ( icon_view,
                                           NULL,
                                           PTK_ICON_VIEW_DROP_LEFT );

        remove_scroll_timeout ( PTK_ICON_VIEW ( widget ) );

        return FALSE; /* no longer a drop site */
    }

    *target = gtk_drag_dest_find_target ( widget, context, icon_view->priv->dest_targets );
    if ( *target == GDK_NONE )
        return FALSE;

    if ( !ptk_icon_view_get_dest_item_at_pos ( icon_view, x, y, &path, &pos ) )
    {
        gint n_children;
        GtkTreeModel *model;

        /* the row got dropped on empty space, let's setup a special case
         */

        if ( path )
            gtk_tree_path_free ( path );

        model = ptk_icon_view_get_model ( icon_view );

        n_children = gtk_tree_model_iter_n_children ( model, NULL );
        if ( n_children )
        {
            pos = PTK_ICON_VIEW_DROP_BELOW;
            path = gtk_tree_path_new_from_indices ( n_children - 1, -1 );
        }
        else
        {
            pos = PTK_ICON_VIEW_DROP_ABOVE;
            path = gtk_tree_path_new_from_indices ( 0, -1 );
        }

        can_drop = TRUE;

        goto out;
    }

    g_assert ( path );

    ptk_icon_view_get_drag_dest_item ( icon_view,
                                       &old_dest_path,
                                       &old_pos );

    if ( old_dest_path )
        gtk_tree_path_free ( old_dest_path );

    if ( TRUE /* FIXME if the location droppable predicate */ )
    {
        can_drop = TRUE;
    }

out:
    if ( can_drop )
    {
        GtkWidget * source_widget;

        *suggested_action = context->suggested_action;
        source_widget = gtk_drag_get_source_widget ( context );

        if ( source_widget == widget )
        {
            /* Default to MOVE, unless the user has
             * pressed ctrl or shift to affect available actions
             */
            if ( ( context->actions & GDK_ACTION_MOVE ) != 0 )
                * suggested_action = GDK_ACTION_MOVE;
        }

        ptk_icon_view_set_drag_dest_item ( PTK_ICON_VIEW ( widget ),
                                           path, pos );
    }
    else
    {
        /* can't drop here */
        ptk_icon_view_set_drag_dest_item ( PTK_ICON_VIEW ( widget ),
                                           NULL,
                                           PTK_ICON_VIEW_DROP_LEFT );
    }

    if ( path )
        gtk_tree_path_free ( path );

    return TRUE;
}

static GtkTreePath*
get_logical_destination ( PtkIconView *icon_view,
                          gboolean *drop_append_mode )
{
    /* adjust path to point to the row the drop goes in front of */
    GtkTreePath * path = NULL;
    PtkIconViewDropPosition pos;

    *drop_append_mode = FALSE;

    ptk_icon_view_get_drag_dest_item ( icon_view, &path, &pos );

    if ( path == NULL )
        return NULL;

    if ( pos == PTK_ICON_VIEW_DROP_RIGHT ||
            pos == PTK_ICON_VIEW_DROP_BELOW )
    {
        GtkTreeIter iter;
        GtkTreeModel *model = icon_view->priv->model;

        if ( !gtk_tree_model_get_iter ( model, &iter, path ) ||
                !gtk_tree_model_iter_next ( model, &iter ) )
            * drop_append_mode = TRUE;
        else
        {
            *drop_append_mode = FALSE;
            gtk_tree_path_next ( path );
        }
    }

    return path;
}

static gboolean
ptk_icon_view_maybe_begin_drag ( PtkIconView *icon_view,
                                 GdkEventMotion *event )
{
    GdkDragContext * context;
    GtkTreePath *path = NULL;
    gint button;
    GtkTreeModel *model;
    gboolean retval = FALSE;

    if ( !icon_view->priv->source_set )
        goto out;

    if ( icon_view->priv->pressed_button < 0 )
        goto out;

    if ( !gtk_drag_check_threshold ( GTK_WIDGET ( icon_view ),
                                     icon_view->priv->press_start_x,
                                     icon_view->priv->press_start_y,
                                     event->x, event->y ) )
        goto out;

    model = ptk_icon_view_get_model ( icon_view );

    if ( model == NULL )
        goto out;

    button = icon_view->priv->pressed_button;
    icon_view->priv->pressed_button = -1;

    path = ptk_icon_view_get_path_at_pos ( icon_view,
                                           icon_view->priv->press_start_x,
                                           icon_view->priv->press_start_y );

    if ( path == NULL )
        goto out;

    if ( !GTK_IS_TREE_DRAG_SOURCE ( model ) ||
            !gtk_tree_drag_source_row_draggable ( GTK_TREE_DRAG_SOURCE ( model ),
                                                  path ) )
        goto out;

    /* FIXME Check whether we're a start button, if not return FALSE and
     * free path
     */

    /* Now we can begin the drag */

    retval = TRUE;

    context = gtk_drag_begin ( GTK_WIDGET ( icon_view ),
                               icon_view->priv->source_targets,
                               icon_view->priv->source_actions,
                               button,
                               ( GdkEvent* ) event );

    set_source_row ( context, model, path );

out:
    if ( path )
        gtk_tree_path_free ( path );

    return retval;
}

/* Source side drag signals */
static void
ptk_icon_view_drag_begin ( GtkWidget *widget,
                           GdkDragContext *context )
{
    PtkIconView * icon_view;
    PtkIconViewItem *item;
    GdkPixmap *icon;
    gint x, y;
    GtkTreePath *path;

    icon_view = PTK_ICON_VIEW ( widget );

    /* if the user uses a custom DnD impl, we don't set the icon here */
    if ( !icon_view->priv->dest_set && !icon_view->priv->source_set )
        return ;

    item = ptk_icon_view_get_item_at_coords ( icon_view,
                                              icon_view->priv->press_start_x,
                                              icon_view->priv->press_start_y,
                                              TRUE,
                                              NULL );

    g_return_if_fail ( item != NULL );

    x = icon_view->priv->press_start_x - item->area.x + 1;
    y = icon_view->priv->press_start_y - item->area.y + 1;

    path = gtk_tree_path_new_from_indices ( item->index, -1 );
    icon = ptk_icon_view_create_drag_icon ( icon_view, path );
    gtk_tree_path_free ( path );

    gtk_drag_set_icon_pixmap ( context,
                               gdk_drawable_get_colormap ( icon ),
                               icon,
                               NULL,
                               x, y );

    g_object_unref ( icon );
}

static void
ptk_icon_view_drag_end ( GtkWidget *widget,
                         GdkDragContext *context )
{
    /* do nothing */
}

static void
ptk_icon_view_drag_data_get ( GtkWidget *widget,
                              GdkDragContext *context,
                              GtkSelectionData *selection_data,
                              guint info,
                              guint time )
{
    PtkIconView * icon_view;
    GtkTreeModel *model;
    GtkTreePath *source_row;

    icon_view = PTK_ICON_VIEW ( widget );
    model = ptk_icon_view_get_model ( icon_view );

    if ( model == NULL )
        return ;

    if ( !icon_view->priv->dest_set )
        return ;

    source_row = get_source_row ( context );

    if ( source_row == NULL )
        return ;

    /* We can implement the GTK_TREE_MODEL_ROW target generically for
     * any model; for DragSource models there are some other targets
     * we also support.
     */

    if ( GTK_IS_TREE_DRAG_SOURCE ( model ) &&
            gtk_tree_drag_source_drag_data_get ( GTK_TREE_DRAG_SOURCE ( model ),
                                                 source_row,
                                                 selection_data ) )
        goto done;

    /* If drag_data_get does nothing, try providing row data. */
    if ( selection_data->target == gdk_atom_intern ( "GTK_TREE_MODEL_ROW", FALSE ) )
        gtk_tree_set_row_drag_data ( selection_data,
                                     model,
                                     source_row );

done:
    gtk_tree_path_free ( source_row );
}

static void
ptk_icon_view_drag_data_delete ( GtkWidget *widget,
                                 GdkDragContext *context )
{
    GtkTreeModel * model;
    PtkIconView *icon_view;
    GtkTreePath *source_row;

    icon_view = PTK_ICON_VIEW ( widget );
    model = ptk_icon_view_get_model ( icon_view );

    if ( !check_model_dnd ( model, GTK_TYPE_TREE_DRAG_SOURCE, "drag_data_delete" ) )
        return ;

    if ( !icon_view->priv->dest_set )
        return ;

    source_row = get_source_row ( context );

    if ( source_row == NULL )
        return ;

    gtk_tree_drag_source_drag_data_delete ( GTK_TREE_DRAG_SOURCE ( model ),
                                            source_row );

    gtk_tree_path_free ( source_row );

    set_source_row ( context, NULL, NULL );
}

/* Target side drag signals */
static void
ptk_icon_view_drag_leave ( GtkWidget *widget,
                           GdkDragContext *context,
                           guint time )
{
    PtkIconView * icon_view;

    icon_view = PTK_ICON_VIEW ( widget );

    /* unset any highlight row */
    ptk_icon_view_set_drag_dest_item ( icon_view,
                                       NULL,
                                       PTK_ICON_VIEW_DROP_LEFT );

    remove_scroll_timeout ( icon_view );
}

static gboolean
ptk_icon_view_drag_motion ( GtkWidget *widget,
                            GdkDragContext *context,
                            gint x,
                            gint y,
                            guint time )
{
    PtkIconViewDropPosition pos;
    GdkDragAction suggested_action = 0;
    GtkTreePath *path = NULL;
    PtkIconView *icon_view = PTK_ICON_VIEW ( widget );
    gboolean empty;
    GdkAtom target;

    if ( !set_destination ( icon_view, context, x, y, &suggested_action, &target ) )
        return FALSE;

    ptk_icon_view_get_drag_dest_item ( icon_view, &path, &pos );

    /* we only know this *after* set_desination_row */
    empty = icon_view->priv->empty_view_drop;

    if ( path == NULL && !empty )
    {
        /* Can't drop here. */
        gdk_drag_status ( context, 0, time );
    }
    else
    {
        if ( icon_view->priv->scroll_timeout_id == 0 )
            icon_view->priv->scroll_timeout_id = g_timeout_add ( 50, drag_scroll_timeout, icon_view );

        if ( target == gdk_atom_intern ( "GTK_TREE_MODEL_ROW", FALSE ) )
        {
            /* Request data so we can use the source row when
             * determining whether to accept the drop
             */
            set_status_pending ( context, suggested_action );
            gtk_drag_get_data ( widget, context, target, time );
        }
        else
        {
            set_status_pending ( context, 0 );
            gdk_drag_status ( context, suggested_action, time );
        }
    }

    if ( path != NULL )
        gtk_tree_path_free ( path );

    return TRUE;
}

static gboolean
ptk_icon_view_drag_drop ( GtkWidget *widget,
                          GdkDragContext *context,
                          gint x,
                          gint y,
                          guint time )
{
    PtkIconView * icon_view;
    GtkTreePath *path;
    GdkDragAction suggested_action = 0;
    GdkAtom target = GDK_NONE;
    GtkTreeModel *model;
    gboolean drop_append_mode;

    icon_view = PTK_ICON_VIEW ( widget );
    model = ptk_icon_view_get_model ( icon_view );

    remove_scroll_timeout ( PTK_ICON_VIEW ( widget ) );

    if ( !icon_view->priv->dest_set )
        return FALSE;

    if ( !check_model_dnd ( model, GTK_TYPE_TREE_DRAG_DEST, "drag_drop" ) )
        return FALSE;

    if ( !set_destination ( icon_view, context, x, y, &suggested_action, &target ) )
        return FALSE;

    path = get_logical_destination ( icon_view, &drop_append_mode );

    if ( target != GDK_NONE && path != NULL )
    {
        /* in case a motion had requested drag data, change things so we
         * treat drag data receives as a drop.
         */
        set_status_pending ( context, 0 );
        set_dest_row ( context, model, path,
                       icon_view->priv->empty_view_drop, drop_append_mode );
    }

    if ( path )
        gtk_tree_path_free ( path );

    /* Unset this thing */
    ptk_icon_view_set_drag_dest_item ( icon_view, NULL, PTK_ICON_VIEW_DROP_LEFT );

    if ( target != GDK_NONE )
    {
        gtk_drag_get_data ( widget, context, target, time );
        return TRUE;
    }
    else
        return FALSE;
}

static void
ptk_icon_view_drag_data_received ( GtkWidget *widget,
                                   GdkDragContext *context,
                                   gint x,
                                   gint y,
                                   GtkSelectionData *selection_data,
                                   guint info,
                                   guint time )
{
    GtkTreePath * path;
    gboolean accepted = FALSE;
    GtkTreeModel *model;
    PtkIconView *icon_view;
    GtkTreePath *dest_row;
    GdkDragAction suggested_action;
    gboolean drop_append_mode;

    icon_view = PTK_ICON_VIEW ( widget );
    model = ptk_icon_view_get_model ( icon_view );

    if ( !check_model_dnd ( model, GTK_TYPE_TREE_DRAG_DEST, "drag_data_received" ) )
        return ;

    if ( !icon_view->priv->dest_set )
        return ;

    suggested_action = get_status_pending ( context );

    if ( suggested_action )
    {
        /* We are getting this data due to a request in drag_motion,
         * rather than due to a request in drag_drop, so we are just
         * supposed to call drag_status, not actually paste in the
         * data.
         */
        path = get_logical_destination ( icon_view, &drop_append_mode );

        if ( path == NULL )
            suggested_action = 0;

        if ( suggested_action )
        {
            if ( !gtk_tree_drag_dest_row_drop_possible ( GTK_TREE_DRAG_DEST ( model ),
                                                         path,
                                                         selection_data ) )
                suggested_action = 0;
        }

        gdk_drag_status ( context, suggested_action, time );

        if ( path )
            gtk_tree_path_free ( path );

        /* If you can't drop, remove user drop indicator until the next motion */
        if ( suggested_action == 0 )
            ptk_icon_view_set_drag_dest_item ( icon_view,
                                               NULL,
                                               PTK_ICON_VIEW_DROP_LEFT );
        return ;
    }


    dest_row = get_dest_row ( context );

    if ( dest_row == NULL )
        return ;

    if ( selection_data->length >= 0 )
    {
        if ( gtk_tree_drag_dest_drag_data_received ( GTK_TREE_DRAG_DEST ( model ),
                                                     dest_row,
                                                     selection_data ) )
            accepted = TRUE;
    }

    gtk_drag_finish ( context,
                      accepted,
                      ( context->action == GDK_ACTION_MOVE ),
                      time );

    gtk_tree_path_free ( dest_row );

    /* drop dest_row */
    set_dest_row ( context, NULL, NULL, FALSE, FALSE );
}



/**
 * ptk_icon_view_enable_model_drag_source:
 * @icon_view         : a #GtkIconTreeView
 * @start_button_mask : Mask of allowed buttons to start drag
 * @targets           : the table of targets that the drag will support
 * @n_targets         : the number of items in @targets
 * @actions           : the bitmask of possible actions for a drag from this widget
 *
 * Turns @icon_view into a drag source for automatic DND.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_enable_model_drag_source ( PtkIconView *icon_view,
                                         GdkModifierType start_button_mask,
                                         const GtkTargetEntry *targets,
                                         gint n_targets,
                                         GdkDragAction actions )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    gtk_drag_source_set ( GTK_WIDGET ( icon_view ), 0, NULL, 0, actions );

    clear_source_info ( icon_view );
    icon_view->priv->start_button_mask = start_button_mask;
    icon_view->priv->source_targets = gtk_target_list_new ( targets, n_targets );
    icon_view->priv->source_actions = actions;

    icon_view->priv->source_set = TRUE;

    unset_reorderable ( icon_view );
}



/**
 * ptk_icon_view_enable_model_drag_dest:
 * @icon_view : a #PtkIconView
 * @targets   : the table of targets that the drag will support
 * @n_targets : the number of items in @targets
 * @actions   : the bitmask of possible actions for a drag from this widget
 *
 * Turns @icon_view into a drop destination for automatic DND.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_enable_model_drag_dest ( PtkIconView *icon_view,
                                       const GtkTargetEntry *targets,
                                       gint n_targets,
                                       GdkDragAction actions )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    gtk_drag_dest_set ( GTK_WIDGET ( icon_view ), 0, NULL, 0, actions );

    clear_dest_info ( icon_view );

    icon_view->priv->dest_targets = gtk_target_list_new ( targets, n_targets );
    icon_view->priv->dest_actions = actions;

    icon_view->priv->dest_set = TRUE;

    unset_reorderable ( icon_view );
}



/**
 * ptk_icon_view_unset_model_drag_source:
 * @icon_view : a #PtkIconView
 *
 * Undoes the effect of #ptk_icon_view_enable_model_drag_source().
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_unset_model_drag_source ( PtkIconView *icon_view )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( icon_view->priv->source_set )
    {
        gtk_drag_source_unset ( GTK_WIDGET ( icon_view ) );
        clear_source_info ( icon_view );
    }

    unset_reorderable ( icon_view );
}



/**
 * ptk_icon_view_unset_model_drag_dest:
 * @icon_view : a #PtkIconView
 *
 * Undoes the effect of #ptk_icon_view_enable_model_drag_dest().
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_unset_model_drag_dest ( PtkIconView *icon_view )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( icon_view->priv->dest_set )
    {
        gtk_drag_dest_unset ( GTK_WIDGET ( icon_view ) );
        clear_dest_info ( icon_view );
    }

    unset_reorderable ( icon_view );
}



/**
 * ptk_icon_view_set_drag_dest_item:
 * @icon_view : a #PtkIconView
 * @path      : The path of the item to highlight, or %NULL.
 * @pos       : Specifies whether to drop, relative to the item
 *
 * Sets the item that is highlighted for feedback.
 *
 * Since: 0.3.1
 */
void
ptk_icon_view_set_drag_dest_item ( PtkIconView *icon_view,
                                   GtkTreePath *path,
                                   PtkIconViewDropPosition pos )
{
    /* Note; this function is exported to allow a custom DND
     * implementation, so it can't touch TreeViewDragInfo
     */

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( icon_view->priv->dest_item )
    {
        GtkTreePath * current_path;
        current_path = gtk_tree_row_reference_get_path ( icon_view->priv->dest_item );
        gtk_tree_row_reference_free ( icon_view->priv->dest_item );
        icon_view->priv->dest_item = NULL;

        ptk_icon_view_queue_draw_path ( icon_view, current_path );
        gtk_tree_path_free ( current_path );
    }

    /* special case a drop on an empty model */
    icon_view->priv->empty_view_drop = FALSE;
    if ( pos == GTK_TREE_VIEW_DROP_BEFORE && path
            && gtk_tree_path_get_depth ( path ) == 1
            && gtk_tree_path_get_indices ( path ) [ 0 ] == 0 )
    {
        gint n_children;

        n_children = gtk_tree_model_iter_n_children ( icon_view->priv->model,
                                                      NULL );

        if ( n_children == 0 )
            icon_view->priv->empty_view_drop = TRUE;
    }

    icon_view->priv->dest_pos = pos;

    if ( path )
    {
        icon_view->priv->dest_item =
            gtk_tree_row_reference_new_proxy ( G_OBJECT ( icon_view ),
                                               icon_view->priv->model, path );

        ptk_icon_view_queue_draw_path ( icon_view, path );
    }
}



/**
 * ptk_icon_view_get_drag_dest_item:
 * @icon_view : a #PtkIconView
 * @path      : Return location for the path of the highlighted item, or %NULL.
 * @pos       : Return location for the drop position, or %NULL
 *
 * Gets information about the item that is highlighted for feedback.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_get_drag_dest_item ( PtkIconView *icon_view,
                                   GtkTreePath **path,
                                   PtkIconViewDropPosition *pos )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    if ( path )
    {
        if ( icon_view->priv->dest_item )
            * path = gtk_tree_row_reference_get_path ( icon_view->priv->dest_item );
        else
            *path = NULL;
    }

    if ( pos )
        * pos = icon_view->priv->dest_pos;
}



/**
 * ptk_icon_view_get_dest_item_at_pos:
 * @icon_view : a #PtkIconView
 * @drag_x    : the position to determine the destination item for
 * @drag_y    : the position to determine the destination item for
 * @path      : Return location for the path of the highlighted item, or %NULL.
 * @pos       : Return location for the drop position, or %NULL
 *
 * Determines the destination item for a given position.
 *
 * Both @drag_x and @drag_y are given in icon window coordinates. Use
 * #ptk_icon_view_widget_to_icon_coords() if you need to translate
 * widget coordinates first.
 *
 * Return value: whether there is an item at the given position.
 *
 * Since: 0.3.1
 **/
gboolean
ptk_icon_view_get_dest_item_at_pos ( PtkIconView *icon_view,
                                     gint drag_x,
                                     gint drag_y,
                                     GtkTreePath **path,
                                     PtkIconViewDropPosition *pos )
{
    PtkIconViewItem * item;

    /* Note; this function is exported to allow a custom DND
     * implementation, so it can't touch TreeViewDragInfo
     */

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );
    g_return_val_if_fail ( drag_x >= 0, FALSE );
    g_return_val_if_fail ( drag_y >= 0, FALSE );
    g_return_val_if_fail ( icon_view->priv->bin_window != NULL, FALSE );

    if ( G_LIKELY ( path != NULL ) )
        * path = NULL;

    item = ptk_icon_view_get_item_at_coords ( icon_view, drag_x, drag_y, FALSE, NULL );

    if ( G_UNLIKELY ( item == NULL ) )
        return FALSE;

    if ( G_LIKELY ( path != NULL ) )
        * path = gtk_tree_path_new_from_indices ( item->index, -1 );

    if ( G_LIKELY ( pos != NULL ) )
    {
        if ( drag_x < item->area.x + item->area.width / 4 )
            * pos = PTK_ICON_VIEW_DROP_LEFT;
        else if ( drag_x > item->area.x + item->area.width * 3 / 4 )
            * pos = PTK_ICON_VIEW_DROP_RIGHT;
        else if ( drag_y < item->area.y + item->area.height / 4 )
            * pos = PTK_ICON_VIEW_DROP_ABOVE;
        else if ( drag_y > item->area.y + item->area.height * 3 / 4 )
            * pos = PTK_ICON_VIEW_DROP_BELOW;
        else
            *pos = PTK_ICON_VIEW_DROP_INTO;
    }

    return TRUE;
}



/**
 * ptk_icon_view_create_drag_icon:
 * @icon_view : a #PtkIconView
 * @path      : a #GtkTreePath in @icon_view
 *
 * Creates a #GdkPixmap representation of the item at @path.
 * This image is used for a drag icon.
 *
 * Return value: a newly-allocated pixmap of the drag icon.
 *
 * Since: 0.3.1
 **/
GdkPixmap*
ptk_icon_view_create_drag_icon ( PtkIconView *icon_view,
                                 GtkTreePath *path )
{
    GdkRectangle area;
    GtkWidget *widget = GTK_WIDGET ( icon_view );
    GdkPixmap *drawable;
    GdkGC *gc;
    GList *lp;
    gint index;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), NULL );
    g_return_val_if_fail ( gtk_tree_path_get_depth ( path ) > 0, NULL );

    /* verify that the widget is realized */
    if ( G_UNLIKELY ( !GTK_WIDGET_REALIZED ( icon_view ) ) )
        return NULL;

    index = gtk_tree_path_get_indices ( path ) [ 0 ];

    for ( lp = icon_view->priv->items; lp != NULL; lp = lp->next )
    {
        PtkIconViewItem * item = lp->data;
        if ( G_UNLIKELY ( index == item->index ) )
        {
            drawable = gdk_pixmap_new ( icon_view->priv->bin_window,
                                        item->area.width + 2,
                                        item->area.height + 2,
                                        -1 );

            gc = gdk_gc_new ( drawable );
            gdk_gc_set_rgb_fg_color ( gc, &widget->style->base[ GTK_WIDGET_STATE ( widget ) ] );
            gdk_draw_rectangle ( drawable, gc, TRUE, 0, 0, item->area.width + 2, item->area.height + 2 );

            area.x = 0;
            area.y = 0;
            area.width = item->area.width;
            area.height = item->area.height;

            ptk_icon_view_paint_item ( icon_view, item, &area, drawable, 1, 1, FALSE );

            gdk_gc_set_rgb_fg_color ( gc, &widget->style->black );
            gdk_draw_rectangle ( drawable, gc, FALSE, 1, 1, item->area.width + 1, item->area.height + 1 );

            g_object_unref ( G_OBJECT ( gc ) );

            return drawable;
        }
    }

    return NULL;
}



/**
 * ptk_icon_view_get_reorderable:
 * @icon_view : a #PtkIconView
 *
 * Retrieves whether the user can reorder the list via drag-and-drop.
 * See ptk_icon_view_set_reorderable().
 *
 * Return value: %TRUE if the list can be reordered.
 *
 * Since: 0.3.1
 **/
gboolean
ptk_icon_view_get_reorderable ( PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    return icon_view->priv->reorderable;
}



/**
 * ptk_icon_view_set_reorderable:
 * @icon_view   : A #PtkIconView.
 * @reorderable : %TRUE, if the list of items can be reordered.
 *
 * This function is a convenience function to allow you to reorder models that
 * support the #GtkTreeDragSourceIface and the #GtkTreeDragDestIface.  Both
 * #GtkTreeStore and #GtkListStore support these.  If @reorderable is %TRUE, then
 * the user can reorder the model by dragging and dropping rows.  The
 * developer can listen to these changes by connecting to the model's
 * ::row-inserted and ::row-deleted signals.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 *
 * Since: 0.3.1
 **/
void
ptk_icon_view_set_reorderable ( PtkIconView *icon_view,
                                gboolean reorderable )
{
    static const GtkTargetEntry item_targets[] =
        {
            { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0,
            },
        };

    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    reorderable = ( reorderable != FALSE );

    if ( G_UNLIKELY ( icon_view->priv->reorderable == reorderable ) )
        return ;

    if ( G_LIKELY ( reorderable ) )
    {
        ptk_icon_view_enable_model_drag_source ( icon_view, GDK_BUTTON1_MASK, item_targets, G_N_ELEMENTS ( item_targets ), GDK_ACTION_MOVE );
        ptk_icon_view_enable_model_drag_dest ( icon_view, item_targets, G_N_ELEMENTS ( item_targets ), GDK_ACTION_MOVE );
    }
    else
    {
        ptk_icon_view_unset_model_drag_source ( icon_view );
        ptk_icon_view_unset_model_drag_dest ( icon_view );
    }

    icon_view->priv->reorderable = reorderable;

    g_object_notify ( G_OBJECT ( icon_view ), "reorderable" );
}








/**
 ** Accessibility Support
 **/

static gpointer accessible_parent_class;
static gpointer accessible_item_parent_class;
static GQuark accessible_private_data_quark = 0;

#define PTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE      (ptk_icon_view_item_accessible_get_type ())
#define PTK_ICON_VIEW_ITEM_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE, PtkIconViewItemAccessible))
#define PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_ICON_VIEW_ITEM_ACCESSIBLE))

static GType ptk_icon_view_item_accessible_get_type ( void );

enum {
    ACTION_ACTIVATE,
    LAST_ACTION
};

typedef struct
{
    AtkObject parent;

    PtkIconViewItem *item;

    GtkWidget *widget;

    AtkStateSet *state_set;

    gchar *text;

    GtkTextBuffer *text_buffer;

    gchar *action_descriptions[ LAST_ACTION ];
    gchar *image_description;
    guint action_idle_handler;
}
PtkIconViewItemAccessible;

static const gchar *const ptk_icon_view_item_accessible_action_names[] =
    {
        "activate",
        NULL
    };

static const gchar *const ptk_icon_view_item_accessible_action_descriptions[] =
    {
        "Activate item",
        NULL
    };
typedef struct _PtkIconViewItemAccessibleClass
{
    AtkObjectClass parent_class;

}
PtkIconViewItemAccessibleClass;

static gboolean ptk_icon_view_item_accessible_is_showing ( PtkIconViewItemAccessible *item );

static gboolean
ptk_icon_view_item_accessible_idle_do_action ( gpointer data )
{
    PtkIconViewItemAccessible * item;
    PtkIconView *icon_view;
    GtkTreePath *path;

    GDK_THREADS_ENTER ();

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( data );
    item->action_idle_handler = 0;

    if ( item->widget != NULL )
    {
        icon_view = PTK_ICON_VIEW ( item->widget );
        path = gtk_tree_path_new_from_indices ( item->item->index, -1 );
        ptk_icon_view_item_activated ( icon_view, path );
        gtk_tree_path_free ( path );
    }

    GDK_THREADS_LEAVE ();

    return FALSE;
}

static gboolean
ptk_icon_view_item_accessible_action_do_action ( AtkAction *action,
                                                 gint i )
{
    PtkIconViewItemAccessible * item;

    if ( i < 0 || i >= LAST_ACTION )
        return FALSE;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( action );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return FALSE;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return FALSE;

    switch ( i )
    {
    case ACTION_ACTIVATE:
        if ( !item->action_idle_handler )
            item->action_idle_handler = g_idle_add ( ptk_icon_view_item_accessible_idle_do_action, item );
        break;
    default:
        g_assert_not_reached ();
        return FALSE;

    }
    return TRUE;
}

static gint
ptk_icon_view_item_accessible_action_get_n_actions ( AtkAction *action )
{
    return LAST_ACTION;
}

static const gchar *
ptk_icon_view_item_accessible_action_get_description ( AtkAction *action,
                                                       gint i )
{
    PtkIconViewItemAccessible * item;

    if ( i < 0 || i >= LAST_ACTION )
        return NULL;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( action );

    if ( item->action_descriptions[ i ] )
        return item->action_descriptions[ i ];
    else
        return ptk_icon_view_item_accessible_action_descriptions[ i ];
}

static const gchar *
ptk_icon_view_item_accessible_action_get_name ( AtkAction *action,
                                                gint i )
{
    if ( i < 0 || i >= LAST_ACTION )
        return NULL;

    return ptk_icon_view_item_accessible_action_names[ i ];
}

static gboolean
ptk_icon_view_item_accessible_action_set_description ( AtkAction *action,
                                                       gint i,
                                                       const gchar *description )
{
    PtkIconViewItemAccessible * item;

    if ( i < 0 || i >= LAST_ACTION )
        return FALSE;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( action );

    if ( item->action_descriptions[ i ] )
        g_free ( item->action_descriptions[ i ] );

    item->action_descriptions[ i ] = g_strdup ( description );

    return TRUE;
}

static void
atk_action_item_interface_init ( AtkActionIface *iface )
{
    iface->do_action = ptk_icon_view_item_accessible_action_do_action;
    iface->get_n_actions = ptk_icon_view_item_accessible_action_get_n_actions;
    iface->get_description = ptk_icon_view_item_accessible_action_get_description;
    iface->get_name = ptk_icon_view_item_accessible_action_get_name;
    iface->set_description = ptk_icon_view_item_accessible_action_set_description;
}

static const gchar *
ptk_icon_view_item_accessible_image_get_image_description ( AtkImage *image )
{
    PtkIconViewItemAccessible * item;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( image );

    return item->image_description;
}

static gboolean
ptk_icon_view_item_accessible_image_set_image_description ( AtkImage *image,
                                                            const gchar *description )
{
    PtkIconViewItemAccessible * item;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( image );

    g_free ( item->image_description );
    item->image_description = g_strdup ( item->image_description );

    return TRUE;
}

static gboolean
get_pixbuf_box ( PtkIconView *icon_view,
                 PtkIconViewItem *item,
                 GdkRectangle *box )
{
    GList * l;

    for ( l = icon_view->priv->cell_list; l; l = l->next )
    {
        PtkIconViewCellInfo * info = l->data;

        if ( GTK_IS_CELL_RENDERER_PIXBUF ( info->cell ) )
        {
            *box = item->box[ info->position ];
            return TRUE;
        }
    }

    return FALSE;
}

static gchar *
get_text ( PtkIconView *icon_view,
           PtkIconViewItem *item )
{
    GList * l;
    gchar *text;

    for ( l = icon_view->priv->cell_list; l; l = l->next )
    {
        PtkIconViewCellInfo * info = l->data;

        if ( PTK_IS_TEXT_RENDERER ( info->cell ) )
        {
            g_object_get ( info->cell, "text", &text, NULL );

            return text;
        }
    }

    return NULL;
}

static void
ptk_icon_view_item_accessible_image_get_image_size ( AtkImage *image,
                                                     gint *width,
                                                     gint *height )
{
    PtkIconViewItemAccessible * item;
    GdkRectangle box;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( image );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return ;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return ;

    if ( get_pixbuf_box ( PTK_ICON_VIEW ( item->widget ), item->item, &box ) )
    {
        *width = box.width;
        *height = box.height;
    }
}

static void
ptk_icon_view_item_accessible_image_get_image_position ( AtkImage *image,
                                                         gint *x,
                                                         gint *y,
                                                         AtkCoordType coord_type )
{
    PtkIconViewItemAccessible * item;
    GdkRectangle box;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( image );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return ;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return ;

    atk_component_get_position ( ATK_COMPONENT ( image ), x, y, coord_type );

    if ( get_pixbuf_box ( PTK_ICON_VIEW ( item->widget ), item->item, &box ) )
    {
        *x += box.x - item->item->area.x;
        *y += box.y - item->item->area.y;
    }

}

static void
atk_image_item_interface_init ( AtkImageIface *iface )
{
    iface->get_image_description = ptk_icon_view_item_accessible_image_get_image_description;
    iface->set_image_description = ptk_icon_view_item_accessible_image_set_image_description;
    iface->get_image_size = ptk_icon_view_item_accessible_image_get_image_size;
    iface->get_image_position = ptk_icon_view_item_accessible_image_get_image_position;
}

static gchar *
ptk_icon_view_item_accessible_text_get_text ( AtkText *text,
                                              gint start_pos,
                                              gint end_pos )
{
    PtkIconViewItemAccessible * item;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return NULL;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return NULL;

    buffer = item->text_buffer;
    gtk_text_buffer_get_iter_at_offset ( buffer, &start, start_pos );
    if ( end_pos < 0 )
        gtk_text_buffer_get_end_iter ( buffer, &end );
    else
        gtk_text_buffer_get_iter_at_offset ( buffer, &end, end_pos );

    return gtk_text_buffer_get_text ( buffer, &start, &end, FALSE );
}

static gunichar
ptk_icon_view_item_accessible_text_get_character_at_offset ( AtkText *text,
                                                             gint offset )
{
    PtkIconViewItemAccessible * item;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;
    gchar *string;
    gunichar unichar;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return '\0';

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return '\0';

    buffer = item->text_buffer;
    if ( offset >= gtk_text_buffer_get_char_count ( buffer ) )
        return '\0';

    gtk_text_buffer_get_iter_at_offset ( buffer, &start, offset );
    end = start;
    gtk_text_iter_forward_char ( &end );
    string = gtk_text_buffer_get_slice ( buffer, &start, &end, FALSE );
    unichar = g_utf8_get_char ( string );
    g_free( string );

    return unichar;
}

static gchar*
ptk_icon_view_item_accessible_text_get_text_before_offset ( AtkText *text,
                                                            gint offset,
                                                            AtkTextBoundary boundary_type,
                                                            gint *start_offset,
                                                            gint *end_offset )
{
    PtkIconViewItemAccessible * item;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;
    PtkIconView *icon_view;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return NULL;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return NULL;

    buffer = item->text_buffer;

    if ( !gtk_text_buffer_get_char_count ( buffer ) )
    {
        *start_offset = 0;
        *end_offset = 0;
        return g_strdup ( "" );
    }
    gtk_text_buffer_get_iter_at_offset ( buffer, &start, offset );

    end = start;

    switch ( boundary_type )
    {
    case ATK_TEXT_BOUNDARY_CHAR:
        gtk_text_iter_backward_char( &start );
        break;
    case ATK_TEXT_BOUNDARY_WORD_START:
        if ( !gtk_text_iter_starts_word ( &start ) )
            gtk_text_iter_backward_word_start ( &start );
        end = start;
        gtk_text_iter_backward_word_start( &start );
        break;
    case ATK_TEXT_BOUNDARY_WORD_END:
        if ( gtk_text_iter_inside_word ( &start ) &&
                !gtk_text_iter_starts_word ( &start ) )
            gtk_text_iter_backward_word_start ( &start );
        while ( !gtk_text_iter_ends_word ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        end = start;
        gtk_text_iter_backward_word_start( &start );
        while ( !gtk_text_iter_ends_word ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
        if ( !gtk_text_iter_starts_sentence ( &start ) )
            gtk_text_iter_backward_sentence_start ( &start );
        end = start;
        gtk_text_iter_backward_sentence_start ( &start );
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
        if ( gtk_text_iter_inside_sentence ( &start ) &&
                !gtk_text_iter_starts_sentence ( &start ) )
            gtk_text_iter_backward_sentence_start ( &start );
        while ( !gtk_text_iter_ends_sentence ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        end = start;
        gtk_text_iter_backward_sentence_start ( &start );
        while ( !gtk_text_iter_ends_sentence ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        break;
    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
        icon_view = PTK_ICON_VIEW ( item->widget );
#if 0
        /* FIXME we probably have to use GailTextCell to salvage this */
        ptk_icon_view_update_item_text ( icon_view, item->item );
        get_pango_text_offsets ( icon_view->priv->layout,
                                 buffer,
                                 0,
                                 boundary_type,
                                 offset,
                                 start_offset,
                                 end_offset,
                                 &start,
                                 &end );
#endif

        break;
    }

    *start_offset = gtk_text_iter_get_offset ( &start );
    *end_offset = gtk_text_iter_get_offset ( &end );

    return gtk_text_buffer_get_text ( buffer, &start, &end, FALSE );
}

static gchar*
ptk_icon_view_item_accessible_text_get_text_at_offset ( AtkText *text,
                                                        gint offset,
                                                        AtkTextBoundary boundary_type,
                                                        gint *start_offset,
                                                        gint *end_offset )
{
    PtkIconViewItemAccessible * item;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;
    PtkIconView *icon_view;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return NULL;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return NULL;

    buffer = item->text_buffer;

    if ( !gtk_text_buffer_get_char_count ( buffer ) )
    {
        *start_offset = 0;
        *end_offset = 0;
        return g_strdup ( "" );
    }
    gtk_text_buffer_get_iter_at_offset ( buffer, &start, offset );

    end = start;

    switch ( boundary_type )
    {
    case ATK_TEXT_BOUNDARY_CHAR:
        gtk_text_iter_forward_char ( &end );
        break;
    case ATK_TEXT_BOUNDARY_WORD_START:
        if ( !gtk_text_iter_starts_word ( &start ) )
            gtk_text_iter_backward_word_start ( &start );
        if ( gtk_text_iter_inside_word ( &end ) )
            gtk_text_iter_forward_word_end ( &end );
        while ( !gtk_text_iter_starts_word ( &end ) )
        {
            if ( !gtk_text_iter_forward_char ( &end ) )
                break;
        }
        break;
    case ATK_TEXT_BOUNDARY_WORD_END:
        if ( gtk_text_iter_inside_word ( &start ) &&
                !gtk_text_iter_starts_word ( &start ) )
            gtk_text_iter_backward_word_start ( &start );
        while ( !gtk_text_iter_ends_word ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        gtk_text_iter_forward_word_end ( &end );
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
        if ( !gtk_text_iter_starts_sentence ( &start ) )
            gtk_text_iter_backward_sentence_start ( &start );
        if ( gtk_text_iter_inside_sentence ( &end ) )
            gtk_text_iter_forward_sentence_end ( &end );
        while ( !gtk_text_iter_starts_sentence ( &end ) )
        {
            if ( !gtk_text_iter_forward_char ( &end ) )
                break;
        }
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
        if ( gtk_text_iter_inside_sentence ( &start ) &&
                !gtk_text_iter_starts_sentence ( &start ) )
            gtk_text_iter_backward_sentence_start ( &start );
        while ( !gtk_text_iter_ends_sentence ( &start ) )
        {
            if ( !gtk_text_iter_backward_char ( &start ) )
                break;
        }
        gtk_text_iter_forward_sentence_end ( &end );
        break;
    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
        icon_view = PTK_ICON_VIEW ( item->widget );
#if 0
        /* FIXME we probably have to use GailTextCell to salvage this */
        ptk_icon_view_update_item_text ( icon_view, item->item );
        get_pango_text_offsets ( icon_view->priv->layout,
                                 buffer,
                                 1,
                                 boundary_type,
                                 offset,
                                 start_offset,
                                 end_offset,
                                 &start,
                                 &end );
#endif

        break;
    }


    *start_offset = gtk_text_iter_get_offset ( &start );
    *end_offset = gtk_text_iter_get_offset ( &end );

    return gtk_text_buffer_get_text ( buffer, &start, &end, FALSE );
}

static gchar*
ptk_icon_view_item_accessible_text_get_text_after_offset ( AtkText *text,
                                                           gint offset,
                                                           AtkTextBoundary boundary_type,
                                                           gint *start_offset,
                                                           gint *end_offset )
{
    PtkIconViewItemAccessible * item;
    GtkTextIter start, end;
    GtkTextBuffer *buffer;
    PtkIconView *icon_view;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return NULL;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return NULL;

    buffer = item->text_buffer;

    if ( !gtk_text_buffer_get_char_count ( buffer ) )
    {
        *start_offset = 0;
        *end_offset = 0;
        return g_strdup ( "" );
    }
    gtk_text_buffer_get_iter_at_offset ( buffer, &start, offset );

    end = start;

    switch ( boundary_type )
    {
    case ATK_TEXT_BOUNDARY_CHAR:
        gtk_text_iter_forward_char( &start );
        gtk_text_iter_forward_chars( &end, 2 );
        break;
    case ATK_TEXT_BOUNDARY_WORD_START:
        if ( gtk_text_iter_inside_word ( &end ) )
            gtk_text_iter_forward_word_end ( &end );
        while ( !gtk_text_iter_starts_word ( &end ) )
        {
            if ( !gtk_text_iter_forward_char ( &end ) )
                break;
        }
        start = end;
        if ( !gtk_text_iter_is_end ( &end ) )
        {
            gtk_text_iter_forward_word_end ( &end );
            while ( !gtk_text_iter_starts_word ( &end ) )
            {
                if ( !gtk_text_iter_forward_char ( &end ) )
                    break;
            }
        }
        break;
    case ATK_TEXT_BOUNDARY_WORD_END:
        gtk_text_iter_forward_word_end ( &end );
        start = end;
        if ( !gtk_text_iter_is_end ( &end ) )
            gtk_text_iter_forward_word_end ( &end );
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
        if ( gtk_text_iter_inside_sentence ( &end ) )
            gtk_text_iter_forward_sentence_end ( &end );
        while ( !gtk_text_iter_starts_sentence ( &end ) )
        {
            if ( !gtk_text_iter_forward_char ( &end ) )
                break;
        }
        start = end;
        if ( !gtk_text_iter_is_end ( &end ) )
        {
            gtk_text_iter_forward_sentence_end ( &end );
            while ( !gtk_text_iter_starts_sentence ( &end ) )
            {
                if ( !gtk_text_iter_forward_char ( &end ) )
                    break;
            }
        }
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
        gtk_text_iter_forward_sentence_end ( &end );
        start = end;
        if ( !gtk_text_iter_is_end ( &end ) )
            gtk_text_iter_forward_sentence_end ( &end );
        break;
    case ATK_TEXT_BOUNDARY_LINE_START:
    case ATK_TEXT_BOUNDARY_LINE_END:
        icon_view = PTK_ICON_VIEW ( item->widget );
#if 0
        /* FIXME we probably have to use GailTextCell to salvage this */
        ptk_icon_view_update_item_text ( icon_view, item->item );
        get_pango_text_offsets ( icon_view->priv->layout,
                                 buffer,
                                 2,
                                 boundary_type,
                                 offset,
                                 start_offset,
                                 end_offset,
                                 &start,
                                 &end );
#endif

        break;
    }
    *start_offset = gtk_text_iter_get_offset ( &start );
    *end_offset = gtk_text_iter_get_offset ( &end );

    return gtk_text_buffer_get_text ( buffer, &start, &end, FALSE );
}

static gint
ptk_icon_view_item_accessible_text_get_character_count ( AtkText *text )
{
    PtkIconViewItemAccessible * item;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return 0;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return 0;

    return gtk_text_buffer_get_char_count ( item->text_buffer );
}

static void
ptk_icon_view_item_accessible_text_get_character_extents ( AtkText *text,
                                                           gint offset,
                                                           gint *x,
                                                           gint *y,
                                                           gint *width,
                                                           gint *height,
                                                           AtkCoordType coord_type )
{
    PtkIconViewItemAccessible * item;
    PtkIconView *icon_view;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return ;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return ;

    icon_view = PTK_ICON_VIEW ( item->widget );
#if 0
    /* FIXME we probably have to use GailTextCell to salvage this */
    ptk_icon_view_update_item_text ( icon_view, item->item );
    item_text = pango_layout_get_text ( icon_view->priv->layout );
    index = g_utf8_offset_to_pointer ( item_text, offset ) - item_text;
    pango_layout_index_to_pos ( icon_view->priv->layout, index, &char_rect );

    atk_component_get_position ( ATK_COMPONENT ( text ), x, y, coord_type );
    *x += item->item->layout_x - item->item->area.x + char_rect.x / PANGO_SCALE;
    /* Look at ptk_icon_view_paint_item() to see where the text is. */
    *x -= ( ( item->item->area.width - item->item->layout_width ) / 2 ) + ( MAX ( item->item->pixbuf_width, icon_view->priv->item_width ) - item->item->area.width ) / 2,
          *y += item->item->layout_y - item->item->area.y + char_rect.y / PANGO_SCALE;
    *width = char_rect.width / PANGO_SCALE;
    *height = char_rect.height / PANGO_SCALE;
#endif
}

static gint
ptk_icon_view_item_accessible_text_get_offset_at_point ( AtkText *text,
                                                         gint x,
                                                         gint y,
                                                         AtkCoordType coord_type )
{
    PtkIconViewItemAccessible * item;
    PtkIconView *icon_view;
    gint offset;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( text );

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return -1;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return -1;

    icon_view = PTK_ICON_VIEW ( item->widget );
#if 0
    /* FIXME we probably have to use GailTextCell to salvage this */
    ptk_icon_view_update_item_text ( icon_view, item->item );
    atk_component_get_position ( ATK_COMPONENT ( text ), &l_x, &l_y, coord_type );
    x -= l_x + item->item->layout_x - item->item->area.x;
    x += ( ( item->item->area.width - item->item->layout_width ) / 2 ) + ( MAX ( item->item->pixbuf_width, icon_view->priv->item_width ) - item->item->area.width ) / 2,
         y -= l_y + item->item->layout_y - item->item->area.y;
    item_text = pango_layout_get_text ( icon_view->priv->layout );
    if ( !pango_layout_xy_to_index ( icon_view->priv->layout,
                                     x * PANGO_SCALE,
                                     y * PANGO_SCALE,
                                     &index, NULL ) )
    {
        if ( x < 0 || y < 0 )
            index = 0;
        else
            index = -1;
    }
    if ( index == -1 )
        offset = g_utf8_strlen ( item_text, -1 );
    else
        offset = g_utf8_pointer_to_offset ( item_text, item_text + index );
#endif

    return offset;
}

static void
atk_text_item_interface_init ( AtkTextIface *iface )
{
    iface->get_text = ptk_icon_view_item_accessible_text_get_text;
    iface->get_character_at_offset = ptk_icon_view_item_accessible_text_get_character_at_offset;
    iface->get_text_before_offset = ptk_icon_view_item_accessible_text_get_text_before_offset;
    iface->get_text_at_offset = ptk_icon_view_item_accessible_text_get_text_at_offset;
    iface->get_text_after_offset = ptk_icon_view_item_accessible_text_get_text_after_offset;
    iface->get_character_count = ptk_icon_view_item_accessible_text_get_character_count;
    iface->get_character_extents = ptk_icon_view_item_accessible_text_get_character_extents;
    iface->get_offset_at_point = ptk_icon_view_item_accessible_text_get_offset_at_point;
}

static void
ptk_icon_view_item_accessible_get_extents ( AtkComponent *component,
                                            gint *x,
                                            gint *y,
                                            gint *width,
                                            gint *height,
                                            AtkCoordType coord_type )
{
    PtkIconViewItemAccessible * item;
    AtkObject *parent_obj;
    gint l_x, l_y;

    g_return_if_fail ( PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE ( component ) );

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( component );
    if ( !GTK_IS_WIDGET ( item->widget ) )
        return ;

    if ( atk_state_set_contains_state ( item->state_set, ATK_STATE_DEFUNCT ) )
        return ;

    *width = item->item->area.width;
    *height = item->item->area.height;
    if ( ptk_icon_view_item_accessible_is_showing ( item ) )
    {
        parent_obj = gtk_widget_get_accessible ( item->widget );
        atk_component_get_position ( ATK_COMPONENT ( parent_obj ), &l_x, &l_y, coord_type );
        *x = l_x + item->item->area.x;
        *y = l_y + item->item->area.y;
    }
    else
    {
        *x = G_MININT;
        *y = G_MININT;
    }
}

static gboolean
ptk_icon_view_item_accessible_grab_focus ( AtkComponent *component )
{
    PtkIconViewItemAccessible * item;
    GtkWidget *toplevel;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE ( component ), FALSE );

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( component );
    if ( !GTK_IS_WIDGET ( item->widget ) )
        return FALSE;

    gtk_widget_grab_focus ( item->widget );
    ptk_icon_view_set_cursor_item ( PTK_ICON_VIEW ( item->widget ), item->item, -1 );
    toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( item->widget ) );
    if ( GTK_WIDGET_TOPLEVEL ( toplevel ) )
        gtk_window_present ( GTK_WINDOW ( toplevel ) );

    return TRUE;
}

static void
atk_component_item_interface_init ( AtkComponentIface *iface )
{
    iface->get_extents = ptk_icon_view_item_accessible_get_extents;
    iface->grab_focus = ptk_icon_view_item_accessible_grab_focus;
}

static gboolean
ptk_icon_view_item_accessible_add_state ( PtkIconViewItemAccessible *item,
                                          AtkStateType state_type,
                                          gboolean emit_signal )
{
    gboolean rc;

    rc = atk_state_set_add_state ( item->state_set, state_type );
    /*
     * The signal should only be generated if the value changed,
     * not when the item is set up.  So states that are set
     * initially should pass FALSE as the emit_signal argument.
     */

    if ( emit_signal )
    {
        atk_object_notify_state_change ( ATK_OBJECT ( item ), state_type, TRUE );
        /* If state_type is ATK_STATE_VISIBLE, additional notification */
        if ( state_type == ATK_STATE_VISIBLE )
            g_signal_emit_by_name ( item, "visible_data_changed" );
    }

    return rc;
}

static gboolean
ptk_icon_view_item_accessible_remove_state ( PtkIconViewItemAccessible *item,
                                             AtkStateType state_type,
                                             gboolean emit_signal )
{
    if ( atk_state_set_contains_state ( item->state_set, state_type ) )
    {
        gboolean rc;

        rc = atk_state_set_remove_state ( item->state_set, state_type );
        /*
         * The signal should only be generated if the value changed,
         * not when the item is set up.  So states that are set
         * initially should pass FALSE as the emit_signal argument.
         */

        if ( emit_signal )
        {
            atk_object_notify_state_change ( ATK_OBJECT ( item ), state_type, FALSE );
            /* If state_type is ATK_STATE_VISIBLE, additional notification */
            if ( state_type == ATK_STATE_VISIBLE )
                g_signal_emit_by_name ( item, "visible_data_changed" );
        }

        return rc;
    }
    else
        return FALSE;
}

static gboolean
ptk_icon_view_item_accessible_is_showing ( PtkIconViewItemAccessible *item )
{
    PtkIconView * icon_view;
    GdkRectangle visible_rect;
    gboolean is_showing;

    /*
     * An item is considered "SHOWING" if any part of the item is in the
     * visible rectangle.
     */

    if ( !PTK_IS_ICON_VIEW ( item->widget ) )
        return FALSE;

    if ( item->item == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( item->widget );
    visible_rect.x = 0;
    if ( icon_view->priv->hadjustment )
        visible_rect.x += icon_view->priv->hadjustment->value;
    visible_rect.y = 0;
    if ( icon_view->priv->hadjustment )
        visible_rect.y += icon_view->priv->vadjustment->value;
    visible_rect.width = item->widget->allocation.width;
    visible_rect.height = item->widget->allocation.height;

    if ( ( ( item->item->area.x + item->item->area.width ) < visible_rect.x ) ||
            ( ( item->item->area.y + item->item->area.height ) < ( visible_rect.y ) ) ||
            ( item->item->area.x > ( visible_rect.x + visible_rect.width ) ) ||
            ( item->item->area.y > ( visible_rect.y + visible_rect.height ) ) )
        is_showing = FALSE;
    else
        is_showing = TRUE;

    return is_showing;
}

static gboolean
ptk_icon_view_item_accessible_set_visibility ( PtkIconViewItemAccessible *item,
                                               gboolean emit_signal )
{
    if ( ptk_icon_view_item_accessible_is_showing ( item ) )
        return ptk_icon_view_item_accessible_add_state ( item, ATK_STATE_SHOWING,
                                                         emit_signal );
    else
        return ptk_icon_view_item_accessible_remove_state ( item, ATK_STATE_SHOWING,
                                                            emit_signal );
}

static void
ptk_icon_view_item_accessible_object_init ( PtkIconViewItemAccessible *item )
{
    gint i;

    item->state_set = atk_state_set_new ();

    atk_state_set_add_state ( item->state_set, ATK_STATE_ENABLED );
    atk_state_set_add_state ( item->state_set, ATK_STATE_FOCUSABLE );
    atk_state_set_add_state ( item->state_set, ATK_STATE_SENSITIVE );
    atk_state_set_add_state ( item->state_set, ATK_STATE_SELECTABLE );
    atk_state_set_add_state ( item->state_set, ATK_STATE_VISIBLE );

    for ( i = 0; i < LAST_ACTION; i++ )
        item->action_descriptions[ i ] = NULL;

    item->image_description = NULL;

    item->action_idle_handler = 0;
}

static void
ptk_icon_view_item_accessible_finalize ( GObject *object )
{
    PtkIconViewItemAccessible * item;
    gint i;

    g_return_if_fail ( PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE ( object ) );

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( object );

    if ( item->widget )
        g_object_remove_weak_pointer ( G_OBJECT ( item->widget ), ( gpointer ) & item->widget );

    if ( item->state_set )
        g_object_unref ( item->state_set );

    if ( item->text_buffer )
        g_object_unref ( item->text_buffer );

    for ( i = 0; i < LAST_ACTION; i++ )
        g_free ( item->action_descriptions[ i ] );

    g_free ( item->image_description );

    if ( item->action_idle_handler )
    {
        g_source_remove ( item->action_idle_handler );
        item->action_idle_handler = 0;
    }

    G_OBJECT_CLASS ( accessible_item_parent_class ) ->finalize ( object );
}

static G_CONST_RETURN gchar*
ptk_icon_view_item_accessible_get_name ( AtkObject *obj )
{
    if ( obj->name )
        return obj->name;
    else
    {
        PtkIconViewItemAccessible *item;
        GtkTextIter start_iter;
        GtkTextIter end_iter;

        item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( obj );

        gtk_text_buffer_get_start_iter ( item->text_buffer, &start_iter );
        gtk_text_buffer_get_end_iter ( item->text_buffer, &end_iter );

        return gtk_text_buffer_get_text ( item->text_buffer, &start_iter, &end_iter, FALSE );
    }
}

static AtkObject*
ptk_icon_view_item_accessible_get_parent ( AtkObject *obj )
{
    PtkIconViewItemAccessible * item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE ( obj ), NULL );
    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( obj );

    if ( item->widget )
        return gtk_widget_get_accessible ( item->widget );
    else
        return NULL;
}

static gint
ptk_icon_view_item_accessible_get_index_in_parent ( AtkObject *obj )
{
    PtkIconViewItemAccessible * item;

    g_return_val_if_fail ( PTK_IS_ICON_VIEW_ITEM_ACCESSIBLE ( obj ), 0 );
    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( obj );

    return item->item->index;
}

static AtkStateSet *
ptk_icon_view_item_accessible_ref_state_set ( AtkObject *obj )
{
    PtkIconViewItemAccessible * item;
    PtkIconView *icon_view;

    item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( obj );
    g_return_val_if_fail ( item->state_set, NULL );

    if ( !item->widget )
        return NULL;

    icon_view = PTK_ICON_VIEW ( item->widget );
    if ( icon_view->priv->cursor_item == item->item )
        atk_state_set_add_state ( item->state_set, ATK_STATE_FOCUSED );
    else
        atk_state_set_remove_state ( item->state_set, ATK_STATE_FOCUSED );

    return g_object_ref ( item->state_set );
}

static void
ptk_icon_view_item_accessible_class_init ( AtkObjectClass *klass )
{
    GObjectClass * gobject_class;

    accessible_item_parent_class = g_type_class_peek_parent ( klass );

    gobject_class = ( GObjectClass * ) klass;

    gobject_class->finalize = ptk_icon_view_item_accessible_finalize;

    klass->get_index_in_parent = ptk_icon_view_item_accessible_get_index_in_parent;
    klass->get_name = ptk_icon_view_item_accessible_get_name;
    klass->get_parent = ptk_icon_view_item_accessible_get_parent;
    klass->ref_state_set = ptk_icon_view_item_accessible_ref_state_set;
}

static GType
ptk_icon_view_item_accessible_get_type ( void )
{
    static GType type = 0;

    if ( !type )
    {
        static const GTypeInfo tinfo =
            {
                sizeof ( PtkIconViewItemAccessibleClass ),
                ( GBaseInitFunc ) NULL,     /* base init */
                ( GBaseFinalizeFunc ) NULL,     /* base finalize */
                ( GClassInitFunc ) ptk_icon_view_item_accessible_class_init,     /* class init */
                ( GClassFinalizeFunc ) NULL,     /* class finalize */
                NULL,     /* class data */
                sizeof ( PtkIconViewItemAccessible ),     /* instance size */
                0,     /* nb preallocs */
                ( GInstanceInitFunc ) ptk_icon_view_item_accessible_object_init,     /* instance init */
                NULL /* value table */
            };

        static const GInterfaceInfo atk_component_info =
            {
                ( GInterfaceInitFunc ) atk_component_item_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };
        static const GInterfaceInfo atk_action_info =
            {
                ( GInterfaceInitFunc ) atk_action_item_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };
        static const GInterfaceInfo atk_image_info =
            {
                ( GInterfaceInitFunc ) atk_image_item_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };
        static const GInterfaceInfo atk_text_info =
            {
                ( GInterfaceInitFunc ) atk_text_item_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };

        type = g_type_register_static ( ATK_TYPE_OBJECT,
                                        I_( "PtkIconViewItemAccessible" ), &tinfo, 0 );
        g_type_add_interface_static ( type, ATK_TYPE_COMPONENT,
                                      &atk_component_info );
        g_type_add_interface_static ( type, ATK_TYPE_ACTION,
                                      &atk_action_info );
        g_type_add_interface_static ( type, ATK_TYPE_IMAGE,
                                      &atk_image_info );
        g_type_add_interface_static ( type, ATK_TYPE_TEXT,
                                      &atk_text_info );
    }

    return type;
}

#define PTK_TYPE_ICON_VIEW_ACCESSIBLE      (ptk_icon_view_accessible_get_type ())
#define PTK_ICON_VIEW_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), PTK_TYPE_ICON_VIEW_ACCESSIBLE, PtkIconViewAccessible))
#define PTK_IS_ICON_VIEW_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_ICON_VIEW_ACCESSIBLE))

static GType ptk_icon_view_accessible_get_type ( void );

typedef struct
{
    AtkObject parent;
}
PtkIconViewAccessible;

typedef struct
{
    AtkObject *item;
    gint index;
}
PtkIconViewItemAccessibleInfo;

typedef struct
{
    GList *items;

    GtkAdjustment *old_hadj;
    GtkAdjustment *old_vadj;

    GtkTreeModel *model;

}
PtkIconViewAccessiblePrivate;

static PtkIconViewAccessiblePrivate *
ptk_icon_view_accessible_get_priv ( AtkObject *accessible )
{
    return g_object_get_qdata ( G_OBJECT ( accessible ),
                                accessible_private_data_quark );
}

static void
ptk_icon_view_item_accessible_info_new ( AtkObject *accessible,
                                         AtkObject *item,
                                         gint index )
{
    PtkIconViewItemAccessibleInfo * info;
    PtkIconViewItemAccessibleInfo *tmp_info;
    PtkIconViewAccessiblePrivate *priv;
    GList *items;

    info = g_new ( PtkIconViewItemAccessibleInfo, 1 );
    info->item = item;
    info->index = index;

    priv = ptk_icon_view_accessible_get_priv ( accessible );
    items = priv->items;
    while ( items )
    {
        tmp_info = items->data;
        if ( tmp_info->index > index )
            break;
        items = items->next;
    }
    priv->items = g_list_insert_before ( priv->items, items, info );
    priv->old_hadj = NULL;
    priv->old_vadj = NULL;
}

static gint
ptk_icon_view_accessible_get_n_children ( AtkObject *accessible )
{
    PtkIconView * icon_view;
    GtkWidget *widget;

    widget = GTK_ACCESSIBLE ( accessible ) ->widget;
    if ( !widget )
        return 0;

    icon_view = PTK_ICON_VIEW ( widget );

    return g_list_length ( icon_view->priv->items );
}

static AtkObject *
ptk_icon_view_accessible_find_child ( AtkObject *accessible,
                                      gint index )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconViewItemAccessibleInfo *info;
    GList *items;

    priv = ptk_icon_view_accessible_get_priv ( accessible );
    items = priv->items;

    while ( items )
    {
        info = items->data;
        if ( info->index == index )
            return info->item;
        items = items->next;
    }
    return NULL;
}

static AtkObject *
ptk_icon_view_accessible_ref_child ( AtkObject *accessible,
                                     gint index )
{
    PtkIconView * icon_view;
    GtkWidget *widget;
    GList *icons;
    AtkObject *obj;
    PtkIconViewItemAccessible *a11y_item;

    widget = GTK_ACCESSIBLE ( accessible ) ->widget;
    if ( !widget )
        return NULL;

    icon_view = PTK_ICON_VIEW ( widget );
    icons = g_list_nth ( icon_view->priv->items, index );
    obj = NULL;
    if ( icons )
    {
        PtkIconViewItem * item = icons->data;

        g_return_val_if_fail ( item->index == index, NULL );
        obj = ptk_icon_view_accessible_find_child ( accessible, index );
        if ( !obj )
        {
            obj = g_object_new ( ptk_icon_view_item_accessible_get_type (), NULL );
            ptk_icon_view_item_accessible_info_new ( accessible,
                                                     obj,
                                                     index );
            obj->role = ATK_ROLE_ICON;
            a11y_item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( obj );
            a11y_item->item = item;
            a11y_item->widget = widget;
            a11y_item->text_buffer = gtk_text_buffer_new ( NULL );

            ptk_icon_view_set_cell_data ( icon_view, item );
            gtk_text_buffer_set_text ( a11y_item->text_buffer,
                                       get_text ( icon_view, item ), -1 );

            ptk_icon_view_item_accessible_set_visibility ( a11y_item, FALSE );
            g_object_add_weak_pointer ( G_OBJECT ( widget ), ( gpointer ) & ( a11y_item->widget ) );
        }
        g_object_ref ( obj );
    }
    return obj;
}

static void
ptk_icon_view_accessible_traverse_items ( PtkIconViewAccessible *view,
                                          GList *list )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconViewItemAccessibleInfo *info;
    PtkIconViewItemAccessible *item;
    GList *items;

    priv = ptk_icon_view_accessible_get_priv ( ATK_OBJECT ( view ) );
    if ( priv->items )
    {
        GtkWidget * widget;
        gboolean act_on_item;

        widget = GTK_ACCESSIBLE ( view ) ->widget;
        if ( widget == NULL )
            return ;

        items = priv->items;

        act_on_item = ( list == NULL );

        while ( items )
        {

            info = ( PtkIconViewItemAccessibleInfo * ) items->data;
            item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( info->item );

            if ( act_on_item == FALSE && list == items )
                act_on_item = TRUE;

            if ( act_on_item )
                ptk_icon_view_item_accessible_set_visibility ( item, TRUE );

            items = items->next;
        }
    }
}

static void
ptk_icon_view_accessible_adjustment_changed ( GtkAdjustment *adjustment,
                                              PtkIconView *icon_view )
{
    AtkObject * obj;
    PtkIconViewAccessible *view;

    /*
     * The scrollbars have changed
     */
    obj = gtk_widget_get_accessible ( GTK_WIDGET ( icon_view ) );
    view = PTK_ICON_VIEW_ACCESSIBLE ( obj );

    ptk_icon_view_accessible_traverse_items ( view, NULL );
}

static void
ptk_icon_view_accessible_set_scroll_adjustments ( GtkWidget *widget,
                                                  GtkAdjustment *hadj,
                                                  GtkAdjustment *vadj )
{
    AtkObject * atk_obj;
    PtkIconViewAccessiblePrivate *priv;

    atk_obj = gtk_widget_get_accessible ( widget );
    priv = ptk_icon_view_accessible_get_priv ( atk_obj );

    if ( priv->old_hadj != hadj )
    {
        if ( priv->old_hadj )
        {
            g_object_remove_weak_pointer ( G_OBJECT ( priv->old_hadj ),
                                           ( gpointer * ) & priv->old_hadj );

            g_signal_handlers_disconnect_by_func ( priv->old_hadj,
                                                   ( gpointer ) ptk_icon_view_accessible_adjustment_changed,
                                                   widget );
        }
        priv->old_hadj = hadj;
        if ( priv->old_hadj )
        {
            g_object_add_weak_pointer ( G_OBJECT ( priv->old_hadj ),
                                        ( gpointer * ) & priv->old_hadj );
            g_signal_connect ( hadj,
                               "value-changed",
                               G_CALLBACK ( ptk_icon_view_accessible_adjustment_changed ),
                               widget );
        }
    }
    if ( priv->old_vadj != vadj )
    {
        if ( priv->old_vadj )
        {
            g_object_remove_weak_pointer ( G_OBJECT ( priv->old_vadj ),
                                           ( gpointer * ) & priv->old_vadj );

            g_signal_handlers_disconnect_by_func ( priv->old_vadj,
                                                   ( gpointer ) ptk_icon_view_accessible_adjustment_changed,
                                                   widget );
        }
        priv->old_vadj = vadj;
        if ( priv->old_vadj )
        {
            g_object_add_weak_pointer ( G_OBJECT ( priv->old_vadj ),
                                        ( gpointer * ) & priv->old_vadj );
            g_signal_connect ( vadj,
                               "value-changed",
                               G_CALLBACK ( ptk_icon_view_accessible_adjustment_changed ),
                               widget );
        }
    }
}

static void
ptk_icon_view_accessible_model_row_changed ( GtkTreeModel *tree_model,
                                             GtkTreePath *path,
                                             GtkTreeIter *iter,
                                             gpointer user_data )
{
    AtkObject * atk_obj;

    atk_obj = gtk_widget_get_accessible ( GTK_WIDGET ( user_data ) );
    g_signal_emit_by_name ( atk_obj, "visible-data-changed" );

    return ;
}

static void
ptk_icon_view_accessible_model_row_inserted ( GtkTreeModel *tree_model,
                                              GtkTreePath *path,
                                              GtkTreeIter *iter,
                                              gpointer user_data )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconViewItemAccessibleInfo *info;
    PtkIconViewAccessible *view;
    PtkIconViewItemAccessible *item;
    GList *items;
    GList *tmp_list;
    AtkObject *atk_obj;
    gint index;

    index = gtk_tree_path_get_indices( path ) [ 0 ];
    atk_obj = gtk_widget_get_accessible ( GTK_WIDGET ( user_data ) );
    view = PTK_ICON_VIEW_ACCESSIBLE ( atk_obj );
    priv = ptk_icon_view_accessible_get_priv ( atk_obj );

    items = priv->items;
    tmp_list = NULL;
    while ( items )
    {
        info = items->data;
        item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( info->item );
        if ( info->index != item->item->index )
        {
            if ( info->index < index )
                g_warning ( "Unexpected index value on insertion %d %d", index, info->index );

            if ( tmp_list == NULL )
                tmp_list = items;

            info->index = item->item->index;
        }

        items = items->next;
    }
    ptk_icon_view_accessible_traverse_items ( view, tmp_list );
    g_signal_emit_by_name ( atk_obj, "children_changed::add",
                            index, NULL, NULL );
    return ;
}

static void
ptk_icon_view_accessible_model_row_deleted ( GtkTreeModel *tree_model,
                                             GtkTreePath *path,
                                             gpointer user_data )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconViewItemAccessibleInfo *info;
    PtkIconViewAccessible *view;
    PtkIconViewItemAccessible *item;
    GList *items;
    GList *tmp_list;
    GList *deleted_item;
    AtkObject *atk_obj;
    gint index;

    index = gtk_tree_path_get_indices( path ) [ 0 ];
    atk_obj = gtk_widget_get_accessible ( GTK_WIDGET ( user_data ) );
    view = PTK_ICON_VIEW_ACCESSIBLE ( atk_obj );
    priv = ptk_icon_view_accessible_get_priv ( atk_obj );

    items = priv->items;
    tmp_list = NULL;
    deleted_item = NULL;
    info = NULL;
    while ( items )
    {
        info = items->data;
        item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( info->item );
        if ( info->index == index )
        {
            deleted_item = items;
        }
        if ( info->index != item->item->index )
        {
            if ( tmp_list == NULL )
                tmp_list = items;
            else
                info->index = item->item->index;
        }

        items = items->next;
    }
    ptk_icon_view_accessible_traverse_items ( view, tmp_list );
    if ( deleted_item )
    {
        info = deleted_item->data;
        ptk_icon_view_item_accessible_add_state ( PTK_ICON_VIEW_ITEM_ACCESSIBLE ( info->item ), ATK_STATE_DEFUNCT, TRUE );
    }
    g_signal_emit_by_name ( atk_obj, "children_changed::remove",
                            index, NULL, NULL );
    if ( deleted_item )
    {
        priv->items = g_list_remove_link ( priv->items, deleted_item );
        g_free ( info );
    }

    return ;
}

static gint
ptk_icon_view_accessible_item_compare ( PtkIconViewItemAccessibleInfo *i1,
                                        PtkIconViewItemAccessibleInfo *i2 )
{
    return i1->index - i2->index;
}

static void
ptk_icon_view_accessible_model_rows_reordered ( GtkTreeModel *tree_model,
                                                GtkTreePath *path,
                                                GtkTreeIter *iter,
                                                gint *new_order,
                                                gpointer user_data )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconViewItemAccessibleInfo *info;
    PtkIconViewAccessible *view;
    PtkIconView *icon_view;
    PtkIconViewItemAccessible *item;
    GList *items;
    GList *tmp_list;
    AtkObject *atk_obj;

    atk_obj = gtk_widget_get_accessible ( GTK_WIDGET ( user_data ) );
    icon_view = PTK_ICON_VIEW ( user_data );
    view = PTK_ICON_VIEW_ACCESSIBLE ( atk_obj );
    priv = ptk_icon_view_accessible_get_priv ( atk_obj );

    items = priv->items;
    tmp_list = NULL;
    while ( items )
    {
        info = items->data;
        item = PTK_ICON_VIEW_ITEM_ACCESSIBLE ( info->item );
        info->index = new_order[ info->index ];
        tmp_list = g_list_nth ( icon_view->priv->items, info->index );
        item->item = tmp_list->data;
        items = items->next;
    }
    priv->items = g_list_sort ( priv->items,
                                ( GCompareFunc ) ptk_icon_view_accessible_item_compare );

    return ;
}

static void
ptk_icon_view_accessible_disconnect_model_signals ( GtkTreeModel *model,
                                                    GtkWidget *widget )
{
    GObject * obj;

    obj = G_OBJECT ( model );
    g_signal_handlers_disconnect_by_func ( obj, ( gpointer ) ptk_icon_view_accessible_model_row_changed, widget );
    g_signal_handlers_disconnect_by_func ( obj, ( gpointer ) ptk_icon_view_accessible_model_row_inserted, widget );
    g_signal_handlers_disconnect_by_func ( obj, ( gpointer ) ptk_icon_view_accessible_model_row_deleted, widget );
    g_signal_handlers_disconnect_by_func ( obj, ( gpointer ) ptk_icon_view_accessible_model_rows_reordered, widget );
}

static void
ptk_icon_view_accessible_connect_model_signals ( PtkIconView *icon_view )
{
    GObject * obj;

    obj = G_OBJECT ( icon_view->priv->model );
    g_signal_connect_data ( obj, "row-changed",
                            ( GCallback ) ptk_icon_view_accessible_model_row_changed,
                            icon_view, NULL, 0 );
    g_signal_connect_data ( obj, "row-inserted",
                            ( GCallback ) ptk_icon_view_accessible_model_row_inserted,
                            icon_view, NULL, G_CONNECT_AFTER );
    g_signal_connect_data ( obj, "row-deleted",
                            ( GCallback ) ptk_icon_view_accessible_model_row_deleted,
                            icon_view, NULL, G_CONNECT_AFTER );
    g_signal_connect_data ( obj, "rows-reordered",
                            ( GCallback ) ptk_icon_view_accessible_model_rows_reordered,
                            icon_view, NULL, G_CONNECT_AFTER );
}

static void
ptk_icon_view_accessible_clear_cache ( PtkIconViewAccessiblePrivate *priv )
{
    PtkIconViewItemAccessibleInfo * info;
    GList *items;

    items = priv->items;
    while ( items )
    {
        info = ( PtkIconViewItemAccessibleInfo * ) items->data;
        g_object_unref ( info->item );
        g_free ( items->data );
        items = items->next;
    }
    g_list_free ( priv->items );
    priv->items = NULL;
}

static void
ptk_icon_view_accessible_notify_gtk ( GObject *obj,
                                      GParamSpec *pspec )
{
    PtkIconView * icon_view;
    GtkWidget *widget;
    AtkObject *atk_obj;
    PtkIconViewAccessible *view;
    PtkIconViewAccessiblePrivate *priv;

    if ( strcmp ( pspec->name, "model" ) == 0 )
    {
        widget = GTK_WIDGET ( obj );
        atk_obj = gtk_widget_get_accessible ( widget );
        view = PTK_ICON_VIEW_ACCESSIBLE ( atk_obj );
        priv = ptk_icon_view_accessible_get_priv ( atk_obj );
        if ( priv->model )
        {
            g_object_remove_weak_pointer ( G_OBJECT ( priv->model ),
                                           ( gpointer * ) & priv->model );
            ptk_icon_view_accessible_disconnect_model_signals ( priv->model, widget );
        }
        ptk_icon_view_accessible_clear_cache ( priv );

        icon_view = PTK_ICON_VIEW ( obj );
        priv->model = icon_view->priv->model;
        /* If there is no model the PtkIconView is probably being destroyed */
        if ( priv->model )
        {
            g_object_add_weak_pointer ( G_OBJECT ( priv->model ), ( gpointer * ) & priv->model );
            ptk_icon_view_accessible_connect_model_signals ( icon_view );
        }
    }

    return ;
}

static void
ptk_icon_view_accessible_initialize ( AtkObject *accessible,
                                      gpointer data )
{
    PtkIconViewAccessiblePrivate * priv;
    PtkIconView *icon_view;

    if ( ATK_OBJECT_CLASS ( accessible_parent_class ) ->initialize )
        ATK_OBJECT_CLASS ( accessible_parent_class ) ->initialize ( accessible, data );

    priv = g_new0 ( PtkIconViewAccessiblePrivate, 1 );
    g_object_set_qdata ( G_OBJECT ( accessible ),
                         accessible_private_data_quark,
                         priv );

    icon_view = PTK_ICON_VIEW ( data );
    if ( icon_view->priv->hadjustment )
    {
        priv->old_hadj = icon_view->priv->hadjustment;
        g_object_add_weak_pointer ( G_OBJECT ( priv->old_hadj ), ( gpointer * ) & priv->old_hadj );
        g_signal_connect ( icon_view->priv->hadjustment,
                           "value-changed",
                           G_CALLBACK ( ptk_icon_view_accessible_adjustment_changed ),
                           icon_view );
    }
    if ( icon_view->priv->vadjustment )
    {
        priv->old_vadj = icon_view->priv->vadjustment;
        g_object_add_weak_pointer ( G_OBJECT ( priv->old_vadj ), ( gpointer * ) & priv->old_vadj );
        g_signal_connect ( icon_view->priv->vadjustment,
                           "value-changed",
                           G_CALLBACK ( ptk_icon_view_accessible_adjustment_changed ),
                           icon_view );
    }
    g_signal_connect_after ( data,
                             "set_scroll_adjustments",
                             G_CALLBACK ( ptk_icon_view_accessible_set_scroll_adjustments ),
                             NULL );
    g_signal_connect ( data,
                       "notify",
                       G_CALLBACK ( ptk_icon_view_accessible_notify_gtk ),
                       NULL );

    priv->model = icon_view->priv->model;
    if ( priv->model )
    {
        g_object_add_weak_pointer ( G_OBJECT ( priv->model ), ( gpointer * ) & priv->model );
        ptk_icon_view_accessible_connect_model_signals ( icon_view );
    }

    accessible->role = ATK_ROLE_LAYERED_PANE;
}

static void
ptk_icon_view_accessible_finalize ( GObject *object )
{
    PtkIconViewAccessiblePrivate * priv;

    priv = ptk_icon_view_accessible_get_priv ( ATK_OBJECT ( object ) );
    ptk_icon_view_accessible_clear_cache ( priv );

    g_free ( priv );

    G_OBJECT_CLASS ( accessible_parent_class ) ->finalize ( object );
}

static void
ptk_icon_view_accessible_destroyed ( GtkWidget *widget,
                                     GtkAccessible *accessible )
{
    AtkObject * atk_obj;
    PtkIconViewAccessiblePrivate *priv;

    atk_obj = ATK_OBJECT ( accessible );
    priv = ptk_icon_view_accessible_get_priv ( atk_obj );
    if ( priv->old_hadj )
    {
        g_object_remove_weak_pointer ( G_OBJECT ( priv->old_hadj ),
                                       ( gpointer * ) & priv->old_hadj );

        g_signal_handlers_disconnect_by_func ( priv->old_hadj,
                                               ( gpointer ) ptk_icon_view_accessible_adjustment_changed,
                                               widget );
        priv->old_hadj = NULL;
    }
    if ( priv->old_vadj )
    {
        g_object_remove_weak_pointer ( G_OBJECT ( priv->old_vadj ),
                                       ( gpointer * ) & priv->old_vadj );

        g_signal_handlers_disconnect_by_func ( priv->old_vadj,
                                               ( gpointer ) ptk_icon_view_accessible_adjustment_changed,
                                               widget );
        priv->old_vadj = NULL;
    }
}

static void
ptk_icon_view_accessible_connect_widget_destroyed ( GtkAccessible *accessible )
{
    if ( accessible->widget )
    {
        g_signal_connect_after ( accessible->widget,
                                 "destroy",
                                 G_CALLBACK ( ptk_icon_view_accessible_destroyed ),
                                 accessible );
    }
    GTK_ACCESSIBLE_CLASS ( accessible_parent_class ) ->connect_widget_destroyed ( accessible );
}

static void
ptk_icon_view_accessible_class_init ( AtkObjectClass *klass )
{
    GObjectClass * gobject_class;
    GtkAccessibleClass *accessible_class;

    accessible_parent_class = g_type_class_peek_parent ( klass );

    gobject_class = ( GObjectClass * ) klass;
    accessible_class = ( GtkAccessibleClass * ) klass;

    gobject_class->finalize = ptk_icon_view_accessible_finalize;

    klass->get_n_children = ptk_icon_view_accessible_get_n_children;
    klass->ref_child = ptk_icon_view_accessible_ref_child;
    klass->initialize = ptk_icon_view_accessible_initialize;

    accessible_class->connect_widget_destroyed = ptk_icon_view_accessible_connect_widget_destroyed;

    accessible_private_data_quark = g_quark_from_static_string ( "icon_view-accessible-private-data" );
}

static AtkObject*
ptk_icon_view_accessible_ref_accessible_at_point ( AtkComponent *component,
                                                   gint x,
                                                   gint y,
                                                   AtkCoordType coord_type )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    gint x_pos, y_pos;

    widget = GTK_ACCESSIBLE ( component ) ->widget;
    if ( widget == NULL )
        /* State is defunct */
        return NULL;

    icon_view = PTK_ICON_VIEW ( widget );
    atk_component_get_extents ( component, &x_pos, &y_pos, NULL, NULL, coord_type );
    item = ptk_icon_view_get_item_at_coords ( icon_view, x - x_pos, y - y_pos, TRUE, NULL );
    if ( item )
        return ptk_icon_view_accessible_ref_child ( ATK_OBJECT ( component ), item->index );

    return NULL;
}

static void
atk_component_interface_init ( AtkComponentIface *iface )
{
    iface->ref_accessible_at_point = ptk_icon_view_accessible_ref_accessible_at_point;
}

static gboolean
ptk_icon_view_accessible_add_selection ( AtkSelection *selection,
                                         gint i )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    GList *l;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( widget );

    l = g_list_nth ( icon_view->priv->items, i );
    if ( !l )
        return FALSE;

    item = l->data;
    ptk_icon_view_select_item ( icon_view, item );

    return TRUE;
}

static gboolean
ptk_icon_view_accessible_clear_selection ( AtkSelection *selection )
{
    GtkWidget * widget;
    PtkIconView *icon_view;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( widget );
    ptk_icon_view_unselect_all ( icon_view );

    return TRUE;
}

static AtkObject*
ptk_icon_view_accessible_ref_selection ( AtkSelection *selection,
                                         gint i )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    GList *l;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return NULL;

    icon_view = PTK_ICON_VIEW ( widget );

    l = icon_view->priv->items;
    while ( l )
    {
        item = l->data;
        if ( item->selected )
        {
            if ( i == 0 )
                return atk_object_ref_accessible_child ( gtk_widget_get_accessible ( widget ), item->index );
            else
                i--;
        }
        l = l->next;
    }

    return NULL;
}

static gint
ptk_icon_view_accessible_get_selection_count ( AtkSelection *selection )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    GList *l;
    gint count;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return 0;

    icon_view = PTK_ICON_VIEW ( widget );

    l = icon_view->priv->items;
    count = 0;
    while ( l )
    {
        item = l->data;

        if ( item->selected )
            count++;

        l = l->next;
    }

    return count;
}

static gboolean
ptk_icon_view_accessible_is_child_selected ( AtkSelection *selection,
                                             gint i )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    GList *l;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( widget );
    l = g_list_nth ( icon_view->priv->items, i );
    if ( !l )
        return FALSE;

    item = l->data;

    return item->selected;
}

static gboolean
ptk_icon_view_accessible_remove_selection ( AtkSelection *selection,
                                            gint i )
{
    GtkWidget * widget;
    PtkIconView *icon_view;
    PtkIconViewItem *item;
    GList *l;
    gint count;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( widget );
    l = icon_view->priv->items;
    count = 0;
    while ( l )
    {
        item = l->data;
        if ( item->selected )
        {
            if ( count == i )
            {
                ptk_icon_view_unselect_item ( icon_view, item );
                return TRUE;
            }
            count++;
        }
        l = l->next;
    }

    return FALSE;
}

static gboolean
ptk_icon_view_accessible_select_all_selection ( AtkSelection *selection )
{
    GtkWidget * widget;
    PtkIconView *icon_view;

    widget = GTK_ACCESSIBLE ( selection ) ->widget;
    if ( widget == NULL )
        return FALSE;

    icon_view = PTK_ICON_VIEW ( widget );
    ptk_icon_view_select_all ( icon_view );
    return TRUE;
}

static void
ptk_icon_view_accessible_selection_interface_init ( AtkSelectionIface *iface )
{
    iface->add_selection = ptk_icon_view_accessible_add_selection;
    iface->clear_selection = ptk_icon_view_accessible_clear_selection;
    iface->ref_selection = ptk_icon_view_accessible_ref_selection;
    iface->get_selection_count = ptk_icon_view_accessible_get_selection_count;
    iface->is_child_selected = ptk_icon_view_accessible_is_child_selected;
    iface->remove_selection = ptk_icon_view_accessible_remove_selection;
    iface->select_all_selection = ptk_icon_view_accessible_select_all_selection;
}

static GType
ptk_icon_view_accessible_get_type ( void )
{
    static GType type = 0;

    if ( !type )
    {
        static GTypeInfo tinfo =
            {
                0,     /* class size */
                ( GBaseInitFunc ) NULL,     /* base init */
                ( GBaseFinalizeFunc ) NULL,     /* base finalize */
                ( GClassInitFunc ) ptk_icon_view_accessible_class_init,
                ( GClassFinalizeFunc ) NULL,     /* class finalize */
                NULL,     /* class data */
                0,     /* instance size */
                0,     /* nb preallocs */
                ( GInstanceInitFunc ) NULL,     /* instance init */
                NULL /* value table */
            };
        static const GInterfaceInfo atk_component_info =
            {
                ( GInterfaceInitFunc ) atk_component_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };
        static const GInterfaceInfo atk_selection_info =
            {
                ( GInterfaceInitFunc ) ptk_icon_view_accessible_selection_interface_init,
                ( GInterfaceFinalizeFunc ) NULL,
                NULL
            };

        /*
         * Figure out the size of the class and instance
         * we are deriving from
         */
        AtkObjectFactory *factory;
        GType derived_type;
        GTypeQuery query;
        GType derived_atk_type;

        derived_type = g_type_parent ( PTK_TYPE_ICON_VIEW );
        factory = atk_registry_get_factory ( atk_get_default_registry (),
                                             derived_type );
        derived_atk_type = atk_object_factory_get_accessible_type ( factory );
        g_type_query ( derived_atk_type, &query );
        tinfo.class_size = query.class_size;
        tinfo.instance_size = query.instance_size;

        type = g_type_register_static ( derived_atk_type,
                                        I_( "PtkIconViewAccessible" ),
                                        &tinfo, 0 );
        g_type_add_interface_static ( type, ATK_TYPE_COMPONENT,
                                      &atk_component_info );
        g_type_add_interface_static ( type, ATK_TYPE_SELECTION,
                                      &atk_selection_info );
    }
    return type;
}

static AtkObject *
ptk_icon_view_accessible_new ( GObject *obj )
{
    AtkObject * accessible;

    g_return_val_if_fail ( GTK_IS_WIDGET ( obj ), NULL );

    accessible = g_object_new ( ptk_icon_view_accessible_get_type (), NULL );
    atk_object_initialize ( accessible, obj );

    return accessible;
}

static GType
ptk_icon_view_accessible_factory_get_accessible_type ( void )
{
    return ptk_icon_view_accessible_get_type ();
}

static AtkObject*
ptk_icon_view_accessible_factory_create_accessible ( GObject *obj )
{
    return ptk_icon_view_accessible_new ( obj );
}

static void
ptk_icon_view_accessible_factory_class_init ( AtkObjectFactoryClass *klass )
{
    klass->create_accessible = ptk_icon_view_accessible_factory_create_accessible;
    klass->get_accessible_type = ptk_icon_view_accessible_factory_get_accessible_type;
}

static GType
ptk_icon_view_accessible_factory_get_type ( void )
{
    static GType type = 0;

    if ( !type )
    {
        static const GTypeInfo tinfo =
            {
                sizeof ( AtkObjectFactoryClass ),
                NULL,               /* base_init */
                NULL,               /* base_finalize */
                ( GClassInitFunc ) ptk_icon_view_accessible_factory_class_init,
                NULL,               /* class_finalize */
                NULL,               /* class_data */
                sizeof ( AtkObjectFactory ),
                0,                 /* n_preallocs */
                NULL, NULL
            };

        type = g_type_register_static ( ATK_TYPE_OBJECT_FACTORY,
                                        I_( "PtkIconViewAccessibleFactory" ),
                                        &tinfo, 0 );
    }
    return type;
}


static AtkObject *
ptk_icon_view_get_accessible ( GtkWidget *widget )
{
    static gboolean first_time = TRUE;

    if ( first_time )
    {
        AtkObjectFactory * factory;
        AtkRegistry *registry;
        GType derived_type;
        GType derived_atk_type;

        /*
         * Figure out whether accessibility is enabled by looking at the
         * type of the accessible object which would be created for
         * the parent type of PtkIconView.
         */
        derived_type = g_type_parent ( PTK_TYPE_ICON_VIEW );

        registry = atk_get_default_registry ();
        factory = atk_registry_get_factory ( registry,
                                             derived_type );
        derived_atk_type = atk_object_factory_get_accessible_type ( factory );
        if ( g_type_is_a ( derived_atk_type, GTK_TYPE_ACCESSIBLE ) )
            atk_registry_set_factory_type ( registry,
                                            PTK_TYPE_ICON_VIEW,
                                            ptk_icon_view_accessible_factory_get_type () );
        first_time = FALSE;
    }
    return ( * GTK_WIDGET_CLASS ( ptk_icon_view_parent_class ) ->get_accessible ) ( widget );
}



/*
 * Interactive search
 */

/**
 * ptk_icon_view_set_enable_search:
 * @icon_view: A #PtkIconView
 * @enable_search: %TRUE, if the user can search interactively
 *
 * If @enable_search is set, then the user can type in text to search through
 * the tree interactively (this is sometimes called "typeahead find").
 *
 * Note that even if this is %FALSE, the user can still initiate a search
 * using the "start-interactive-search" key binding.
 */
void
ptk_icon_view_set_enable_search ( PtkIconView *icon_view,
                                  gboolean enable_search )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    enable_search = !!enable_search;

    if ( icon_view->priv->enable_search != enable_search )
    {
        icon_view->priv->enable_search = enable_search;
        g_object_notify ( G_OBJECT ( icon_view ), "enable-search" );
    }
}

/**
 * ptk_icon_view_get_enable_search:
 * @icon_view: A #PtkIconView
 *
 * Returns whether or not the tree allows to start interactive searching
 * by typing in text.
 *
 * Return value: whether or not to let the user search interactively
 */
gboolean
ptk_icon_view_get_enable_search ( PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    return icon_view->priv->enable_search;
}


/**
 * ptk_icon_view_get_search_column:
 * @icon_view: A #PtkIconView
 *
 * Gets the column searched on by the interactive search code.
 *
 * Return value: the column the interactive search code searches in.
 */
gint
ptk_icon_view_get_search_column ( PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), -1 );

    return ( icon_view->priv->search_column );
}

/**
 * ptk_icon_view_set_search_column:
 * @icon_view: A #PtkIconView
 * @column: the column of the model to search in, or -1 to disable searching
 *
 * Sets @column as the column where the interactive search code should
 * search in.
 *
 * If the sort column is set, users can use the "start-interactive-search"
 * key binding to bring up search popup. The enable-search property controls
 * whether simply typing text will also start an interactive search.
 *
 * Note that @column refers to a column of the model.
 */
void
ptk_icon_view_set_search_column ( PtkIconView *icon_view,
                                  gint column )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( column >= -1 );

    if ( icon_view->priv->search_column == column )
        return ;

    icon_view->priv->search_column = column;
    g_object_notify ( G_OBJECT ( icon_view ), "search-column" );
}

/**
 * ptk_icon_view_get_search_equal_func:
 * @icon_view: A #PtkIconView
 *
 * Returns the compare function currently in use.
 *
 * Return value: the currently used compare function for the search code.
 */

PtkIconViewSearchEqualFunc
ptk_icon_view_get_search_equal_func ( PtkIconView *icon_view )
{
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), NULL );

    return icon_view->priv->search_equal_func;
}

/**
 * ptk_icon_view_set_search_equal_func:
 * @icon_view: A #PtkIconView
 * @search_equal_func: the compare function to use during the search
 * @search_user_data: user data to pass to @search_equal_func, or %NULL
 * @search_destroy: Destroy notifier for @search_user_data, or %NULL
 *
 * Sets the compare function for the interactive search capabilities; note
 * that somewhat like strcmp() returning 0 for equality
 * #PtkIconViewSearchEqualFunc returns %FALSE on matches.
 **/
void
ptk_icon_view_set_search_equal_func ( PtkIconView *icon_view,
                                      PtkIconViewSearchEqualFunc search_equal_func,
                                      gpointer search_user_data,
                                      GtkDestroyNotify search_destroy )
{
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );
    g_return_if_fail ( search_equal_func != NULL );

    if ( icon_view->priv->search_destroy )
        ( * icon_view->priv->search_destroy ) ( icon_view->priv->search_user_data );

    icon_view->priv->search_equal_func = search_equal_func;
    icon_view->priv->search_user_data = search_user_data;
    icon_view->priv->search_destroy = search_destroy;
    if ( icon_view->priv->search_equal_func == NULL )
        icon_view->priv->search_equal_func = ptk_icon_view_search_equal_func;
}

static void
ptk_icon_view_search_dialog_hide ( GtkWidget *search_dialog,
                                   PtkIconView *icon_view )
{
    if ( icon_view->priv->disable_popdown )
        return ;

    if ( icon_view->priv->search_entry_changed_id )
    {
        g_signal_handler_disconnect ( icon_view->priv->search_entry,
                                      icon_view->priv->search_entry_changed_id );
        icon_view->priv->search_entry_changed_id = 0;
    }
    if ( icon_view->priv->typeselect_flush_timeout )
    {
        g_source_remove ( icon_view->priv->typeselect_flush_timeout );
        icon_view->priv->typeselect_flush_timeout = 0;
    }

    /* send focus-in event */
    send_focus_change ( GTK_WIDGET ( icon_view->priv->search_entry ), FALSE );
    gtk_widget_hide ( search_dialog );
    gtk_entry_set_text ( GTK_ENTRY ( icon_view->priv->search_entry ), "" );
}

static void
ptk_icon_view_search_position_func ( PtkIconView *icon_view,
                                     GtkWidget *search_dialog )
{
    gint x, y;
    gint tree_x, tree_y;
    gint tree_width, tree_height;
    GdkWindow *tree_window = GTK_WIDGET ( icon_view ) ->window;
    GdkScreen *screen = gdk_drawable_get_screen ( tree_window );
    GtkRequisition requisition;
    gint monitor_num;
    GdkRectangle monitor;

    monitor_num = gdk_screen_get_monitor_at_window ( screen, tree_window );
    gdk_screen_get_monitor_geometry ( screen, monitor_num, &monitor );

    gtk_widget_realize ( search_dialog );

    gdk_window_get_origin ( tree_window, &tree_x, &tree_y );
    gdk_drawable_get_size ( tree_window,
                            &tree_width,
                            &tree_height );
    gtk_widget_size_request ( search_dialog, &requisition );

    if ( tree_x + tree_width - requisition.width > gdk_screen_get_width ( screen ) )
        x = gdk_screen_get_width ( screen ) - requisition.width;
    else if ( tree_x + tree_width - requisition.width < 0 )
        x = 0;
    else
        x = tree_x + tree_width - requisition.width;

    if ( tree_y + tree_height > gdk_screen_get_height ( screen ) )
        y = gdk_screen_get_height ( screen ) - requisition.height;
    else if ( tree_y + tree_height < 0 )     /* isn't really possible ... */
        y = 0;
    else
        y = tree_y + tree_height;

    gtk_window_move ( GTK_WINDOW ( search_dialog ), x, y );
}

static void
ptk_icon_view_search_disable_popdown ( GtkEntry *entry,
                                       GtkMenu *menu,
                                       gpointer data )
{
    PtkIconView * icon_view = ( PtkIconView * ) data;

    icon_view->priv->disable_popdown = 1;
    g_signal_connect ( menu, "hide",
                       G_CALLBACK ( ptk_icon_view_search_enable_popdown ), data );
}

static gboolean
ptk_icon_view_search_entry_flush_timeout ( PtkIconView *icon_view )
{
    GDK_THREADS_ENTER ();

    ptk_icon_view_search_dialog_hide ( icon_view->priv->search_window, icon_view );
    icon_view->priv->typeselect_flush_timeout = 0;

    GDK_THREADS_LEAVE ();

    return FALSE;
}

/* Because we're visible but offscreen, we just set a flag in the preedit
 * callback.
 */
static void
ptk_icon_view_search_preedit_changed ( GtkIMContext *im_context,
                                       PtkIconView *icon_view )
{
    icon_view->priv->imcontext_changed = 1;
    if ( icon_view->priv->typeselect_flush_timeout )
    {
        g_source_remove ( icon_view->priv->typeselect_flush_timeout );
        icon_view->priv->typeselect_flush_timeout =
            g_timeout_add ( PTK_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                            ( GSourceFunc ) ptk_icon_view_search_entry_flush_timeout,
                            icon_view );
    }

}

static void
ptk_icon_view_search_activate ( GtkEntry *entry,
                                PtkIconView *icon_view )
{
    GtkTreePath * path;

    ptk_icon_view_search_dialog_hide ( icon_view->priv->search_window,
                                       icon_view );

    if ( ptk_icon_view_get_cursor ( icon_view, &path, NULL ) )
    {
        ptk_icon_view_item_activated( icon_view, path );
        gtk_tree_path_free ( path );
    }
}

static gboolean
ptk_icon_view_real_search_enable_popdown ( gpointer data )
{
    PtkIconView * icon_view = ( PtkIconView * ) data;

    GDK_THREADS_ENTER ();

    icon_view->priv->disable_popdown = 0;

    GDK_THREADS_LEAVE ();

    return FALSE;
}

static void
ptk_icon_view_search_enable_popdown ( GtkWidget *widget,
                                      gpointer data )
{
    g_timeout_add ( 200, ptk_icon_view_real_search_enable_popdown, data );
}

static gboolean
ptk_icon_view_search_delete_event ( GtkWidget *widget,
                                    GdkEventAny *event,
                                    PtkIconView *icon_view )
{
    g_return_val_if_fail ( GTK_IS_WIDGET ( widget ), FALSE );

    ptk_icon_view_search_dialog_hide ( widget, icon_view );

    return TRUE;
}

static gboolean
ptk_icon_view_search_button_press_event ( GtkWidget *widget,
                                          GdkEventButton *event,
                                          PtkIconView *icon_view )
{
    g_return_val_if_fail ( GTK_IS_WIDGET ( widget ), FALSE );

    ptk_icon_view_search_dialog_hide ( widget, icon_view );

    if ( event->window == icon_view->priv->bin_window )
        ptk_icon_view_button_press_event ( GTK_WIDGET ( icon_view ), event );

    return TRUE;
}

static gboolean
ptk_icon_view_search_scroll_event ( GtkWidget *widget,
                                    GdkEventScroll *event,
                                    PtkIconView *icon_view )
{
    gboolean retval = FALSE;

    if ( event->direction == GDK_SCROLL_UP )
    {
        ptk_icon_view_search_move ( widget, icon_view, TRUE );
        retval = TRUE;
    }
    else if ( event->direction == GDK_SCROLL_DOWN )
    {
        ptk_icon_view_search_move ( widget, icon_view, FALSE );
        retval = TRUE;
    }

    return retval;
}

static gboolean
ptk_icon_view_search_key_press_event ( GtkWidget *widget,
                                       GdkEventKey *event,
                                       PtkIconView *icon_view )
{
    gboolean retval = FALSE;

    g_return_val_if_fail ( GTK_IS_WIDGET ( widget ), FALSE );
    g_return_val_if_fail ( PTK_IS_ICON_VIEW ( icon_view ), FALSE );

    /* close window and cancel the search */
    if ( event->keyval == GDK_Escape ||
            event->keyval == GDK_Tab ||
            event->keyval == GDK_KP_Tab ||
            event->keyval == GDK_ISO_Left_Tab )
    {
        ptk_icon_view_search_dialog_hide ( widget, icon_view );
        return TRUE;
    }

    /* select previous matching iter */
    if ( event->keyval == GDK_Up || event->keyval == GDK_KP_Up )
    {
        ptk_icon_view_search_move ( widget, icon_view, TRUE );
        retval = TRUE;
    }

    if ( ( ( event->state & ( GDK_CONTROL_MASK | GDK_SHIFT_MASK ) ) == ( GDK_CONTROL_MASK | GDK_SHIFT_MASK ) )
            && ( event->keyval == GDK_g || event->keyval == GDK_G ) )
    {
        ptk_icon_view_search_move ( widget, icon_view, TRUE );
        retval = TRUE;
    }

    /* select next matching iter */
    if ( event->keyval == GDK_Down || event->keyval == GDK_KP_Down )
    {
        ptk_icon_view_search_move ( widget, icon_view, FALSE );
        retval = TRUE;
    }

    if ( ( ( event->state & ( GDK_CONTROL_MASK | GDK_SHIFT_MASK ) ) == GDK_CONTROL_MASK )
            && ( event->keyval == GDK_g || event->keyval == GDK_G ) )
    {
        ptk_icon_view_search_move ( widget, icon_view, FALSE );
        retval = TRUE;
    }

    /* renew the flush timeout */
    if ( retval && icon_view->priv->typeselect_flush_timeout )
    {
        g_source_remove ( icon_view->priv->typeselect_flush_timeout );
        icon_view->priv->typeselect_flush_timeout =
            g_timeout_add ( PTK_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                            ( GSourceFunc ) ptk_icon_view_search_entry_flush_timeout,
                            icon_view );
    }

    return retval;
}

static void
ptk_icon_view_search_move ( GtkWidget *window,
                            PtkIconView *icon_view,
                            gboolean up )
{
    gboolean ret;
    gint len;
    gint count = 0;
    const gchar *text;
    GtkTreeIter iter;
    GtkTreeModel *model;

    text = gtk_entry_get_text ( GTK_ENTRY ( icon_view->priv->search_entry ) );

    g_return_if_fail ( text != NULL );

    if ( up && icon_view->priv->selected_iter == 1 )
        return ;

    len = strlen ( text );

    if ( len < 1 )
        return ;

    model = ptk_icon_view_get_model ( icon_view );

    /* search */
    ptk_icon_view_unselect_all ( icon_view );
    if ( !gtk_tree_model_get_iter_first ( model, &iter ) )
        return ;

    ret = ptk_icon_view_search_iter ( icon_view, &iter, text,
                                      &count, up ? ( ( icon_view->priv->selected_iter ) - 1 ) : ( ( icon_view->priv->selected_iter + 1 ) ) );

    if ( ret )
    {
        /* found */
        icon_view->priv->selected_iter += up ? ( -1 ) : ( 1 );
    }
    else
    {
        /* return to old iter */
        count = 0;
        gtk_tree_model_get_iter_first ( model, &iter );
        ptk_icon_view_search_iter ( icon_view,
                                    &iter, text,
                                    &count, icon_view->priv->selected_iter );
    }
}

static gboolean
ptk_icon_view_search_equal_func ( GtkTreeModel *model,
                                  gint column,
                                  const gchar *key,
                                  GtkTreeIter *iter,
                                  gpointer search_data )
{
    gboolean retval = TRUE;
    const gchar *str;
    gchar *normalized_string;
    gchar *normalized_key;
    gchar *case_normalized_string = NULL;
    gchar *case_normalized_key = NULL;
    GValue value = {0, };
    GValue transformed = {0, };

    gtk_tree_model_get_value ( model, iter, column, &value );

    g_value_init ( &transformed, G_TYPE_STRING );

    if ( !g_value_transform ( &value, &transformed ) )
    {
        g_value_unset ( &value );
        return TRUE;
    }

    g_value_unset ( &value );

    str = g_value_get_string ( &transformed );
    if ( !str )
    {
        g_value_unset ( &transformed );
        return TRUE;
    }

    normalized_string = g_utf8_normalize ( str, -1, G_NORMALIZE_ALL );
    normalized_key = g_utf8_normalize ( key, -1, G_NORMALIZE_ALL );

    if ( normalized_string && normalized_key )
    {
        case_normalized_string = g_utf8_casefold ( normalized_string, -1 );
        case_normalized_key = g_utf8_casefold ( normalized_key, -1 );

        if ( strncmp ( case_normalized_key, case_normalized_string, strlen ( case_normalized_key ) ) == 0 )
            retval = FALSE;
    }

    g_value_unset ( &transformed );
    g_free ( normalized_key );
    g_free ( normalized_string );
    g_free ( case_normalized_key );
    g_free ( case_normalized_string );

    return retval;
}

static gboolean
ptk_icon_view_search_iter ( PtkIconView *icon_view,
                            GtkTreeIter *iter,
                            const gchar *text,
                            gint *count,
                            gint n )
{
    GtkTreeModel * model = ptk_icon_view_get_model( icon_view );
    GtkTreePath *path;

    path = gtk_tree_model_get_path ( model, iter );
    do
    {
        if ( ! ( *icon_view->priv->search_equal_func ) ( model, icon_view->priv->search_column, text, iter, icon_view->priv->search_user_data ) )
        {
            ( *count ) ++;
            if ( *count == n )
            {
                ptk_icon_view_scroll_to_path ( icon_view, path, TRUE, 0.5, 0.0 );
                ptk_icon_view_set_cursor ( icon_view, path, NULL, FALSE );
                ptk_icon_view_select_path( icon_view, path );

                if ( path )
                    gtk_tree_path_free ( path );

                return TRUE;
            }
        }
        gtk_tree_path_next ( path );
    }
    while ( gtk_tree_model_iter_next ( model, iter ) );

    if ( path )
        gtk_tree_path_free ( path );

    return FALSE;
}

static void
ptk_icon_view_search_init ( GtkWidget *entry,
                            PtkIconView *icon_view )
{
    gint ret;
    gint len;
    gint count = 0;
    const gchar *text;
    GtkTreeIter iter;
    GtkTreeModel *model;

    g_return_if_fail ( GTK_IS_ENTRY ( entry ) );
    g_return_if_fail ( PTK_IS_ICON_VIEW ( icon_view ) );

    text = gtk_entry_get_text ( GTK_ENTRY ( entry ) );
    len = strlen ( text );

    /* search */
    ptk_icon_view_unselect_all ( icon_view );
    if ( icon_view->priv->typeselect_flush_timeout )
    {
        g_source_remove ( icon_view->priv->typeselect_flush_timeout );
        icon_view->priv->typeselect_flush_timeout =
            g_timeout_add ( PTK_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                            ( GSourceFunc ) ptk_icon_view_search_entry_flush_timeout,
                            icon_view );
    }

    if ( len < 1 )
        return ;

    model = ptk_icon_view_get_model( icon_view );
    if ( !gtk_tree_model_get_iter_first ( model, &iter ) )
        return ;

    ret = ptk_icon_view_search_iter ( icon_view,
                                      &iter, text,
                                      &count, 1 );

    if ( ret )
        icon_view->priv->selected_iter = 1;
}

/* Cut and paste from gtktreeview.c */
static void
send_focus_change ( GtkWidget *widget,
                    gboolean in )
{
    GdkEvent * fevent = gdk_event_new ( GDK_FOCUS_CHANGE );

    g_object_ref ( widget );

    if ( in )
        GTK_WIDGET_SET_FLAGS ( widget, GTK_HAS_FOCUS );
    else
        GTK_WIDGET_UNSET_FLAGS ( widget, GTK_HAS_FOCUS );

    fevent->focus_change.type = GDK_FOCUS_CHANGE;
    fevent->focus_change.window = g_object_ref ( widget->window );
    fevent->focus_change.in = in;

    gtk_widget_event ( widget, fevent );

    g_object_notify ( G_OBJECT ( widget ), "has-focus" );

    g_object_unref ( widget );
    gdk_event_free ( fevent );
}

static void
ptk_icon_view_ensure_interactive_directory ( PtkIconView *icon_view )
{
    GtkWidget * frame, *vbox, *toplevel;

    toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( icon_view ) );

    if ( icon_view->priv->search_window != NULL )
    {
        if ( GTK_WINDOW ( toplevel ) ->group )
            gtk_window_group_add_window ( GTK_WINDOW ( toplevel ) ->group,
                                          GTK_WINDOW ( icon_view->priv->search_window ) );
        else if ( GTK_WINDOW ( icon_view->priv->search_window ) ->group )
            gtk_window_group_remove_window ( GTK_WINDOW ( icon_view->priv->search_window ) ->group,
                                             GTK_WINDOW ( icon_view->priv->search_window ) );
        return ;
    }

    icon_view->priv->search_window = gtk_window_new ( GTK_WINDOW_POPUP );

    if ( GTK_WINDOW ( toplevel ) ->group )
        gtk_window_group_add_window ( GTK_WINDOW ( toplevel ) ->group,
                                      GTK_WINDOW ( icon_view->priv->search_window ) );

    gtk_window_set_modal ( GTK_WINDOW ( icon_view->priv->search_window ), TRUE );
    g_signal_connect ( icon_view->priv->search_window, "delete_event",
                       G_CALLBACK ( ptk_icon_view_search_delete_event ),
                       icon_view );
    g_signal_connect ( icon_view->priv->search_window, "key_press_event",
                       G_CALLBACK ( ptk_icon_view_search_key_press_event ),
                       icon_view );
    g_signal_connect ( icon_view->priv->search_window, "button_press_event",
                       G_CALLBACK ( ptk_icon_view_search_button_press_event ),
                       icon_view );
    g_signal_connect ( icon_view->priv->search_window, "scroll_event",
                       G_CALLBACK ( ptk_icon_view_search_scroll_event ),
                       icon_view );

    frame = gtk_frame_new ( NULL );
    gtk_frame_set_shadow_type ( GTK_FRAME ( frame ), GTK_SHADOW_ETCHED_IN );
    gtk_widget_show ( frame );
    gtk_container_add ( GTK_CONTAINER ( icon_view->priv->search_window ), frame );

    vbox = gtk_vbox_new ( FALSE, 0 );
    gtk_widget_show ( vbox );
    gtk_container_add ( GTK_CONTAINER ( frame ), vbox );
    gtk_container_set_border_width ( GTK_CONTAINER ( vbox ), 3 );

    /* add entry */
    icon_view->priv->search_entry = gtk_entry_new ();
    gtk_widget_show ( icon_view->priv->search_entry );
    g_signal_connect ( icon_view->priv->search_entry, "populate_popup",
                       G_CALLBACK ( ptk_icon_view_search_disable_popdown ),
                       icon_view );
    g_signal_connect ( icon_view->priv->search_entry,
                       "activate", G_CALLBACK ( ptk_icon_view_search_activate ),
                       icon_view );
    g_signal_connect ( GTK_ENTRY ( icon_view->priv->search_entry ) ->im_context,
                       "preedit-changed",
                       G_CALLBACK ( ptk_icon_view_search_preedit_changed ),
                       icon_view );
    gtk_container_add ( GTK_CONTAINER ( vbox ),
                        icon_view->priv->search_entry );

    gtk_widget_realize ( icon_view->priv->search_entry );
}


/* Pops up the interactive search entry.  If keybinding is TRUE then the user
 * started this by typing the start_interactive_search keybinding.  Otherwise, it came from
 */
static gboolean
ptk_icon_view_real_start_interactive_search ( PtkIconView *icon_view,
                                              gboolean keybinding )
{
    /* We only start interactive search if we have focus or the columns
    * have focus.  If one of our children have focus, we don't want to
    * start the search.
    */
    GList * list;
    gboolean found_focus = FALSE;
    GtkWidgetClass *entry_parent_class;

    if ( !icon_view->priv->enable_search && !keybinding )
        return FALSE;

    if ( icon_view->priv->search_window != NULL &&
            GTK_WIDGET_VISIBLE ( icon_view->priv->search_window ) )
        return TRUE;

#if 0 /* What's this??? */

    for ( list = icon_view->priv->columns; list; list = list->next )
    {
        PtkIconViewColumn * column;

        column = list->data;
        if ( ! column->visible )
            continue;

        if ( GTK_WIDGET_HAS_FOCUS ( column->button ) )
        {
            found_focus = TRUE;
            break;
        }
    }
#endif

    if ( GTK_WIDGET_HAS_FOCUS ( icon_view ) )
        found_focus = TRUE;

    if ( !found_focus )
        return FALSE;

    if ( icon_view->priv->search_column < 0 )
        return FALSE;

    ptk_icon_view_ensure_interactive_directory ( icon_view );

    if ( keybinding )
        gtk_entry_set_text ( GTK_ENTRY ( icon_view->priv->search_entry ), "" );

    /* done, show it */
    icon_view->priv->search_dialog_position_func ( icon_view, icon_view->priv->search_window );
    gtk_widget_show ( icon_view->priv->search_window );
    if ( icon_view->priv->search_entry_changed_id == 0 )
    {
        icon_view->priv->search_entry_changed_id =
            g_signal_connect ( icon_view->priv->search_entry, "changed",
                               G_CALLBACK ( ptk_icon_view_search_init ),
                               icon_view );
    }

    icon_view->priv->typeselect_flush_timeout =
        g_timeout_add ( PTK_ICON_VIEW_SEARCH_DIALOG_TIMEOUT,
                        ( GSourceFunc ) ptk_icon_view_search_entry_flush_timeout,
                        icon_view );

    /* Grab focus will select all the text.  We don't want that to happen, so we
    * call the parent instance and bypass the selection change.  This is probably
    * really non-kosher. */
    entry_parent_class = g_type_class_peek_parent ( GTK_ENTRY_GET_CLASS ( icon_view->priv->search_entry ) );
    ( entry_parent_class->grab_focus ) ( icon_view->priv->search_entry );

    /* send focus-in event */
    send_focus_change ( icon_view->priv->search_entry, TRUE );

    /* search first matching iter */
    ptk_icon_view_search_init ( icon_view->priv->search_entry, icon_view );

    return TRUE;
}

static gboolean
ptk_icon_view_start_interactive_search ( PtkIconView *icon_view )
{
    return ptk_icon_view_real_start_interactive_search ( icon_view, TRUE );
}

static gboolean
ptk_icon_view_key_press ( GtkWidget *widget,
                          GdkEventKey *event )
{
    PtkIconView * icon_view = ( PtkIconView * ) widget;
    GtkWidgetClass* parent_class;
    GList *list;

    /* Chain up to the parent class.  It handles the keybindings. */
    parent_class = g_type_class_peek_parent( PTK_ICON_VIEW_GET_CLASS( icon_view ) );
    if ( ( *parent_class->key_press_event ) ( widget, event ) )
        return TRUE;

    /* We pass the event to the search_entry.  If its text changes, then we start
    * the typeahead find capabilities. */
    if ( icon_view->priv->enable_search )
    {
        GdkEvent * new_event;
        char *old_text;
        const char *new_text;
        gboolean retval;
        GdkScreen *screen;
        gboolean text_modified;
        gulong popup_menu_id;

        ptk_icon_view_ensure_interactive_directory ( icon_view );

        /* Make a copy of the current text */
        old_text = g_strdup ( gtk_entry_get_text ( GTK_ENTRY ( icon_view->priv->search_entry ) ) );
        new_event = gdk_event_copy ( ( GdkEvent * ) event );
        g_object_unref ( ( ( GdkEventKey * ) new_event ) ->window );
        ( ( GdkEventKey * ) new_event ) ->window = g_object_ref ( icon_view->priv->search_window->window );
        gtk_widget_realize ( icon_view->priv->search_window );

        popup_menu_id = g_signal_connect ( icon_view->priv->search_entry,
                                           "popup_menu", G_CALLBACK ( gtk_true ), NULL );

        /* Move the entry off screen */
        screen = gtk_widget_get_screen ( GTK_WIDGET ( icon_view ) );
        gtk_window_move ( GTK_WINDOW ( icon_view->priv->search_window ),
                          gdk_screen_get_width ( screen ) + 1,
                          gdk_screen_get_height ( screen ) + 1 );
        gtk_widget_show ( icon_view->priv->search_window );

        /* Send the event to the window.  If the preedit_changed signal is emitted
        * during this event, we will set priv->imcontext_changed  */
        icon_view->priv->imcontext_changed = FALSE;
        retval = gtk_widget_event ( icon_view->priv->search_window, new_event );
        gdk_event_free ( new_event );
        gtk_widget_hide ( icon_view->priv->search_window );

        g_signal_handler_disconnect ( icon_view->priv->search_entry,
                                      popup_menu_id );

        /* We check to make sure that the entry tried to handle the text, and that
        * the text has changed.
        */
        new_text = gtk_entry_get_text ( GTK_ENTRY ( icon_view->priv->search_entry ) );
        text_modified = strcmp ( old_text, new_text ) != 0;
        g_free ( old_text );
        if ( icon_view->priv->imcontext_changed ||       /* we're in a preedit */
                ( retval && text_modified ) )                  /* ...or the text was modified */
        {
            if ( ptk_icon_view_real_start_interactive_search ( icon_view, FALSE ) )
            {
                gtk_widget_grab_focus ( GTK_WIDGET ( icon_view ) );
                return TRUE;
            }
            else
            {
                gtk_entry_set_text ( GTK_ENTRY ( icon_view->priv->search_entry ), "" );
                return FALSE;
            }
        }
    }

    return FALSE;
}

static gboolean
ptk_icon_view_key_release ( GtkWidget *widget,
                            GdkEventKey *event )
{
    GtkWidgetClass * parent_class;
    parent_class = g_type_class_peek_parent( PTK_ICON_VIEW_GET_CLASS( widget ) );
    return ( * GTK_WIDGET_CLASS ( parent_class ) ->key_release_event ) ( widget, event );
}

void ptk_icon_view_set_background ( PtkIconView *icon_view, GdkPixmap* background, int x_off, int y_off )
{
    if ( icon_view->priv->pixmap )
        g_object_unref( G_OBJECT( icon_view->priv->pixmap ) );
    icon_view->priv->pixmap = background ? GDK_PIXMAP( g_object_ref( background ) ) : NULL;
    icon_view->priv->pixmap_x_offset = x_off;
    icon_view->priv->pixmap_y_offset = y_off;
}

GdkPixmap* ptk_icon_view_get_background ( PtkIconView *icon_view )
{
    return icon_view->priv->pixmap;
}


#define __PTK_ICON_VIEW_C__

