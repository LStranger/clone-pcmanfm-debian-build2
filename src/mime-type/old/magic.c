/*
 *      magic.c
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

#include "mime.h"
#include "magic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _MimeMagicRule
{
    guint indent;
    guint offset;
    guint16 len;
    char* value;
    char* mask;
    guchar word_size;
    guint range;
}MimeMagicRule;

typedef struct _MimeMagic
{
    char* type_name;
    int priority;
    GSList* rules;
}MimeMagic;


GSList* all_magic = NULL;
guint max_data_len = 0;


void swap_value( char* value, int len, int word_size )
{
    char* end = value + len;
    if( word_size == 2 )
    {
        for( ; value < end; value += 2 )
            *(guint16*)value = GUINT16_SWAP_LE_BE( *(guint16*)value );
    }
    else if( word_size == 4 )
    {
        for( ; value < end; value += 4 )
            *(guint32*)value = GUINT32_SWAP_LE_BE( *(guint32*)value );
    }
}

void mime_load_magic( const char* path )
{
    FILE *f;
    MimeMagic* magic = NULL;
    MimeMagicRule* rule = NULL;
    char line_buf[1024] ,*line, *sep;
    int data_len = 0;
    /* GArray* rules; */

    f = fopen( path, "r" );

    if( G_UNLIKELY( ! f || ! fgets( line_buf, 1024, f ) ) )    return;

    /* not magic file */
    if( G_UNLIKELY( strcmp( line_buf, "MIME-Magic" ) ) )    return;

    /* temp array to hold rules for mime-type */
    /* rules = g_array_sized_new(FALSE, TRUE, sizeof(MimeMagicRule*), 10); */

    while( fgets( line_buf, 1024, f ) )
    {
        if( line_buf[0] == '[' )    /* begin of mime-type rules */
        {
            if( G_LIKELY(magic) )    /* end previous magic */
            {
                magic->rules = g_slist_reverse( magic->rules );
                all_magic = g_slist_prepend( all_magic, magic );
                magic = NULL;
            }

            line = strtok( line_buf, "[]\n" );
            /* if this happens, it's a bug of shared mime database */
            if( G_UNLIKELY( ! line ) )
                continue;

            sep = strchr( line, ':' );
            if( G_UNLIKELY( ! sep ) )
                continue;

            *sep = '\0';
            magic = g_slice_new0( MimeMagic );
            magic->priority = atoi( line );
            magic->type_name = g_strdup( sep + 1 );
            /* g_array_set_size( rules, 0 ); */
        }
        else if( G_LIKELY(magic) )   /* rules of mime-type */
        {
            /* format of each line:
             * [indent]>offset=value[&mask][~word_size][+range]
            */
            sep = strchr( line_buf, '>' );
            if( G_UNLIKELY( ! sep ) )
                continue;
            *sep = '\0';
            rule = g_slice_new0(MimeMagicRule);
            if( isdigit(*line_buf) )    /* indent is defined */
                rule->indent = atoi( line_buf );    /* store indent */

            ++sep;
            line = sep;    /* offset */
            sep = strchr( line, '=' );
            if( G_UNLIKELY( ! sep ) )    /* prevent bug of the mime database */
            {
                g_slice_free( MimeMagicRule, rule );
                continue;
            }
            *sep = '\0';
            rule->offset = atoi( line );    /* store offset */
            line = sep + 1;    /* len of value */
            if( G_BYTE_ORDER == G_LITTLE_ENDIAN )
                rule->len = GUINT16_SWAP_LE_BE( *(guint16*)line );
            else
                rule->len = *(guint16*)line;
            line += 2;    /* skip len, get the value itself */
            rule->value = g_slice_alloc( rule->len );
            memcpy( rule->value, line, rule->len );
            line += rule->len;
            if( G_UNLIKELY( *line == '&' ) )    /* mask presents */
            {
                ++line;
                rule->mask = g_slice_alloc( rule->len );
                memcpy( rule->mask, line, rule->len );
                line += rule->len;
            }

            if( G_UNLIKELY( *line == '~' ) )    /* word_size presents */
            {
                ++line;
                /* this value doesn't seem to exceed 255, so guchar should be OK. */
                rule->word_size = (guchar)atoi( line );
                while( isdigit( *line ) )
                    ++line;
            }
            else
            {
                /* range is default to 1 according to freedesktop.org */
                rule->word_size = 1;
            }

            if( G_UNLIKELY( *line == '+' ) )    /* range presents */
            {
                ++line;
                rule->range = atoi( line );
            }
            else
            {
                /* range is default to 1 according to freedesktop.org */
                rule->range = 1;
            }
            /* g_array_append_val( rules, rule ); */

            /* need to swap bytes in value and mask here */
            if( G_BYTE_ORDER == G_LITTLE_ENDIAN )
            {
                if( G_UNLIKELY( rule->word_size > 1 ) && G_LIKELY( rule->word_size <= rule->len ) )
                {
                    swap_value( rule->value, rule->len, rule->word_size );
                    if( rule->mask )
                        swap_value( rule->mask, rule->len, rule->word_size );
                }
            }

            magic->rules = g_slist_prepend( magic->rules, rule );
            data_len = rule->offset + rule->range + rule->len;
            if( data_len > max_data_len )
                max_data_len = data_len;
        }
    }
    if( magic )
    {
        magic->rules = g_slist_reverse( magic->rules );
        all_magic = g_slist_prepend( all_magic, magic );
    }

    /* g_array_free( rules, TRUE ); */

    fclose(f);
}

static int magic_compare( MimeMagic* magic1, MimeMagic* magic2 )
{
    return (magic2->priority - magic1->priority);
}

void mime_magic_sort()
{
    all_magic = g_slist_sort( all_magic, (GCompareFunc)magic_compare );
}

gboolean magic_match( MimeMagic* magic, const char* data, int len )
{
    GSList* l;
    MimeMagicRule* rule;
    int indent = 0, offset, max_offset;
    gboolean match = FALSE;

    for( l = magic->rules; l; l = l->next )
    {
        rule = (MimeMagicRule*)l->data;
        if( rule->indent < indent )
        {
            if( match )
                return TRUE;
            match = FALSE;
            indent = 0;
            continue;
        }

        offset = rule->offset;
        max_offset = offset + rule->range;

        for( ; offset < max_offset; ++offset )
        {
            if( (offset + rule->len) > len )    /* the data is shorter than the magic pattern */
                break;

            if( G_UNLIKELY( rule->mask ) )    /* compare with mask applied */
            {
                int i = 0;
                for( ; i < rule->len; ++i )
                {
                    if( data[offset + i] & rule->mask[i] != rule->value[i] )
                        break;
                }
                if( i >= rule->len )
                {
                    ++indent;
                    match = TRUE;
                }
            }
            else if( 0 == memcmp( rule->value, data + offset, rule->len ) )    /* direct comparison */
            {
                ++indent;
                match = TRUE;
                break;
            }
        }
    }
    return match;
}

const char* mime_get_type_by_data( const char* data, int len )
{
    GSList* l;
    for( l = all_magic; l; l = l->next )
    {
        MimeMagic* magic = (MimeMagic*)l->data;
        if( magic_match( magic, data, len ) )
            return magic->type_name;
    }
    return NULL;
}

const char*  mime_get_type_by_magic( const char* file_path )
{
    /* FIXME: use mmap() or open() to optimize here. */
    const char* type;
    char* buf;
    FILE* f = fopen( file_path, "rb" );
    if( ! f )
        return NULL;

    buf = g_malloc( max_data_len );
    fread( buf, 1, max_data_len,  f );
    type = mime_get_type_by_data( buf, max_data_len );
    g_free( buf );
    fclose( f );

    return type ? type : NULL;
}

/*
void dump()
{
    GSList* l;
    for( l = all_magic; l; l = l->next )
    {
        MimeMagic* magic = l->data;
        g_debug(magic->type_name);
    }

    g_debug( "%s", magic->type_name );
    GSList* l;
    for( l = magic->rules; l; l = l->next )
    {
        rule =(MimeMagicRule*) l->data;
        g_debug("  indent=%d, len=%d", rule->indent, rule->len);
    }
    g_debug("\n");
}
*/
