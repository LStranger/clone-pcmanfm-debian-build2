/*
 * DO NOT EDIT THIS FILE - it is generated by Glade.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "file-assoc-dlg.h"
#include "file-assoc-dlg-ui.h"
#include "glade-support.h"

#define GLADE_HOOKUP_OBJECT(component,widget,name) \
  g_object_set_data_full (G_OBJECT (component), name, \
    gtk_widget_ref (widget), (GDestroyNotify) gtk_widget_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component,widget,name) \
  g_object_set_data (G_OBJECT (component), name, widget)

GtkWidget*
create_file_assoc_dlg (void)
{
  GtkWidget *file_assoc_dlg;
  GtkWidget *dialog_vbox1;
  GtkWidget *hpaned;
  GtkWidget *vbox3;
  GtkWidget *label2;
  GtkWidget *scrolledwindow1;
  GtkWidget *types;
  GtkWidget *vbox1;
  GtkWidget *label1;
  GtkWidget *hbox4;
  GtkWidget *scrolledwindow2;
  GtkWidget *actions;
  GtkWidget *vbox4;
  GtkWidget *add_app;
  GtkWidget *alignment6;
  GtkWidget *hbox8;
  GtkWidget *image6;
  GtkWidget *label8;
  GtkWidget *add_custom;
  GtkWidget *alignment7;
  GtkWidget *hbox9;
  GtkWidget *image7;
  GtkWidget *label9;
  GtkWidget *edit;
  GtkWidget *remove;
  GtkWidget *up;
  GtkWidget *alignment8;
  GtkWidget *hbox10;
  GtkWidget *image8;
  GtkWidget *label10;
  GtkWidget *button13;
  GtkWidget *alignment9;
  GtkWidget *hbox11;
  GtkWidget *image9;
  GtkWidget *label11;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancel;
  GtkWidget *ok;

  file_assoc_dlg = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (file_assoc_dlg), _("File Associations"));
  gtk_window_set_position (GTK_WINDOW (file_assoc_dlg), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (file_assoc_dlg), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (file_assoc_dlg), 640, 400);
  gtk_window_set_type_hint (GTK_WINDOW (file_assoc_dlg), GDK_WINDOW_TYPE_HINT_DIALOG);

  dialog_vbox1 = GTK_DIALOG (file_assoc_dlg)->vbox;
  gtk_widget_show (dialog_vbox1);

  hpaned = gtk_hpaned_new ();
  gtk_widget_show (hpaned);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), hpaned, TRUE, TRUE, 0);
  gtk_paned_set_position (GTK_PANED (hpaned), 160);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox3);
  gtk_paned_pack1 (GTK_PANED (hpaned), vbox3, TRUE, FALSE);

  label2 = gtk_label_new (_("Known File Types:"));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (vbox3), label2, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (label2), 4, 4);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (vbox3), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_SHADOW_IN);

  types = gtk_tree_view_new ();
  gtk_widget_show (types);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), types);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (types), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (types), TRUE);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_paned_pack2 (GTK_PANED (hpaned), vbox1, TRUE, FALSE);

  label1 = gtk_label_new (_("Actions Showed in Popup Menus"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_misc_set_padding (GTK_MISC (label1), 4, 4);

  hbox4 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox4);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox4, TRUE, TRUE, 0);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow2);
  gtk_box_pack_start (GTK_BOX (hbox4), scrolledwindow2, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_SHADOW_IN);

  actions = gtk_tree_view_new ();
  gtk_widget_show (actions);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), actions);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (actions), TRUE);
  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (actions), TRUE);

  vbox4 = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (vbox4);
  gtk_box_pack_start (GTK_BOX (hbox4), vbox4, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox4), 2);

  add_app = gtk_button_new ();
  gtk_widget_show (add_app);
  gtk_box_pack_start (GTK_BOX (vbox4), add_app, FALSE, FALSE, 0);

  alignment6 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_show (alignment6);
  gtk_container_add (GTK_CONTAINER (add_app), alignment6);

  hbox8 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox8);
  gtk_container_add (GTK_CONTAINER (alignment6), hbox8);

  image6 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image6);
  gtk_box_pack_start (GTK_BOX (hbox8), image6, FALSE, FALSE, 0);

  label8 = gtk_label_new_with_mnemonic (_("Add _Application"));
  gtk_widget_show (label8);
  gtk_box_pack_start (GTK_BOX (hbox8), label8, FALSE, FALSE, 0);

  add_custom = gtk_button_new ();
  gtk_widget_show (add_custom);
  gtk_box_pack_start (GTK_BOX (vbox4), add_custom, FALSE, FALSE, 0);

  alignment7 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_show (alignment7);
  gtk_container_add (GTK_CONTAINER (add_custom), alignment7);

  hbox9 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox9);
  gtk_container_add (GTK_CONTAINER (alignment7), hbox9);

  image7 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image7);
  gtk_box_pack_start (GTK_BOX (hbox9), image7, FALSE, FALSE, 0);

  label9 = gtk_label_new_with_mnemonic (_("Add _Custom Action"));
  gtk_widget_show (label9);
  gtk_box_pack_start (GTK_BOX (hbox9), label9, FALSE, FALSE, 0);

  edit = gtk_button_new_from_stock ("gtk-edit");
  gtk_widget_show (edit);
  gtk_box_pack_start (GTK_BOX (vbox4), edit, FALSE, FALSE, 0);

  remove = gtk_button_new_from_stock ("gtk-remove");
  gtk_widget_show (remove);
  gtk_box_pack_start (GTK_BOX (vbox4), remove, FALSE, FALSE, 0);

  up = gtk_button_new ();
  gtk_widget_show (up);
  gtk_box_pack_start (GTK_BOX (vbox4), up, FALSE, FALSE, 0);

  alignment8 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_show (alignment8);
  gtk_container_add (GTK_CONTAINER (up), alignment8);

  hbox10 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox10);
  gtk_container_add (GTK_CONTAINER (alignment8), hbox10);

  image8 = gtk_image_new_from_stock ("gtk-go-up", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image8);
  gtk_box_pack_start (GTK_BOX (hbox10), image8, FALSE, FALSE, 0);

  label10 = gtk_label_new_with_mnemonic (_("Move _Up"));
  gtk_widget_show (label10);
  gtk_box_pack_start (GTK_BOX (hbox10), label10, FALSE, FALSE, 0);

  button13 = gtk_button_new ();
  gtk_widget_show (button13);
  gtk_box_pack_start (GTK_BOX (vbox4), button13, FALSE, FALSE, 0);

  alignment9 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_widget_show (alignment9);
  gtk_container_add (GTK_CONTAINER (button13), alignment9);

  hbox11 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox11);
  gtk_container_add (GTK_CONTAINER (alignment9), hbox11);

  image9 = gtk_image_new_from_stock ("gtk-go-down", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image9);
  gtk_box_pack_start (GTK_BOX (hbox11), image9, FALSE, FALSE, 0);

  label11 = gtk_label_new_with_mnemonic (_("Move _Down"));
  gtk_widget_show (label11);
  gtk_box_pack_start (GTK_BOX (hbox11), label11, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (file_assoc_dlg)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancel = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancel);
  gtk_dialog_add_action_widget (GTK_DIALOG (file_assoc_dlg), cancel, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);

  ok = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (ok);
  gtk_dialog_add_action_widget (GTK_DIALOG (file_assoc_dlg), ok, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);

  /* Store pointers to all widgets, for use by lookup_widget(). */
  GLADE_HOOKUP_OBJECT_NO_REF (file_assoc_dlg, file_assoc_dlg, "file_assoc_dlg");
  GLADE_HOOKUP_OBJECT_NO_REF (file_assoc_dlg, dialog_vbox1, "dialog_vbox1");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hpaned, "hpaned");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, vbox3, "vbox3");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label2, "label2");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, scrolledwindow1, "scrolledwindow1");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, types, "types");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, vbox1, "vbox1");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label1, "label1");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hbox4, "hbox4");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, scrolledwindow2, "scrolledwindow2");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, actions, "actions");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, vbox4, "vbox4");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, add_app, "add_app");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, alignment6, "alignment6");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hbox8, "hbox8");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, image6, "image6");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label8, "label8");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, add_custom, "add_custom");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, alignment7, "alignment7");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hbox9, "hbox9");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, image7, "image7");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label9, "label9");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, edit, "edit");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, remove, "remove");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, up, "up");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, alignment8, "alignment8");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hbox10, "hbox10");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, image8, "image8");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label10, "label10");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, button13, "button13");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, alignment9, "alignment9");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, hbox11, "hbox11");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, image9, "image9");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, label11, "label11");
  GLADE_HOOKUP_OBJECT_NO_REF (file_assoc_dlg, dialog_action_area1, "dialog_action_area1");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, cancel, "cancel");
  GLADE_HOOKUP_OBJECT (file_assoc_dlg, ok, "ok");

  return file_assoc_dlg;
}

