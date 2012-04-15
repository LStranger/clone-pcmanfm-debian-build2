/*
*  C Implementation: ptkbookmarks
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-bookmarks.h"

#include <stdio.h>
#include <string.h>

const char bookmarks_file_name[] = ".gtk-bookmarks";
static PtkBookmarks bookmarks = {0};

typedef struct
{
    GFunc callback;
    gpointer user_data;
}
BookmarksCallback;

/* Notify all callbacks that the bookmarks are changed. */
static void ptk_bookmarks_notify()
{
    BookmarksCallback* cb;
    int i;
    /* Call the callback functions set by the user */
    if( bookmarks.callbacks && bookmarks.callbacks->len )
    {
        cb = (BookmarksCallback*)bookmarks.callbacks->data;
        for( i = 0; i < bookmarks.callbacks->len; ++i )
        {
            cb[i].callback( &bookmarks, cb[i].user_data );
        }
    }
}

/*
  Get a self-maintained list of bookmarks
  This is read from "~/.gtk-bookmarks".
*/
PtkBookmarks* ptk_bookmarks_get ()
{
    FILE* file;
    gchar* path;
    gchar* upath;
    gchar* uri;
    gchar* item;
    gchar* name;
    gchar* basename;
    char line[1024];
    gsize name_len, upath_len;

    if( 0 == bookmarks.n_ref )
    {
        path = g_build_filename( g_get_home_dir(), bookmarks_file_name, NULL );
        file = fopen( path, "r" );
        g_free( path );
        if( file )
        {
            while( fgets( line, sizeof(line), file ) )
            {
                /* Every line is an URI containing no space charactetrs
                   with its name appended (optional) */
                uri = strtok( line, " \r\n" );
                if( ! uri || !*uri )
                    continue;
                path = g_filename_from_uri(uri, NULL, NULL);
                if( path )
                {
                    upath = g_filename_to_utf8(path, -1, NULL, &upath_len, NULL);
                    g_free( path );
                    if( upath )
                    {
                        name = strtok( NULL, "\r\n" );
                        if( name )
                        {
                            name_len = strlen( name );
                            basename = NULL;
                        }
                        else
                        {
                            name = basename = g_path_get_basename( upath );
                            name_len = strlen( basename );
                        }
                        item = ptk_bookmarks_item_new( name, name_len,
                                                       upath, upath_len );
                        bookmarks.list = g_list_append( bookmarks.list,
                                                        item );
                        g_free(upath);
                        g_free( basename );
                    }
                }
            }
            fclose( file );
        }
    }
    g_atomic_int_inc( &bookmarks.n_ref );
    return &bookmarks;
}

/*
  Replace the content of the bookmarks with new_list.
  PtkBookmarks will then owns new_list, and hence
  new_list shouldn't be freed after this function is called.
*/
void ptk_bookmarks_set ( GList* new_list )
{
    g_list_foreach( bookmarks.list, (GFunc)g_free, NULL );
    g_list_free( bookmarks.list );
    bookmarks.list = new_list;
    ptk_bookmarks_notify();
}

/* Insert an item into bookmarks */
void ptk_bookmarks_insert ( const char* name, const char* path, gint pos )
{
    char* item;
    item = ptk_bookmarks_item_new(name, strlen(name), path, strlen(path));
    bookmarks.list = g_list_insert( bookmarks.list,
                                    item, pos );
    ptk_bookmarks_notify();
}

/* Append an item into bookmarks */
void ptk_bookmarks_append ( const char* name, const char* path )
{
    char* item;
    item = ptk_bookmarks_item_new(name, strlen(name), path, strlen(path));
    bookmarks.list = g_list_append( bookmarks.list,
                                    item );
    ptk_bookmarks_notify();
}

static GList* find_item( const char* path )
{
    GList* l;
    char* item;
    char* item_path;
    int len;

    for( l = bookmarks.list; l; l = l->next )
    {
        item = (char*)l->data;
        len = strlen( item );
        item_path = item + len + 1;
        if( 0 == strcmp( path, item_path ) )
            break;
    }
    return l;
}

/* find an item from bookmarks */
void ptk_bookmarks_remove ( const char* path )
{
    GList* l;

    if( (l = find_item( path )) )
    {
        g_free( l->data );
        bookmarks.list = g_list_delete_link( bookmarks.list, l );
        ptk_bookmarks_notify();
    }
}

void ptk_bookmarks_rename ( const char* path, const char* new_name )
{
    GList* l;
    char* item;

    if( path && new_name && (l = find_item( path )) )
    {
        item = ptk_bookmarks_item_new(new_name, strlen(new_name),
                                      path, strlen(path));
        g_free( l->data );
        l->data = item;
        ptk_bookmarks_notify();
    }
}

static void ptk_bookmarks_save_item( GList* l, FILE* file )
{
    gchar* item;
    const gchar* upath;
    int len;
    char* uri;
    char* path;

    item = (char*)l->data;
    upath = ptk_bookmarks_item_get_path( item );
    path = g_filename_from_utf8( upath, -1, NULL, NULL, NULL );
    if( path )
    {
        uri = g_filename_to_uri( path, NULL, NULL );
        g_free( path );
        if( uri )
        {
            fprintf( file, "%s %s\n", uri, item );
            g_free( uri );
        }
    }
}

/* Save the content of the bookmarks to "~/.gtk-bookmarks" */
void ptk_bookmarks_save ()
{
    FILE* file;
    gchar* path;
    GList* l;

    path = g_build_filename( g_get_home_dir(), bookmarks_file_name, NULL );
    file = fopen( path, "w" );
    g_free( path );

    if( file )
    {
        for( l = bookmarks.list; l; l = l->next )
        {
            ptk_bookmarks_save_item( l, file );
        }
        fclose( file );
    }
}

/* Add a callback which gets called when the content of bookmarks changed */
void ptk_bookmarks_add_callback ( GFunc callback, gpointer user_data )
{
    BookmarksCallback cb;
    cb.callback = callback;
    cb.user_data = user_data;
    if( !bookmarks.callbacks )
    {
        bookmarks.callbacks = g_array_new (FALSE, FALSE, sizeof(BookmarksCallback));
    }
    bookmarks.callbacks = g_array_append_val( bookmarks.callbacks, cb );
}

/* Remove a callback added by ptk_bookmarks_add_callback */
void ptk_bookmarks_remove_callback ( GFunc callback, gpointer user_data )
{
    BookmarksCallback* cb = (BookmarksCallback*)bookmarks.callbacks->data;
    int i;
    if( bookmarks.callbacks )
    {
        for(i = 0; i < bookmarks.callbacks->len; ++i )
        {
            if( cb[i].callback == callback && cb[i].user_data == user_data )
            {
                bookmarks.callbacks = g_array_remove_index_fast ( bookmarks.callbacks, i );
                break;
            }
        }
    }
}

void ptk_bookmarks_unref ()
{
    if( g_atomic_int_dec_and_test(&bookmarks.n_ref) )
    {
        bookmarks.n_ref = 0;
        if( bookmarks.list )
        {
            g_list_foreach( bookmarks.list, (GFunc)g_free, NULL );
            g_list_free( bookmarks.list );
            bookmarks.list = NULL;
        }

        if( bookmarks.callbacks )
        {
            g_array_free(bookmarks.callbacks, TRUE);
            bookmarks.callbacks = NULL;
        }
    }
}

/*
* Create a new bookmark item.
* name: displayed name of the bookmark item.
* name_len: length of name;
* upath: dir path of the bookmark item encoded in UTF-8.
* upath_len: length of upath;
* Returned value is a newly allocated string.
*/
gchar* ptk_bookmarks_item_new( const gchar* name, gsize name_len,
                               const gchar* upath, gsize upath_len )
{
    char* buf;
    ++name_len; /* include terminating null */
    ++upath_len; /* include terminating null */
    buf = g_new( char, name_len + upath_len );
    memcpy( buf, name, name_len );
    memcpy( buf + name_len, upath, upath_len );
    return buf;
}

/*
* item: bookmark item
* Returned value: dir path of the bookmark item. (int utf-8)
*/
const gchar* ptk_bookmarks_item_get_path( const gchar* item )
{
    int name_len = strlen(item);
    return (item + name_len + 1);
}

