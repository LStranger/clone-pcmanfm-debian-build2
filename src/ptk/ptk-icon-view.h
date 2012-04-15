/* $Id: ptk-icon-view.h 491 2008-02-13 05:02:19Z pcmanx $ */
/*-
 * Copyright (c) 2004-2005  os-cillation e.K.
 * Copyright (c) 2002,2004  Anders Carlsson <andersca@gnu.org>
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#ifndef __PTK_ICON_VIEW_H__
#define __PTK_ICON_VIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PtkIconViewPrivate PtkIconViewPrivate;
typedef struct _PtkIconViewClass PtkIconViewClass;
typedef struct _PtkIconView PtkIconView;

#define PTK_TYPE_ICON_VIEW            (ptk_icon_view_get_type ())
#define PTK_ICON_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PTK_TYPE_ICON_VIEW, PtkIconView))
#define PTK_ICON_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PTK_TYPE_ICON_VIEW, PtkIconViewClass))
#define PTK_IS_ICON_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_ICON_VIEW))
#define PTK_IS_ICON_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PTK_TYPE_ICON_VIEW))
#define PTK_ICON_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PTK_TYPE_ICON_VIEW, PtkIconViewClass))

/**
 * PtkIconViewForeachFunc:
 * @icon_view :
 * @path      :
 * @user_data :
 **/
typedef void ( *PtkIconViewForeachFunc ) ( PtkIconView *icon_view,
                                           GtkTreePath *path,
                                           gpointer user_data );

typedef gboolean ( *PtkIconViewSearchEqualFunc ) ( GtkTreeModel *model,
                                                   gint column,
                                                   const gchar *key,
                                                   GtkTreeIter *iter,
                                                   gpointer search_data );

/**
 * PtkIconViewDropPosition:
 * @PTK_ICON_VIEW_NO_DROP    :
 * @PTK_ICON_VIEW_DROP_LEFT  :
 * @PTK_ICON_VIEW_DROP_RIGHT :
 * @PTK_ICON_VIEW_DROP_ABOVE :
 * @PTK_ICON_VIEW_DROP_BELOW :
 **/
typedef enum
{
    PTK_ICON_VIEW_NO_DROP,
    PTK_ICON_VIEW_DROP_INTO,
    PTK_ICON_VIEW_DROP_LEFT,
    PTK_ICON_VIEW_DROP_RIGHT,
    PTK_ICON_VIEW_DROP_ABOVE,
    PTK_ICON_VIEW_DROP_BELOW
} PtkIconViewDropPosition;

struct _PtkIconView
{
    GtkContainer __parent__;

    /*< private >*/
    PtkIconViewPrivate *priv;
};

struct _PtkIconViewClass
{
    GtkContainerClass __parent__;

    /* virtual methods */
    void ( *set_scroll_adjustments ) ( PtkIconView *icon_view,
                                       GtkAdjustment *hadjustment,
                                       GtkAdjustment *vadjustment );

    /* signals */
    void ( *item_activated ) ( PtkIconView *icon_view,
                               GtkTreePath *path );
    void ( *selection_changed ) ( PtkIconView *icon_view );

    /* Key binding signals */
    void ( *select_all ) ( PtkIconView *icon_view );
    void ( *unselect_all ) ( PtkIconView *icon_view );
    void ( *select_cursor_item ) ( PtkIconView *icon_view );
    void ( *toggle_cursor_item ) ( PtkIconView *icon_view );
    gboolean ( *move_cursor ) ( PtkIconView *icon_view,
                                GtkMovementStep step,
                                gint count );
    gboolean ( *activate_cursor_item ) ( PtkIconView *icon_view );

    /*< private >*/
    void ( *reserved1 ) ( void );
    void ( *reserved2 ) ( void );
    void ( *reserved3 ) ( void );
    void ( *reserved4 ) ( void );
};

GType ptk_icon_view_get_type ( void ) G_GNUC_CONST;

GtkWidget *ptk_icon_view_new ( void );
GtkWidget *ptk_icon_view_new_with_model ( GtkTreeModel *model );

GtkTreeModel *ptk_icon_view_get_model ( const PtkIconView *icon_view );
void ptk_icon_view_set_model ( PtkIconView *icon_view, GtkTreeModel *model );

gint ptk_icon_view_get_text_column ( const PtkIconView *icon_view );
void ptk_icon_view_set_text_column ( PtkIconView *icon_view,
                                     gint column );

gint ptk_icon_view_get_markup_column ( const PtkIconView *icon_view );
void ptk_icon_view_set_markup_column ( PtkIconView *icon_view,
                                       gint column );

gint ptk_icon_view_get_pixbuf_column ( const PtkIconView *icon_view );
void ptk_icon_view_set_pixbuf_column ( PtkIconView *icon_view,
                                       gint column );

GtkOrientation ptk_icon_view_get_orientation ( const PtkIconView *icon_view );
void ptk_icon_view_set_orientation ( PtkIconView *icon_view,
                                     GtkOrientation orientation );

gint ptk_icon_view_get_columns ( const PtkIconView *icon_view );
void ptk_icon_view_set_columns ( PtkIconView *icon_view,
                                 gint columns );

gint ptk_icon_view_get_item_width ( const PtkIconView *icon_view );
void ptk_icon_view_set_item_width ( PtkIconView *icon_view,
                                    gint item_width );

gint ptk_icon_view_get_spacing ( const PtkIconView *icon_view );
void ptk_icon_view_set_spacing ( PtkIconView *icon_view,
                                 gint spacing );

gint ptk_icon_view_get_row_spacing ( const PtkIconView *icon_view );
void ptk_icon_view_set_row_spacing ( PtkIconView *icon_view,
                                     gint row_spacing );

gint ptk_icon_view_get_column_spacing ( const PtkIconView *icon_view );
void ptk_icon_view_set_column_spacing ( PtkIconView *icon_view,
                                        gint column_spacing );

gint ptk_icon_view_get_margin ( const PtkIconView *icon_view );
void ptk_icon_view_set_margin ( PtkIconView *icon_view,
                                gint margin );

GtkSelectionMode ptk_icon_view_get_selection_mode ( const PtkIconView *icon_view );
void ptk_icon_view_set_selection_mode ( PtkIconView *icon_view,
                                        GtkSelectionMode mode );

void ptk_icon_view_widget_to_icon_coords ( const PtkIconView *icon_view,
                                           gint wx,
                                           gint wy,
                                           gint *ix,
                                           gint *iy );
void ptk_icon_view_icon_to_widget_coords ( const PtkIconView *icon_view,
                                           gint ix,
                                           gint iy,
                                           gint *wx,
                                           gint *wy );

GtkTreePath *ptk_icon_view_get_path_at_pos ( const PtkIconView *icon_view,
                                             gint x,
                                             gint y );
gboolean ptk_icon_view_get_item_at_pos ( const PtkIconView *icon_view,
                                         gint x,
                                         gint y,
                                         GtkTreePath **path,
                                         GtkCellRenderer **cell );

gboolean ptk_icon_view_get_visible_range ( const PtkIconView *icon_view,
                                           GtkTreePath **start_path,
                                           GtkTreePath **end_path );

void ptk_icon_view_selected_foreach ( PtkIconView *icon_view,
                                      PtkIconViewForeachFunc func,
                                      gpointer data );
void ptk_icon_view_select_path ( PtkIconView *icon_view,
                                 GtkTreePath *path );
void ptk_icon_view_unselect_path ( PtkIconView *icon_view,
                                   GtkTreePath *path );
gboolean ptk_icon_view_path_is_selected ( const PtkIconView *icon_view,
                                          GtkTreePath *path );
GList *ptk_icon_view_get_selected_items ( const PtkIconView *icon_view );
void ptk_icon_view_select_all ( PtkIconView *icon_view );
void ptk_icon_view_unselect_all ( PtkIconView *icon_view );
void ptk_icon_view_item_activated ( PtkIconView *icon_view,
                                    GtkTreePath *path );

gboolean ptk_icon_view_get_cursor ( const PtkIconView *icon_view,
                                    GtkTreePath **path,
                                    GtkCellRenderer **cell );
void ptk_icon_view_set_cursor ( PtkIconView *icon_view,
                                GtkTreePath *path,
                                GtkCellRenderer *cell,
                                gboolean start_editing );

void ptk_icon_view_scroll_to_path ( PtkIconView *icon_view,
                                    GtkTreePath *path,
                                    gboolean use_align,
                                    gfloat row_align,
                                    gfloat col_align );

/* Drag-and-Drop support */
void ptk_icon_view_enable_model_drag_source ( PtkIconView *icon_view,
                                              GdkModifierType start_button_mask,
                                              const GtkTargetEntry *targets,
                                              gint n_targets,
                                              GdkDragAction actions );
void ptk_icon_view_enable_model_drag_dest ( PtkIconView *icon_view,
                                            const GtkTargetEntry *targets,
                                            gint n_targets,
                                            GdkDragAction actions );
void ptk_icon_view_unset_model_drag_source ( PtkIconView *icon_view );
void ptk_icon_view_unset_model_drag_dest ( PtkIconView *icon_view );
void ptk_icon_view_set_reorderable ( PtkIconView *icon_view,
                                     gboolean reorderable );
gboolean ptk_icon_view_get_reorderable ( PtkIconView *icon_view );


/* These are useful to implement your own custom stuff. */
void ptk_icon_view_set_drag_dest_item ( PtkIconView *icon_view,
                                        GtkTreePath *path,
                                        PtkIconViewDropPosition pos );
void ptk_icon_view_get_drag_dest_item ( PtkIconView *icon_view,
                                        GtkTreePath **path,
                                        PtkIconViewDropPosition *pos );
gboolean ptk_icon_view_get_dest_item_at_pos ( PtkIconView *icon_view,
                                              gint drag_x,
                                              gint drag_y,
                                              GtkTreePath **path,
                                              PtkIconViewDropPosition *pos );
GdkPixmap *ptk_icon_view_create_drag_icon ( PtkIconView *icon_view,
                                            GtkTreePath *path );


/* Interactive search */
void ptk_icon_view_set_enable_search ( PtkIconView *icon_view,
                                       gboolean enable_search );

gboolean ptk_icon_view_get_enable_search ( PtkIconView *icon_view );
gint ptk_icon_view_get_search_column ( PtkIconView *icon_view );
void ptk_icon_view_set_search_column ( PtkIconView *icon_view,
                                       gint column );

PtkIconViewSearchEqualFunc ptk_icon_view_get_search_equal_func ( PtkIconView *icon_view );

void ptk_icon_view_set_search_equal_func ( PtkIconView *icon_view,
                                           PtkIconViewSearchEqualFunc search_equal_func,
                                           gpointer search_user_data,
                                           GtkDestroyNotify search_destroy );

void ptk_icon_view_set_background ( PtkIconView *icon_view, GdkPixmap* background, int x_off, int y_off );

GdkPixmap* ptk_icon_view_get_background ( PtkIconView *icon_view );

G_END_DECLS

#endif /* __PTK_ICON_VIEW_H__ */
