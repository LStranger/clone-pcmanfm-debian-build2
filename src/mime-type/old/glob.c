/*
 *      glob.c
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

typedef struct _MimeGlob
{
    char* type_name;    /* mime-type */
    GPatternSpec* pattern;    /* The pattern */
    int pattern_len;    /* length of the pattern */
}MimeGlob;

// extern GSList* all_mime_types;    /* defined in mime.c */
GSList* all_globs = NULL;

void mime_load_globs( const char* path )
{
    MimeGlob* glob;
    char line[1024], *sep;
    int len;
    FILE *f;

    f = fopen( path, "r" );
    if( ! f ) return;

    while( fgets( line, 1024, f ) )
    {
        sep = strchr( line, ':' );
        if( G_UNLIKELY( !sep ) )
            continue;
        *sep = '\0';
        glob = g_slice_new( MimeGlob );
        glob->type_name = g_strdup( line );
        ++sep;
        len = strlen( sep );
        sep[ len-1 ] = '\0';    /* remove tailing \n */
        glob->pattern = g_pattern_spec_new( sep );
        glob->pattern_len = strlen( sep );
        all_globs = g_slist_prepend( all_globs, glob );
    }
    fclose(f);
}

static glob_compare( MimeGlob* glob1, MimeGlob* glob2 )
{
    return (glob2->pattern_len - glob1->pattern_len);
}

/* should be called after all globs are loaded
 * This will put globs with longer patterns before those
 * with shorter patterns, and make the match faster.
*/
void mime_glob_sort()
{
    all_globs = g_slist_sort( all_globs, (GCompareFunc)glob_compare );
}

const char* mime_glob_find( const char* name )
{
    GSList* l;
    for( l = all_globs; l; l = l->next )
    {
        MimeGlob* glob = (MimeGlob*)l->data;
        if( g_pattern_match_string( glob->pattern, name ) )
            return glob->type_name;
    }
}
