/*
*  C Implementation: ptk-path-entry
*
* Description: A custom entry widget with auto-completion
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-path-entry.h"
#include <gdk/gdkkeysyms.h>

static char*
get_cwd( GtkEntry* entry )
{
    if( g_path_is_absolute( gtk_entry_get_text(entry) ) )
        return g_path_get_dirname( gtk_entry_get_text(entry) );
    return NULL;
}

static void update_completion( GtkEntry* entry,
                               GtkEntryCompletion* completion )
{
    char* new_dir;
    const char* old_dir;
    GtkListStore* list;

    new_dir = get_cwd( entry );
    old_dir = (const char*)g_object_get_data( (GObject*)completion, "cwd" );
    if( old_dir && new_dir && 0 == strcmp( old_dir, new_dir ) )
    {
        g_free( new_dir );
        return;
    }
    g_object_set_data_full( (GObject*)completion, "cwd",
                             new_dir, g_free );
    list = (GtkListStore*)gtk_entry_completion_get_model( completion );
    gtk_list_store_clear( list );
    if( new_dir )
    {
        GDir* dir;
        if( dir = g_dir_open( new_dir, 0, NULL ) )
        {
            const char* name;
            while( name = g_dir_read_name( dir ) )
            {
                char* full_path = g_build_filename( new_dir, name, NULL );
                if( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    GtkTreeIter it;
                    char* disp_path = g_filename_display_name( full_path );
                    gtk_list_store_append( list, &it );
                    gtk_list_store_set( list, &it, 0, disp_path, -1 );
                    g_free( disp_path );
                }
                g_free( full_path );
            }
            g_dir_close( dir );
        }
    }
}

static void
on_changed( GtkEntry* entry, gpointer user_data )
{
    GtkEntryCompletion* completion;
    completion = gtk_entry_get_completion( entry );
    update_completion( entry, completion );
}

static gboolean
on_key_press( GtkWidget *entry, GdkEventKey* evt, gpointer user_data )
{
    if( evt->keyval == GDK_Tab && ! (evt->state & GDK_CONTROL_MASK) )
    {
#if GTK_CHECK_VERSION(2, 8, 0)
        /* This API exists since gtk+ 2.6, but gtk+ 2.6.x seems to have bugs
           related to this API which cause crash.
           Reported in bug #1570063 by Orlando Fiol <fiolorlando@gmail.com>
        */
        gtk_entry_completion_insert_prefix( gtk_entry_get_completion(entry) );
#endif
        gtk_editable_set_position( (GtkEditable*)entry, -1 );
        return TRUE;
    }
    return FALSE;
}

static gboolean
on_focus_in( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkListStore* list = gtk_list_store_new( 1, G_TYPE_STRING );
    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_model( completion, GTK_TREE_MODEL(list) );
    g_object_unref( list );
    gtk_entry_completion_set_text_column( completion, 0 );
    gtk_entry_completion_set_inline_completion( completion, TRUE );
#if GTK_CHECK_VERSION( 2, 8, 0)
    /* gtk+ prior to 2.8.0 doesn't have this API */
    gtk_entry_completion_set_popup_set_width( completion, TRUE );
#endif
    gtk_entry_set_completion( GTK_ENTRY(entry), completion );
    g_signal_connect( entry, "changed", on_changed, NULL );
    g_object_unref( completion );

    return FALSE;
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    gtk_entry_set_completion( GTK_ENTRY(entry), NULL );
    g_signal_handlers_disconnect_by_func( entry, on_changed, NULL );
    return FALSE;
}

GtkWidget* ptk_path_entry_new()
{
    GtkWidget* entry = gtk_entry_new();

    g_signal_connect( entry, "focus-in-event", G_CALLBACK(on_focus_in), NULL );
    g_signal_connect( entry, "focus-out-event", G_CALLBACK(on_focus_out), NULL );

    /* used to eat the tab key */
    g_signal_connect( entry, "key-press-event", G_CALLBACK(on_key_press), NULL );

    return entry;
}
