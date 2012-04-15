/*
 *      mime.c
 *      
 *      Copyright 2007 Houng Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "glob.h"
#include "magic.h"

GSList* all_mime_types = NULL;
GHashTable* all_subclasses = NULL;
GHashTable* all_aliases = NULL;

const char default_mime_type[]="application/octet-stream";


void mime_load_hash( GHashTable* hash, const char* path )
{
    char line[1024];
    FILE *f;
    char *type1, *type2;

    f = fopen( path, "r" );
    if( ! f )    return;
    while( fgets( line, 1024, f ) )
    {
        type1 = strtok( line, " \n" );
        type2 = strtok( NULL, "\n" );
        g_hash_table_insert( hash, g_strdup(type1), g_strdup(type2) );
    }
    fclose(f);
}

const char* mime_get_type_by_filename( const char* filename )
{
    const char* type;
    type = mime_glob_find( filename );
    if( type )
        return type;
    return default_mime_type;
}

const char* mime_get_type( const char* filename )
{
    const char* type;
    type = mime_glob_find( filename );
    
    return default_mime_type;
}

void mime_load_from_dir( const char* dir )
{
    char* path;
    path = g_build_filename( dir, "/mime/globs", NULL );
    mime_load_globs( path );
    g_free( path );
    mime_glob_sort();    /* sort to optimize the matching */

    path = g_build_filename( dir, "/mime/magic", NULL );
    mime_load_magic( path );
    mime_magic_sort();
    g_free( path );

    all_aliases = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, g_free );
    path = g_build_filename( dir, "/mime/aliases", NULL );
    mime_load_hash( all_aliases, path );
    g_free( path );

    all_subclasses = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, g_free );
    path = g_build_filename( dir, "/mime/subclasses", NULL );
    mime_load_hash( all_subclasses, path );
    g_free( path );
}

void mime_init()
{
    const char* const * dirs;
    const char* const * dir;
    dirs = g_get_system_data_dirs();
    for( dir = dirs; *dir; ++dir )
        mime_load_from_dir( *dir );
    mime_load_from_dir( g_get_user_data_dir() );
}

static void mime_free()
{
    
}

void mime_finalize()
{
    mime_free();
}

int main(int argc, char** argv)
{
    if( argc < 2 )
        return 1;
    mime_init();
    g_debug("mime-type of %s: %s\n", argv[1], mime_get_type_by_filename(argv[1]) );
    g_debug("mime-type of %s: %s\n", argv[1], mime_get_type_by_magic(argv[1]) );
    mime_finalize();
    return 0;
}
