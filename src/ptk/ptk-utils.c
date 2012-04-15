/*
*  C Implementation: ptkutils
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-utils.h"
#include <glib.h>
#include <glib/gi18n.h>

GtkWidget* ptk_menu_new_from_data( PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group )
{
  GtkWidget* menu;
  menu = gtk_menu_new();
  ptk_menu_add_items_from_data( menu, entries, cb_data, accel_group );
  return menu;
}

void ptk_menu_add_items_from_data( GtkWidget* menu,
                                   PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group )
{
  PtkMenuItemEntry* ent;
  GtkWidget* menu_item;
  GtkWidget* sub_menu;
  GtkWidget* image;
  GSList* radio_group = NULL;
  const char* signal;

  for( ent = entries; ; ++ent )
  {
    if( G_LIKELY( ent->label ) )
    {
      /* Stock item */
      signal = "activate";
      if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )  {
        menu_item = gtk_image_menu_item_new_from_stock( ent->label, accel_group );
      }
      else if( G_LIKELY(ent->stock_icon) )  {
        if( G_LIKELY( ent->stock_icon > 2 ) )  {
          menu_item = gtk_image_menu_item_new_with_mnemonic(_(ent->label));
          image = gtk_image_new_from_stock( ent->stock_icon, GTK_ICON_SIZE_MENU );
          gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(menu_item), image );
        }
        else if( G_UNLIKELY( PTK_IS_CHECK_MENU_ITEM(ent) ) )  {
          menu_item = gtk_check_menu_item_new_with_mnemonic(_(ent->label));
          signal = "toggled";
        }
        else if( G_UNLIKELY( PTK_IS_RADIO_MENU_ITEM(ent) ) )  {
          menu_item = gtk_radio_menu_item_new_with_mnemonic( radio_group, _(ent->label) );
          if( G_LIKELY( PTK_IS_RADIO_MENU_ITEM( (ent + 1) ) ) )
            radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(menu_item) );
          else
            radio_group = NULL;
          signal = "toggled";
        }
      }
      else  {
        menu_item = gtk_menu_item_new_with_mnemonic(_(ent->label));
      }

      if( G_LIKELY(accel_group) && ent->key ) {
        gtk_widget_add_accelerator (menu_item, "activate", accel_group,
                                    ent->key, ent->mod, GTK_ACCEL_VISIBLE);
      }

      if( G_LIKELY(ent->callback) )  { /* Callback */
        g_signal_connect( menu_item, signal, ent->callback, cb_data);
      }

      if( G_UNLIKELY( ent->sub_menu ) )  { /* Sub menu */
        sub_menu = ptk_menu_new_from_data( ent->sub_menu, cb_data, accel_group );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu_item), sub_menu );
      }
    }
    else
    {
      if( ! ent->stock_icon ) /* End of menu */
        break;
      menu_item = gtk_separator_menu_item_new();
    }
    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), menu_item );
    if( G_UNLIKELY(ent->ret) ) {/* Return */
      *ent->ret = menu_item;
      ent->ret = NULL;
    }
  }
}


GtkWidget* ptk_toolbar_add_items_from_data( GtkWidget* toolbar,
                                            PtkToolItemEntry* entries,
                                            gpointer cb_data,
                                            GtkTooltips* tooltips )
{
  GtkWidget* btn;
  PtkToolItemEntry* ent;
  GtkWidget* image;
  GtkWidget* menu;
  GtkIconSize icon_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (toolbar));
  GSList* radio_group = NULL;

  for( ent = entries; ; ++ent )
  {
    /* Normal tool item */
    if( G_LIKELY( ent->stock_icon || ent->tooltip || ent->label ) )
    {
      /* Stock item */
      if( G_LIKELY(ent->stock_icon) )
        image = gtk_image_new_from_stock( ent->stock_icon, icon_size );
      else
        image = NULL;

      if( G_LIKELY( ! ent->menu ) )  { /* Normal button */
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = gtk_tool_button_new_from_stock ( ent->label );
        else
          btn = gtk_tool_button_new ( image, _(ent->label) );
      }
      else if( G_UNLIKELY( PTK_IS_CHECK_TOOL_ITEM(ent) ) )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = gtk_toggle_tool_button_new_from_stock( ent->label );
        else {
          btn = gtk_toggle_tool_button_new ();
          gtk_tool_button_set_icon_widget( btn, image );
          gtk_tool_button_set_label(btn, _(ent->label));
        }
      }
      else if( G_UNLIKELY( PTK_IS_RADIO_TOOL_ITEM(ent) ) )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = gtk_radio_tool_button_new_from_stock( radio_group, ent->label );
        else {
          btn = gtk_radio_tool_button_new( radio_group );
          if( G_LIKELY( PTK_IS_RADIO_TOOL_ITEM( (ent + 1) ) ) )
            radio_group = gtk_radio_tool_button_get_group( GTK_RADIO_TOOL_BUTTON(btn) );
          else
            radio_group = NULL;
          gtk_tool_button_set_icon_widget( btn, image );
          gtk_tool_button_set_label(btn, _(ent->label));
        }
      }
      else if( ent->menu )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = gtk_menu_tool_button_new_from_stock ( ent->label );
        else {
          btn = gtk_menu_tool_button_new ( image, _(ent->label) );
          if( G_LIKELY( ent->menu ) )  { /* Sub menu */
            menu = ptk_menu_new_from_data( ent->menu, cb_data, NULL );
            gtk_menu_tool_button_set_menu( GTK_MENU_TOOL_BUTTON(btn), menu );
          }
        }
      }

      if( G_LIKELY(ent->callback) )  { /* Callback */
        if( G_LIKELY( ent->menu == NULL ) )
          g_signal_connect( btn, "clicked", ent->callback, cb_data);
        else
          g_signal_connect( btn, "toggled", ent->callback, cb_data);
      }

      if( G_LIKELY(ent->tooltip) )
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (btn), tooltips, _(ent->tooltip), NULL);
    }
    else
    {
      if( ! PTK_IS_SEPARATOR_TOOL_ITEM(ent) ) /* End of menu */
        break;
      btn = gtk_separator_tool_item_new ();
    }

    gtk_toolbar_insert ( GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn), -1 );

    if( G_UNLIKELY(ent->ret) ) {/* Return */
      *ent->ret = btn;
      ent->ret = NULL;
    }
  }
}

void ptk_show_error(GtkWindow* parent, const char* message )
{
  GtkWidget* dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, message);
  gtk_dialog_run( GTK_DIALOG(dlg) );
  gtk_widget_destroy( dlg );
}

