/*
 *      test.c
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

#include <stdio.h>

#include "mime-type.h"
#include "mime-cache.h"

static void func( MimeCache* cache, gpointer user_data )
{
    g_print( "%s\n", cache->file_path );
}

int main(int argc, char** argv)
{
    const char* type;
    char* desc;

    mime_type_init();
//    mime_cache_foreach( (GFunc)func, NULL );

    type = mime_type_get_by_file( argv[1], NULL, NULL );
    desc = mime_type_get_desc(type, NULL);
    printf("mime-type of %s:\nType: %s\nDescription: %s\n", argv[1], type, desc );
    test_alias( "video/x-ogg" );
    test_parents( "application/x-awk" );
    g_free(desc);

    mime_type_finalize();
    return 0;
}
