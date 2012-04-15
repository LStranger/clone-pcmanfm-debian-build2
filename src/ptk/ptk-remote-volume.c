/*
 *      ptk-remote-fs-dlg.c
 *      
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#include "ptk-remote-volume.h"
#include "ptk-ui-xml.h"

typedef struct
{
    GtkWidget* dlg;
    GtkWidget* name;
    GtkWidget* host;
    GtkWidget* port;
    GtkWidget* server_type;
    GtkWidget* user;
    GtkWidget* passwd_label;
    GtkWidget* passwd;
    GtkWidget* encoding;
    GtkWidget* init_dir;
    GtkWidget* proxy_page;
    GtkWidget* proxy_type;
    GtkWidget* proxy_host;
    GtkWidget* proxy_port;
    GtkWidget* proxy_user;
    GtkWidget* proxy_passwd;
    VFSRemoteVolume* vol;
    int response;
}PtkRemoteDlg;

typedef struct {
    const char* protocol_name;
    guint default_port;
    const char* disp_name;
}SupportedFS;

enum {
    PROT_FTP,
    PROT_FTPS,
    PROT_SFTP,
    /* PROT_HTTP, */
    N_SUPPORTED_FS
};

SupportedFS supported_fs[] = {
    {"ftp", 21, N_("FTP - File Transfer Protocol")},
    {"ftps", 990, N_("FTPS - FTP over implicit SSL/TLS")},
    {"sftp", 22, N_("SFTP - SSH File Transfer Protocol")},
    /* {"http", 80, N_("HTTP - WebDAV HTTP Extension")}, */
};

enum {
    PROXY_HTTP,
    PROXY_SOCKS4,
    PROXY_SOCKS5
};

const char* supported_proxy[] = {
    "http", "socks4", "socks5"
};

static gboolean do_edit( VFSRemoteVolume* vol );

VFSRemoteVolume* ptk_remote_volume_add_new()
{
    VFSRemoteVolume* vol = vfs_remote_volume_new();
    return ptk_remote_volume_add( vol ) ? vol : NULL;
}

VFSRemoteVolume* ptk_remote_volume_add_from_url( const char* url )
{
    VFSRemoteVolume* vol = vfs_remote_volume_new();
    return ptk_remote_volume_add( vol ) ? vol : NULL;
}

gboolean ptk_remote_volume_add( VFSRemoteVolume* vol )
{
    VFSRemoteFSMgr* mgr = vfs_remote_fs_mgr_get();
    if( do_edit( vol ) )
        vfs_remote_fs_mgr_add_volume( mgr, vol );
    else
        vol = NULL;
    g_object_unref( mgr );

    return vol != NULL;
}

static void on_server_type_changed( GtkComboBox* server_type, PtkRemoteDlg* data )
{
    int sel = gtk_combo_box_get_active(server_type);
    if( sel == PROT_FTP || sel == PROT_FTPS )  /* ftp/ftps */
        gtk_widget_show( data->proxy_page );
    else
        gtk_widget_hide( data->proxy_page );

    if( sel == PROT_SFTP )
    {
        const char* user;
        user = gtk_entry_get_text( (GtkEntry*)data->user );
        if( ! user || ! *user )
            gtk_entry_set_text( (GtkEntry*)data->user, g_get_user_name() );
        gtk_widget_hide( data->passwd );
        gtk_widget_hide( data->passwd_label );
    }
    else
    {
        if( ! data->vol->user )
            gtk_entry_set_text( (GtkEntry*)data->user, "" );

        gtk_widget_show( data->passwd );
        gtk_widget_show( data->passwd_label );
    }

    if( data->vol->port == 0 )
    {
        char port_str[16];
        g_snprintf( port_str, 16, "%d", supported_fs[sel].default_port );
        gtk_entry_set_text( (GtkEntry*)data->port, port_str );
    }
}

#define GET_STR_FROM_DLG(var_name) \
        g_free( vol->var_name ); \
        tmp = gtk_entry_get_text( (GtkEntry*)data->var_name ); \
        vol->var_name = ( (tmp && *tmp) ? g_strdup( tmp ) : NULL);

static void on_response( GtkDialog* dlg, int response, PtkRemoteDlg* data )
{
    VFSRemoteVolume* vol = data->vol;
    data->response = response;
    if( response == GTK_RESPONSE_OK )
    {
        const char* tmp;
        int type, proxy_type;
        type = gtk_combo_box_get_active( (GtkComboBox*)data->server_type );

        if( ! ( tmp = gtk_entry_get_text( (GtkEntry*)data->name ) ) || ! *tmp )
        {
            ptk_show_error( dlg, _("Error"), _("Please give this network drive a name") );
            gtk_widget_grab_focus( data->name );
            g_signal_stop_emission_by_name( dlg, "response" );
            return;
        }

        if( ! ( tmp = gtk_entry_get_text( (GtkEntry*)data->host ) ) || ! *tmp )
        {
            ptk_show_error( dlg, _("Error"), _("Please input host address") );
            gtk_widget_grab_focus( data->host );
            g_signal_stop_emission_by_name( dlg, "response" );
            return;
        }

        g_free( vol->type );
        vol->type = g_strdup( supported_fs[type].protocol_name );

        GET_STR_FROM_DLG( name )
        GET_STR_FROM_DLG( host )

        if( (tmp = gtk_entry_get_text( (GtkEntry*)data->port )) && *tmp )
        {
            int port = atoi( tmp );
            if( port != supported_fs[type].default_port )
                vol->port = port;
            else
                vol->port = 0;
        }
        else
            vol->port = 0;

        GET_STR_FROM_DLG( user )
        GET_STR_FROM_DLG( passwd )

        GET_STR_FROM_DLG( init_dir )

        GET_STR_FROM_DLG( proxy_host )

        if( (tmp = gtk_entry_get_text( (GtkEntry*)data->proxy_port )) && *tmp )
        {
            int port = atoi( tmp );
            vol->proxy_port = port;
        }
        else
            vol->proxy_port = 0;

        proxy_type = gtk_combo_box_get_active( (GtkComboBox*)data->proxy_type );
        g_free( vol->proxy_type );
        if( proxy_type >= 0 )
            vol->proxy_type = g_strdup( supported_proxy[proxy_type] );
        else
            vol->proxy_type = NULL;

        GET_STR_FROM_DLG( proxy_user )
        GET_STR_FROM_DLG( proxy_passwd )

        g_free( vol->encoding );
        if( (tmp = gtk_entry_get_text( (GtkEntry*)data->encoding ) ) && *tmp
            && g_ascii_strcasecmp(tmp, "utf8") && g_ascii_strcasecmp(tmp, "utf-8") )
            vol->encoding = g_strdup( tmp );
        else
            vol->encoding = NULL;
    }
}

static void ptk_remote_dlg_free( PtkRemoteDlg* data )
{
    vfs_remote_volume_unref( data->vol );
    g_free( data );
}

static gboolean do_edit( VFSRemoteVolume* vol )
{
    PtkUIXml* xml;
    char port_str[16];
    int i, resp;
    PtkRemoteDlg* data = g_new( PtkRemoteDlg, 1 );
    data->vol = vfs_remote_volume_ref( vol );
    data->dlg = ptk_ui_xml_create_widget_from_file( PACKAGE_UI_DIR "/remote-fs-dlg.glade" );
    g_object_weak_ref( data->dlg, (GWeakNotify)ptk_remote_dlg_free, data );
    /* gtk_dialog_set_default_response( (GtkDialog*)data->dlg, GTK_RESPONSE_OK ); */

    xml = ptk_ui_xml_get(data->dlg);
    data->name = ptk_ui_xml_lookup( xml, "name" );
    data->host = ptk_ui_xml_lookup( xml, "host" );
    data->port = ptk_ui_xml_lookup( xml, "port" );
    data->server_type = ptk_ui_xml_lookup( xml, "server_type" );
    data->user = ptk_ui_xml_lookup( xml, "user" );
    data->passwd_label = ptk_ui_xml_lookup( xml, "passwd_label" );
    data->passwd = ptk_ui_xml_lookup( xml, "passwd" );
    data->encoding = ptk_ui_xml_lookup( xml, "encoding" );
    data->init_dir = ptk_ui_xml_lookup( xml, "init_dir" );
    data->proxy_page = ptk_ui_xml_lookup( xml, "proxy_page" );
    data->proxy_host = ptk_ui_xml_lookup( xml, "proxy_host" );
    data->proxy_type = ptk_ui_xml_lookup( xml, "proxy_type" );
    data->proxy_port = ptk_ui_xml_lookup( xml, "proxy_port" );
    data->proxy_user = ptk_ui_xml_lookup( xml, "proxy_user" );
    data->proxy_passwd = ptk_ui_xml_lookup( xml, "proxy_passwd" );

    if( vol->name )
        gtk_entry_set_text( (GtkEntry*)data->name, vol->name );
    if( vol->host )
        gtk_entry_set_text( (GtkEntry*)data->host, vol->host );

    if( vol->port )
    {
        g_snprintf( port_str, 16, "%d", vol->port );
        gtk_entry_set_text( (GtkEntry*)data->port, port_str );
    }

    g_signal_connect( data->server_type, "changed", G_CALLBACK(on_server_type_changed), data );

    for( i = 0; i < G_N_ELEMENTS(supported_fs); ++i )
    {
        gtk_combo_box_append_text( (GtkComboBox*)data->server_type, _(supported_fs[i].disp_name) );
        if( vol->type && strcmp( supported_fs[i].protocol_name, vol->type ) == 0 )
            gtk_combo_box_set_active( (GtkComboBox*)data->server_type, i );
    }
    if( gtk_combo_box_get_active( (GtkComboBox*)data->server_type ) == -1 )
        gtk_combo_box_set_active( (GtkComboBox*)data->server_type, 0 );

    if( vol->user )
    {
        gtk_entry_set_text( (GtkEntry*)data->user, vol->user );
        if( vol->passwd )
            gtk_entry_set_text( (GtkEntry*)data->passwd, vol->passwd );
    }

    if( vol->init_dir )
        gtk_entry_set_text( (GtkEntry*)data->init_dir, vol->init_dir );

    if( vol->encoding && g_ascii_strcasecmp(vol->encoding, "utf8") && g_ascii_strcasecmp(vol->encoding, "utf-8") )
        gtk_entry_set_text( (GtkEntry*)data->encoding, vol->encoding );

    if( vol->proxy_type )
    {
        for( i = 0; i < G_N_ELEMENTS(supported_proxy); ++i )
            if( strcmp( supported_proxy[i], vol->proxy_type ) == 0 )
                gtk_combo_box_set_active( data->proxy_type, i );      
    }

    if( vol->proxy_host )
        gtk_entry_set_text( (GtkEntry*)data->proxy_host, vol->proxy_host );

    if( vol->proxy_port > 0 )
    {
        g_snprintf( port_str, 16, "%d", vol->proxy_port );
        gtk_entry_set_text( (GtkEntry*)data->proxy_port, port_str );
    }

    if( vol->proxy_user )
        gtk_entry_set_text( (GtkEntry*)data->proxy_user, vol->proxy_user );
    if( vol->proxy_passwd )
        gtk_entry_set_text( (GtkEntry*)data->proxy_passwd, vol->proxy_passwd );

    g_signal_connect( data->dlg, "response", G_CALLBACK(on_response), data );

    resp = gtk_dialog_run( data->dlg );

    gtk_widget_destroy( data->dlg );

    return ( resp == GTK_RESPONSE_OK );
}

gboolean ptk_remote_volume_edit( VFSRemoteVolume* vol )
{
    if( do_edit(vol) )
    {
        VFSRemoteFSMgr* mgr = vfs_remote_fs_mgr_get();
        vfs_remote_fs_mgr_change_volume( mgr, vol );
        g_object_unref( mgr );
        return TRUE;
    }
    return FALSE;
}
