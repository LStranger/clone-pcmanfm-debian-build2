/*
*  C Implementation: ptkfileiconrenderer
*
* Description: PtkFileIconRenderer is used to render file icons
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
* Part of this class is taken from GtkCellRendererPixbuf written by
* Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
*
*/


#include "ptk-file-icon-renderer.h"

static void
ptk_file_icon_renderer_init       (PtkFileIconRenderer      *renderer);

static void
ptk_file_icon_renderer_class_init (PtkFileIconRendererClass *klass);

static void
ptk_file_icon_renderer_get_property  (GObject                    *object,
                                      guint                       param_id,
                                      GValue                     *value,
                                      GParamSpec                 *pspec);

static void
ptk_file_icon_renderer_set_property  (GObject                    *object,
                                      guint                       param_id,
                                      const GValue               *value,
                                      GParamSpec                 *pspec);

static void
ptk_file_icon_renderer_finalize (GObject *gobject);

/*
static void
ptk_file_icon_renderer_get_size   (GtkCellRenderer            *cell,
                                   GtkWidget                  *widget,
                                   GdkRectangle               *cell_area,
                                   gint                       *x_offset,
                                   gint                       *y_offset,
                                   gint                       *width,
                                   gint                       *height);
*/

static void
ptk_file_icon_renderer_render     (GtkCellRenderer            *cell,
                                   GdkWindow                  *window,
                                   GtkWidget                  *widget,
                                   GdkRectangle               *background_area,
                                   GdkRectangle               *cell_area,
                                   GdkRectangle               *expose_area,
                                   guint                       flags);

enum
{
  PROP_STAT = 1,
  PROP_FLAGS,
  PROP_FOLLOW_STATE
};

static   gpointer parent_class;

static GdkPixbuf* link_icon_large = NULL;
static GdkPixbuf* link_icon_small = NULL;

/***************************************************************************
 *
 *  ptk_file_icon_renderer_get_type: here we register our type with
 *                                          the GObject type system if we
 *                                          haven't done so yet. Everything
 *                                          else is done in the callbacks.
 *
 ***************************************************************************/

GType
ptk_file_icon_renderer_get_type (void)
{
  static GType renderer_type = 0;
  if ( G_UNLIKELY(!renderer_type) )
  {
    static const GTypeInfo renderer_info =
    {
      sizeof (PtkFileIconRendererClass),
      NULL,                                                     /* base_init */
      NULL,                                                     /* base_finalize */
      (GClassInitFunc) ptk_file_icon_renderer_class_init,
      NULL,                                                     /* class_finalize */
      NULL,                                                     /* class_data */
      sizeof (PtkFileIconRenderer),
      0,                                                        /* n_preallocs */
      (GInstanceInitFunc) ptk_file_icon_renderer_init,
    };

    /* Derive from GtkCellRendererPixbuf */
    renderer_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_PIXBUF,
                                            "PtkFileIconRenderer",
                                            &renderer_info,
                                            0);
  }

  return renderer_type;
}


/***************************************************************************
 *
 *  ptk_file_icon_renderer_init: set some default properties of the
 *                                      parent (GtkCellRendererPixbuf).
 *
 ***************************************************************************/

static void
ptk_file_icon_renderer_init (PtkFileIconRenderer *renderer)
{
  GtkIconTheme *icon_theme;
  if( !link_icon_large ) {
    icon_theme = gtk_icon_theme_get_default();
    link_icon_large = gtk_icon_theme_load_icon (icon_theme, "emblem-symbolic-link", 32, 0, NULL );
    /*
    link_icon_small = gtk_icon_theme_load_icon (icon_theme, "emblem-symbolic-link", 16, 0, NULL );
    */
  }
}


/***************************************************************************
 *
 *  ptk_file_icon_renderer_class_init:
 *
 ***************************************************************************/

static void
ptk_file_icon_renderer_class_init (PtkFileIconRendererClass *klass)
{
  GtkCellRendererClass *parent_renderer_class = GTK_CELL_RENDERER_CLASS(klass);
  GObjectClass         *object_class = G_OBJECT_CLASS(klass);

  parent_class           = g_type_class_peek_parent (klass);
  object_class->finalize = ptk_file_icon_renderer_finalize;

  /* Hook up functions to set and get our
  *   custom cell renderer properties */
  object_class->get_property = ptk_file_icon_renderer_get_property;
  object_class->set_property = ptk_file_icon_renderer_set_property;

  /* parent_renderer_class->get_size = ptk_file_icon_renderer_get_size; */
  parent_renderer_class->render   = ptk_file_icon_renderer_render;

  g_object_class_install_property ( object_class,
                                    PROP_STAT,
                                    g_param_spec_pointer ("stat",
                                        "File stat",
                                        "File stat",
                                        G_PARAM_READWRITE) );
  /*
  g_object_class_install_property ( object_class,
                                    PROP_FLAGS,
                                    g_param_spec_long ("flags",
                                        "Flags",
                                        "Flags about file types",
                                        0, G_MAXLONG, 0,
                                        G_PARAM_READWRITE) );
  */
  g_object_class_install_property ( object_class,
                                    PROP_FOLLOW_STATE,
                                    g_param_spec_boolean ("follow-state",
                                        "Follow State",
                                        "Whether the rendered pixbuf should be "
                                        "colorized according to the state",
                                        FALSE,
                                        G_PARAM_READWRITE) );
}


/***************************************************************************
 *
 *  ptk_file_icon_renderer_finalize: free any resources here
 *
 ***************************************************************************/

static void
ptk_file_icon_renderer_finalize (GObject *object)
{
/*
  g_object_unref( G_OBJECT(link_icon_large) );
  g_object_unref( G_OBJECT(link_icon_small) );
*/
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/***************************************************************************
 *
 *  ptk_file_icon_renderer_get_property: as it says
 *
 ***************************************************************************/

static void
ptk_file_icon_renderer_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *psec)
{
  PtkFileIconRenderer  *renderer = PTK_FILE_ICON_RENDERER(object);

  switch (param_id)
  {
/*    case PROP_FLAGS:
      g_value_set_long(value, renderer->flags);
      break;
*/
    case PROP_STAT:
      g_value_set_pointer(value, renderer->file_stat);

    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, renderer->follow_state);

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, psec);
      break;
  }
}


/***************************************************************************
 *
 *  ptk_file_icon_renderer_set_property: as it says
 *
 ***************************************************************************/

static void
ptk_file_icon_renderer_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  PtkFileIconRenderer *renderer = PTK_FILE_ICON_RENDERER (object);

  switch (param_id)
  {
    case PROP_STAT:
      renderer->file_stat = g_value_get_pointer(value);
      break;

/*
    case PROP_FLAGS:
      renderer->flags = g_value_get_long(value);
      break;
*/

    case PROP_FOLLOW_STATE:
      renderer->follow_state = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
  }
}

/***************************************************************************
 *
 *  ptk_file_icon_renderer_new: return a new cell renderer instance
 *
 ***************************************************************************/

GtkCellRenderer *
ptk_file_icon_renderer_new (void)
{
  return (GtkCellRenderer*)g_object_new(PTK_TYPE_FILE_ICON_RENDERER, NULL);
}


static GdkPixbuf *
create_colorized_pixbuf (GdkPixbuf *src,
                         GdkColor  *new_color)
{
  gint i, j;
  gint width, height, has_alpha, src_row_stride, dst_row_stride;
  gint red_value, green_value, blue_value;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pixsrc;
  guchar *pixdest;
  GdkPixbuf *dest;

  red_value = new_color->red / 255.0;
  green_value = new_color->green / 255.0;
  blue_value = new_color->blue / 255.0;

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
                         gdk_pixbuf_get_has_alpha (src),
                         gdk_pixbuf_get_bits_per_sample (src),
                         gdk_pixbuf_get_width (src),
                         gdk_pixbuf_get_height (src));

  has_alpha = gdk_pixbuf_get_has_alpha (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  src_row_stride = gdk_pixbuf_get_rowstride (src);
  dst_row_stride = gdk_pixbuf_get_rowstride (dest);
  target_pixels = gdk_pixbuf_get_pixels (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);

  for (i = 0; i < height; i++) {
    pixdest = target_pixels + i*dst_row_stride;
    pixsrc = original_pixels + i*src_row_stride;
    for (j = 0; j < width; j++) {
      *pixdest++ = (*pixsrc++ * red_value) >> 8;
      *pixdest++ = (*pixsrc++ * green_value) >> 8;
      *pixdest++ = (*pixsrc++ * blue_value) >> 8;
      if (has_alpha) {
        *pixdest++ = *pixsrc++;
      }
    }
  }
  return dest;
}


/***************************************************************************
    *
    *  ptk_file_icon_renderer_render: crucial - do the rendering.
    *
 ***************************************************************************/

static void
ptk_file_icon_renderer_render (GtkCellRenderer *cell,
                               GdkWindow       *window,
                               GtkWidget       *widget,
                               GdkRectangle    *background_area,
                               GdkRectangle    *cell_area,
                               GdkRectangle    *expose_area,
                               guint            flags)
{
  GtkCellRendererPixbuf *cellpixbuf = (GtkCellRendererPixbuf *) cell;

  GdkPixbuf *pixbuf;
  GdkPixbuf *invisible = NULL;
  GdkPixbuf *colorized = NULL;
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
#if GTK_CHECK_VERSION( 2, 8, 0 )
  cairo_t *cr;
#endif

  GtkCellRendererClass* parent_renderer_class;

  parent_renderer_class = GTK_CELL_RENDERER_CLASS(parent_class);

  parent_renderer_class->get_size ( cell, widget, cell_area,
                                    &pix_rect.x,
                                    &pix_rect.y,
                                    &pix_rect.width,
                                    &pix_rect.height );

  pix_rect.x += cell_area->x;
  pix_rect.y += cell_area->y;
  pix_rect.width  -= cell->xpad * 2;
  pix_rect.height -= cell->ypad * 2;

  if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) ||
       !gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect))
    return;

  pixbuf = cellpixbuf->pixbuf;

  if (cell->is_expander)
  {
    if (cell->is_expanded &&
        cellpixbuf->pixbuf_expander_open != NULL)
      pixbuf = cellpixbuf->pixbuf_expander_open;
    else if (!cell->is_expanded &&
              cellpixbuf->pixbuf_expander_closed != NULL)
      pixbuf = cellpixbuf->pixbuf_expander_closed;
  }

  if (!pixbuf)
    return;

  if( PTK_FILE_ICON_RENDERER(cell)->follow_state )
  {
    if ( GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE || !cell->sensitive)
    {
      GtkIconSource *source;

      source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (source, pixbuf);
        /* The size here is arbitrary; since size isn't
      * wildcarded in the source, it isn't supposed to be
      * scaled by the engine function
        */
      gtk_icon_source_set_size (source, GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_icon_source_set_size_wildcarded (source, FALSE);

      invisible = gtk_style_render_icon (widget->style,
                                        source,
                                        gtk_widget_get_direction (widget),
                                        GTK_STATE_INSENSITIVE,
                                        /* arbitrary */
                                        (GtkIconSize)-1,
                                        widget,
                                        "gtkcellrendererpixbuf");

      gtk_icon_source_free (source);
      pixbuf = invisible;
    }
    else if ( (flags & (GTK_CELL_RENDERER_SELECTED/*|GTK_CELL_RENDERER_PRELIT*/)) != 0)
    {
      GtkStateType state;
      GdkColor* color;

      if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
      {
        if (GTK_WIDGET_HAS_FOCUS (widget))
          state = GTK_STATE_SELECTED;
        else
          state = GTK_STATE_ACTIVE;
        color = &widget->style->base[state];
      }
      else {
        state = GTK_STATE_PRELIGHT;
      }

      colorized = create_colorized_pixbuf (pixbuf,
                                          color);

      pixbuf = colorized;
    }
  }
#if GTK_CHECK_VERSION(2, 8, 0)
  cr = gdk_cairo_create (window);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, pix_rect.x, pix_rect.y);
  gdk_cairo_rectangle (cr, &draw_rect);
  cairo_fill (cr);
#else
  gdk_draw_pixbuf (GDK_DRAWABLE (window), NULL, pixbuf, 0, 0,
                   pix_rect.x, pix_rect.y, pix_rect.width, pix_rect.height,
                   GDK_RGB_DITHER_NORMAL, 0, 0);
#endif

  if( PTK_FILE_ICON_RENDERER(cell)->file_stat )
  {
    if( S_ISLNK(PTK_FILE_ICON_RENDERER(cell)->file_stat->st_mode) )
    {
#if GTK_CHECK_VERSION(2, 8, 0)
      gdk_cairo_set_source_pixbuf (cr, link_icon_large,
                                   pix_rect.x - 2,
                                   pix_rect.y - 2 );
      draw_rect.x -= 2;
      draw_rect.y -= 2;
      gdk_cairo_rectangle (cr, &draw_rect);
      cairo_fill (cr);
#else
      gdk_draw_pixbuf (GDK_DRAWABLE (window), NULL, link_icon_large, 0, 0,
                       pix_rect.x - 2, pix_rect.y - 2,
                       -1, -1, GDK_RGB_DITHER_NORMAL,
                       0, 0);
#endif
    }
  }

#if GTK_CHECK_VERSION(2, 8, 0)
  cairo_destroy (cr);
#endif

  if (invisible)
    g_object_unref (invisible);

  if (colorized)
    g_object_unref (colorized);

}

