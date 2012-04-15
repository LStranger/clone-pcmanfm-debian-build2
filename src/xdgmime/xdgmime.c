/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmime.c: XDG Mime Spec mime resolver.  Based on version 0.11 of the spec.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2003,2004  Red Hat, Inc.
 * Copyright (C) 2003,2004  Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2006       Hong Jen Yee <pcman.tw@gmail.com>
 *
 * Licensed under the Academic Free License version 2.0
 * Or under the following terms:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * xdgmime is originally provided by freedesktop.org
 * This copy has been "heavily modified" by Hong Jen Yee to
 * be used in PCMan File Manager.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xdgmime.h"
#include "xdgmimeint.h"
#include "xdgmimeglob.h"
#include "xdgmimemagic.h"
#include "xdgmimealias.h"
#include "xdgmimeparent.h"
#include "xdgmimecache.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <sys/time.h>
#include <unistd.h>
#include <assert.h>

#include <glib.h>

#include "vfs-file-monitor.h"

/* typedef struct XdgDirTimeList XdgDirTimeList; */
typedef struct XdgCallbackList XdgCallbackList;

static time_t last_stat_time = 0;

static XdgGlobHash *global_hash = NULL;
static XdgMimeMagic *global_magic = NULL;
static XdgAliasList *alias_list = NULL;
static XdgParentList *parent_list = NULL;
/* static XdgDirTimeList *dir_time_list = NULL; */
static XdgCallbackList *callback_list = NULL;

XdgMimeCache **_caches = NULL;
static int n_caches = 0;

const char xdg_mime_type_unknown[] = "application/octet-stream";

/* 2006.01.16 added by Hong Jen Yee (PCMan) */
const char xdg_mime_type_directory[] = "inode/directory";
const char xdg_mime_type_executable[] = "application/x-executable";
const char xdg_mime_type_plain_text[] = "text/plain";

/* VFSFileMonotors used to monitor the change of mime data files */
static GSList* monitor_list = NULL;
static guint update_timout = 0;

enum
{
    XDG_CHECKED_UNCHECKED,
    XDG_CHECKED_VALID,
    XDG_CHECKED_INVALID
};

struct XdgCallbackList
{
    XdgCallbackList *next;
    XdgCallbackList *prev;
    int callback_id;
    XdgMimeCallback callback;
    void *data;
    XdgMimeDestroy destroy;
};

/* Function called by xdg_run_command_on_dirs.  If it returns TRUE, further
 * directories aren't looked at */
typedef int ( *XdgDirectoryFunc ) ( const char *directory,
                                    void *user_data );

static gboolean xdg_mime_reload_cache( gpointer user_data );

static void xdg_mime_file_changed ( VFSFileMonitor* fm,
                                    VFSFileMonitorEvent event,
                                    const char* file_name,
                                    gpointer user_data )
{
    if ( 0 == update_timout )   /* Update cache */
        update_timout = g_timeout_add( 1000, ( GSourceFunc ) xdg_mime_reload_cache, NULL );
}

static int
xdg_mime_init_from_directory ( const char *directory )
{
    char * file_name;
    struct stat st;

    assert ( directory != NULL );

    file_name = malloc ( strlen ( directory ) + strlen ( "/mime/mime.cache" ) + 1 );
    strcpy ( file_name, directory );
    strcat ( file_name, "/mime/mime.cache" );

    XdgMimeCache * cache = _xdg_mime_cache_new_from_file ( file_name );
    /* If non-existant files are monitored, the callback functions
       will be called when the file is created. */
    VFSFileMonitor* monitor = vfs_file_monitor_add( file_name,
                                                    xdg_mime_file_changed, NULL );
    monitor_list = g_slist_prepend( monitor_list, monitor );

    if ( stat ( file_name, &st ) == 0 )
    {
        if ( cache != NULL )
        {
            _caches = realloc ( _caches, sizeof ( XdgMimeCache * ) * ( n_caches + 2 ) );
            _caches[ n_caches ] = cache;
            _caches[ n_caches + 1 ] = NULL;
            n_caches++;

            return FALSE;
        }
    }
    free ( file_name );

    file_name = malloc ( strlen ( directory ) + strlen ( "/mime/globs" ) + 1 );
    strcpy ( file_name, directory );
    strcat ( file_name, "/mime/globs" );
    if ( stat ( file_name, &st ) == 0 )
    {
        _xdg_mime_glob_read_from_file ( global_hash, file_name );
    }
    free ( file_name );

    file_name = malloc ( strlen ( directory ) + strlen ( "/mime/magic" ) + 1 );
    strcpy ( file_name, directory );
    strcat ( file_name, "/mime/magic" );
    if ( stat ( file_name, &st ) == 0 )
    {
        _xdg_mime_magic_read_from_file ( global_magic, file_name );
    }
    free ( file_name );

    file_name = malloc ( strlen ( directory ) + strlen ( "/mime/aliases" ) + 1 );
    strcpy ( file_name, directory );
    strcat ( file_name, "/mime/aliases" );
    _xdg_mime_alias_read_from_file ( alias_list, file_name );
    free ( file_name );

    file_name = malloc ( strlen ( directory ) + strlen ( "/mime/subclasses" ) + 1 );
    strcpy ( file_name, directory );
    strcat ( file_name, "/mime/subclasses" );
    _xdg_mime_parent_read_from_file ( parent_list, file_name );
    free ( file_name );

    return FALSE; /* Keep processing */
}

/* Runs a command on all the directories in the search path */
void xdg_run_command_on_dirs ( XdgDirectoryFunc func,
                               void *user_data )
{
    const char * xdg_data_home;
    const char *xdg_data_dirs;
    const char *ptr;

    xdg_data_home = getenv ( "XDG_DATA_HOME" );
    if ( xdg_data_home )
    {
        if ( ( func ) ( xdg_data_home, user_data ) )
            return ;
    }
    else
    {
        const char *home;

        home = getenv ( "HOME" );
        if ( home != NULL )
        {
            char * guessed_xdg_home;
            int stop_processing;

            guessed_xdg_home = malloc ( strlen ( home ) + strlen ( "/.local/share" ) + 1 );
            strcpy ( guessed_xdg_home, home );
            strcat ( guessed_xdg_home, "/.local/share" );
            stop_processing = ( func ) ( guessed_xdg_home, user_data );
            free ( guessed_xdg_home );

            if ( stop_processing )
                return ;
        }
    }

    xdg_data_dirs = getenv ( "XDG_DATA_DIRS" );
    if ( xdg_data_dirs == NULL )
        xdg_data_dirs = "/usr/local/share:/usr/share";

    ptr = xdg_data_dirs;

    while ( *ptr != '\000' )
    {
        const char * end_ptr;
        char *dir;
        int len;
        int stop_processing;

        end_ptr = ptr;
        while ( *end_ptr != ':' && *end_ptr != '\000' )
            end_ptr ++;

        if ( end_ptr == ptr )
        {
            ptr++;
            continue;
        }

        if ( *end_ptr == ':' )
            len = end_ptr - ptr;
        else
            len = end_ptr - ptr + 1;
        dir = malloc ( len + 1 );
        strncpy ( dir, ptr, len );
        dir[ len ] = '\0';
        stop_processing = ( func ) ( dir, user_data );
        free ( dir );

        if ( stop_processing )
            return ;

        ptr = end_ptr;
    }
}

void xdg_mime_init ( void )
{
    global_hash = _xdg_glob_hash_new ();
    global_magic = _xdg_mime_magic_new ();
    alias_list = _xdg_mime_alias_list_new ();
    parent_list = _xdg_mime_parent_list_new ();

    xdg_run_command_on_dirs ( ( XdgDirectoryFunc ) xdg_mime_init_from_directory,
                              NULL );
}

gboolean xdg_mime_reload_cache( gpointer user_data )
{
    XdgCallbackList * list;

    xdg_mime_shutdown();
    xdg_mime_init();
    update_timout = 0;

    for ( list = callback_list; list; list = list->next )
        ( list->callback ) ( list->data );

    return FALSE;
}

const char *
xdg_mime_get_mime_type_for_data ( const void *data,
                                  size_t len )
{
    const char * mime_type;

    if ( _caches )
        return _xdg_mime_cache_get_mime_type_for_data ( data, len );

    mime_type = _xdg_mime_magic_lookup_data ( global_magic, data, len, NULL, 0 );

    if ( mime_type )
        return mime_type;

    return XDG_MIME_TYPE_UNKNOWN;
}

/* 2006.02.26 added by Hong Jen Yee */
int xdg_mime_is_text_file( const char *file_path, const char* mime_type )
{
    int file;
    unsigned char data[ 256 ];
    int rlen;
    int i;
    int ret = 0;

    if ( mime_type
            && xdg_mime_mime_type_subclass( mime_type, XDG_MIME_TYPE_PLAIN_TEXT ) )
    {
        return 1;
    }

    if ( !file_path )
    {
        return 0;
    }

    file = open ( file_path, O_RDONLY );
    if ( file != -1 )
    {
        rlen = read ( file, data, sizeof( data ) );
        if ( rlen != -1 )
        {
            for ( i = 0; i < rlen; ++i )
            {
                if ( data[ i ] == '\0' )
                {
                    break;
                }
            }
            if ( i >= rlen )
            {
                ret = 1;
            }
        }
        close ( file );
    }
    return ret;
}

/* 2006.02.26 added by Hong Jen Yee */
int xdg_mime_is_executable_file( const char *file_path, const char* mime_type )
{
    int file;
    int i;
    int ret = 0;

    if ( !mime_type )
    {
        mime_type = xdg_mime_get_mime_type_for_file( file_path, NULL, NULL );
    }

    /*
    * Only executable types can be executale.
    * Since some common types, such as application/x-shellscript,
    * are not in mime database, we have to add them ourselves.
    */
    if ( xdg_mime_mime_type_subclass( mime_type, XDG_MIME_TYPE_EXECUTABLE ) ||
            xdg_mime_mime_type_subclass( mime_type, "application/x-shellscript" ) )
    {
        if ( file_path )
        {
            if ( ! g_file_test( file_path, G_FILE_TEST_IS_EXECUTABLE ) )
                return 0;
        }
        return 1;
    }
    return 0;
}

const char *
xdg_mime_get_mime_type_for_file ( const char *file_path,
                                  const char *base_name,
                                  struct stat *statbuf )
{
    const char *mime_type;
    /* currently, only a few globs occur twice, and none
     * more often, so 5 seems plenty.
     */
    const char *mime_types[ 5 ];
    int file;
    unsigned char data[ 256 ];
    int max_extent;
    int bytes_read;
    struct stat buf;
    int n;

    if ( file_path == NULL )
        return NULL;

    /*
    2006.01.07 modified by Hong Jen Yee
    if (! _xdg_utf8_validate (file_path))
      return NULL;
    */
    if ( _caches )
        return _xdg_mime_cache_get_mime_type_for_file ( file_path, statbuf );

    if ( !base_name )
        base_name = _xdg_get_base_name ( file_path );
    n = _xdg_glob_hash_lookup_file_name ( global_hash, base_name, mime_types, 5 );

    if ( n >= 1 && mime_types[ 0 ] && *mime_types[ 0 ] )
        return mime_types[ 0 ];

    if ( !statbuf )
    {
        if ( stat ( file_path, &buf ) != 0 )
            return XDG_MIME_TYPE_UNKNOWN;
        statbuf = &buf;
    }
    if ( S_ISDIR( statbuf->st_mode ) )
        return XDG_MIME_TYPE_DIRECTORY;

    if ( !S_ISREG ( statbuf->st_mode ) )
        return XDG_MIME_TYPE_UNKNOWN;

    /* FIXME: Need to make sure that max_extent isn't totally broken.  This could
     * be large and need getting from a stream instead of just reading it all
     * in. */
    /*
    max_extent = _xdg_mime_magic_get_buffer_extents (global_magic);
    data = malloc (max_extent);
    if (data == NULL)
      return XDG_MIME_TYPE_UNKNOWN;
    */

    file = open ( file_path, O_RDONLY );
    if ( file == -1 )
    {
        /* free (data); */
        return XDG_MIME_TYPE_UNKNOWN;
    }

    bytes_read = read ( file, data, sizeof( data ) );
    if ( bytes_read == -1 )
    {
        close ( file );
        return XDG_MIME_TYPE_UNKNOWN;
    }
    close ( file );

    mime_type = _xdg_mime_magic_lookup_data ( global_magic, data, bytes_read,
                                              mime_types, n );

    /* 2005.01.07 modified by Hong Jen Yee */
    /* Fallback for text files */
    if ( !mime_type )
    {
        for ( n = 0; n < bytes_read; ++n )
        {
            if ( data[ n ] == '\0' )
                break;
        }
        if ( n >= bytes_read )
        {
            mime_type = XDG_MIME_TYPE_PLAIN_TEXT;
        }
    }

    if ( mime_type )
        return mime_type;

    return XDG_MIME_TYPE_UNKNOWN;
}

const char*
xdg_mime_get_mime_type_from_file_name ( const char *file_name )
{
    const char* mime_types[ 2 ];

    if ( _caches )
        return _xdg_mime_cache_get_mime_type_from_file_name ( file_name );

    if ( _xdg_glob_hash_lookup_file_name ( global_hash, file_name, mime_types, 2 ) == 1
            && mime_types[ 0 ] && *mime_types[ 0 ] )
        return mime_types[ 0 ];
    else
        return XDG_MIME_TYPE_UNKNOWN;
}

int
xdg_mime_is_valid_mime_type ( const char *mime_type )
{
    /* FIXME: We should make this a better test
     */ 
    return _xdg_utf8_validate ( mime_type );
}

void
xdg_mime_shutdown ( void )
{
    GSList* l;

    for ( l = monitor_list; l; l = l->next )
    {
	if ( l->data )
            vfs_file_monitor_remove( ( VFSFileMonitor* ) l->data,
                                     xdg_mime_file_changed, NULL );
    }
    g_slist_free( monitor_list );
    monitor_list = NULL;

    if ( global_hash )
    {
        _xdg_glob_hash_free ( global_hash );
        global_hash = NULL;
    }
    if ( global_magic )
    {
        _xdg_mime_magic_free ( global_magic );
        global_magic = NULL;
    }

    if ( alias_list )
    {
        _xdg_mime_alias_list_free ( alias_list );
        alias_list = NULL;
    }

    if ( parent_list )
    {
        _xdg_mime_parent_list_free ( parent_list );
        parent_list = NULL;
    }
}

int
xdg_mime_get_max_buffer_extents ( void )
{
    if ( _caches )
        return _xdg_mime_cache_get_max_buffer_extents ();

    return _xdg_mime_magic_get_buffer_extents ( global_magic );
}

const char *
xdg_mime_unalias_mime_type ( const char *mime_type )
{
    const char * lookup;

    if ( _caches )
        return _xdg_mime_cache_unalias_mime_type ( mime_type );

    if ( ( lookup = _xdg_mime_alias_list_lookup ( alias_list, mime_type ) ) != NULL )
        return lookup;

    return mime_type;
}

int
xdg_mime_mime_type_equal ( const char *mime_a,
                           const char *mime_b )
{
    const char * unalias_a, *unalias_b;

    unalias_a = xdg_mime_unalias_mime_type ( mime_a );
    unalias_b = xdg_mime_unalias_mime_type ( mime_b );

    if ( strcmp ( unalias_a, unalias_b ) == 0 )
        return 1;

    return 0;
}

int
xdg_mime_media_type_equal ( const char *mime_a,
                            const char *mime_b )
{
    char * sep;

    sep = strchr ( mime_a, '/' );

    if ( sep && strncmp ( mime_a, mime_b, sep - mime_a + 1 ) == 0 )
        return 1;

    return 0;
}

#if 0
static int
xdg_mime_is_super_type ( const char *mime )
{
    int length;
    const char *type;

    length = strlen ( mime );
    type = &( mime[ length - 2 ] );

    if ( strcmp ( type, "/*" ) == 0 )
        return 1;

    return 0;
}
#endif

int
xdg_mime_mime_type_subclass ( const char *mime,
                              const char *base )
{
    const char * umime, *ubase;
    const char **parents;

    if ( _caches )
        return _xdg_mime_cache_mime_type_subclass ( mime, base );

    umime = xdg_mime_unalias_mime_type ( mime );
    ubase = xdg_mime_unalias_mime_type ( base );

    if ( strcmp ( umime, ubase ) == 0 )
        return 1;

#if 0
    /* Handle supertypes */
    if ( xdg_mime_is_super_type ( ubase ) &&
            xdg_mime_media_type_equal ( umime, ubase ) )
        return 1;
#endif

    /*  Handle special cases text/plain and application/octet-stream */
    if ( strcmp ( ubase, XDG_MIME_TYPE_PLAIN_TEXT ) == 0 &&
            strncmp ( umime, "text/", 5 ) == 0 )
        return 1;

    if ( strcmp ( ubase, XDG_MIME_TYPE_UNKNOWN ) == 0 )
        return 1;

    parents = _xdg_mime_parent_list_lookup ( parent_list, umime );
    for ( ; parents && *parents; parents++ )
    {
        if ( xdg_mime_mime_type_subclass ( *parents, ubase ) )
            return 1;
    }

    return 0;
}

char **
xdg_mime_list_mime_parents ( const char *mime )
{
    const char **parents;
    char **result;
    int i, n;

    if ( _caches )
        return _xdg_mime_cache_list_mime_parents ( mime );

    parents = xdg_mime_get_mime_parents ( mime );

    if ( !parents )
        return NULL;

    for ( i = 0; parents[ i ]; i++ )
        ;

    n = ( i + 1 ) * sizeof ( char * );
    result = ( char ** ) malloc ( n );
    memcpy ( result, parents, n );
    result[ i ] = NULL;

    return result;
}

const char **
xdg_mime_get_mime_parents ( const char *mime )
{
    const char * umime;
    umime = xdg_mime_unalias_mime_type ( mime );
    return _xdg_mime_parent_list_lookup ( parent_list, umime );
}

#if  0
void
xdg_mime_dump ( void )
{
    printf ( "*** ALIASES ***\n\n" );
    _xdg_mime_alias_list_dump ( alias_list );
    printf ( "\n*** PARENTS ***\n\n" );
    _xdg_mime_parent_list_dump ( parent_list );
}
#endif

/* Registers a function to be called every time the mime database reloads its files
 */
int
xdg_mime_register_reload_callback ( XdgMimeCallback callback,
                                    void *data,
                                    XdgMimeDestroy destroy )
{
    XdgCallbackList *list_el, *tail;
    static int callback_id = 1;

    /* Make a new list element */
    list_el = calloc ( 1, sizeof ( XdgCallbackList ) );
    list_el->callback_id = callback_id;
    list_el->callback = callback;
    list_el->data = data;
    list_el->destroy = destroy;

    /* Find tail of the list, added by Hong Jen Yee 2006.08.01 */
    if( !callback_list )
        callback_list = list_el;
    else
    {
        tail = callback_list;
        while( tail->next )
            tail = tail->next;
        list_el->prev = tail;
        tail->next = list_el;
    }
    callback_id ++;

    return callback_id - 1;
}

void
xdg_mime_remove_callback ( int callback_id )
{
    XdgCallbackList * list;

    for ( list = callback_list; list; list = list->next )
    {
        if ( list->callback_id == callback_id )
        {
            if ( list->next )
                list->next = list->prev;

            if ( list->prev )
                list->prev->next = list->next;
            else
                callback_list = list->next;

            /* invoke the destroy handler */
            if ( list->destroy )
            {
                ( list->destroy ) ( list->data );
            }
            free ( list );
            return ;
        }
    }
}
