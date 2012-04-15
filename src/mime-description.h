/*
*  C Interface: mimedescription
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _MIME_DESCRIPTION_H_
#define _MIME_DESCRIPTION_H_

#include <glib.h>

G_BEGIN_DECLS

void mime_description_init();
void mime_description_clean();

/* Get human-readable description of mime type */
const char* get_mime_description( const char* mime_type );

G_END_DECLS
#endif

