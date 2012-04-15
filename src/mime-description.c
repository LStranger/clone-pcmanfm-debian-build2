/*
*  C Implementation: mimedescription
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "mime-description.h"
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static GHashTable *mime_table = NULL;

void mime_description_init()
{
  mime_table = g_hash_table_new_full( g_direct_hash,
                                      NULL,
                                      NULL,
                                      (GDestroyNotify)g_free );
}

void mime_description_clean()
{
  g_hash_table_destroy( mime_table );
}

/* Get human-readable description of mime type */
const char* get_mime_description( const char* mime_type )
{
  int fd;
  struct stat file_stat;
  const char** lang;
  char full_path[256];
  char* buf = NULL;
  char* eng_desc = NULL;
  char* tmp;
  int len;
  char* desc = g_hash_table_lookup( mime_table, mime_type );

  if( desc ){
    return desc;
  }
  /* FIXME: This path shouldn't be hard-coded. */
  sprintf( full_path, "/usr/share/mime/%s.xml", mime_type );

  fd = open( full_path, O_RDONLY );
  if( fd != -1 )
  {
    fstat( fd, &file_stat);
    if( file_stat.st_size > 0 )
    {
      buf = g_malloc( file_stat.st_size + 1);
      read( fd, buf, file_stat.st_size );
      buf[ file_stat.st_size ] = '\0';
      eng_desc = strstr( buf, "<comment>" );
      if( eng_desc )
      {
        eng_desc += 9;
        for( desc = eng_desc; *desc != '\n' && *desc; ++desc )
          ;
        while( desc = strstr( desc, "<comment xml:lang=" ) )
        {
          if( !desc )
            break;

          desc += 19;
          lang = g_get_language_names();
	  tmp = strchr( lang[0], '.' );
          len = tmp ? ((int)tmp - (int)lang[0]) : strlen( lang[0] );
          
	  if( lang && 0 == strncmp( desc, lang[0], len ) )
          {
            desc += (len + 2);
            break;
          }
          while( *desc != '\n' && *desc )
            ++desc;
        }
      }
    }
    close( fd );
  }

  if( !desc ){
    desc = eng_desc;
    if( !desc )
      return mime_type;
  }

  if( desc ) {
    if( tmp = strstr(desc, "</") )
      *tmp = '\0';
    desc = g_strdup( desc );
    g_hash_table_insert( mime_table, mime_type, desc );
  }
  if( buf ) {
    g_free(buf);
  }
  return desc;
}
