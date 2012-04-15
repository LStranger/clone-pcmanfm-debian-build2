/*
 *      mime.h
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

#ifndef _MIME_H_INCLUDED_
#define _MIME_H_INCLUDED_

#include <glib.h>

G_BEGIN_DECLS

void mime_init();
void mime_finalize();
void mime_reload();

const char* mime_get_type( const char* filename );
const char* mime_get_type_by_filename( const char* filename );
const char*  mime_get_type_by_magic( const char* file_path );
const char* mime_get_type_by_data( const char* data, int len );

G_END_DECLS

#endif
