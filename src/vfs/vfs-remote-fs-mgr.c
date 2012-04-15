/*
 *      vfs-remote-fs-mgr.c
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
#include <glib/gstdio.h>
#include <string.h>
#include <stdio.h>

/* for chmod() */
#include <sys/types.h>
#include <sys/stat.h>

#include "vfs-remote-fs-mgr.h"
#include "glib-mem.h"
#include "glib-utils.h"

#define CONFIG_FILE_NAME    "remote-fs.list"

#define PROG_NOT_FOUND_MSG _("This function requires program '%s', but it's not found on the system.")

/*
typedef struct _VFSRemoteFS VFSRemoteFS;

struct _VFSRemoteFS{
    const char* name;
    const char* cmd;
    const char* encoding_param;
    const char* proxy_param;
};
*/

enum {
    ADD_SIGNAL,
    REMOVE_SIGNAL,
    CHANGE_SIGNAL,
    MOUNT_SIGNAL,
    UNMOUNT_SIGNAL,
    N_SIGNALS
};

static GObject*	vfs_remote_fs_mgr_new			(void);

static void vfs_remote_fs_mgr_class_init			(VFSRemoteFSMgrClass *klass);
static void vfs_remote_fs_mgr_init				(VFSRemoteFSMgr *self);
static void vfs_remote_fs_mgr_finalize			(GObject *object);

static void load_config( VFSRemoteFSMgr* mgr );
static void save_config( VFSRemoteFSMgr* mgr );


/* Local data */
static GObjectClass *parent_class = NULL;
static guint signals[N_SIGNALS];

static VFSRemoteFSMgr* mgr= NULL;

GType vfs_remote_fs_mgr_get_type(void)
{
	static GType self_type = 0;
	if (! self_type)
	{
		static const GTypeInfo self_info =
		{
			sizeof(VFSRemoteFSMgrClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)vfs_remote_fs_mgr_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(VFSRemoteFSMgr),
			0,
			(GInstanceInitFunc)vfs_remote_fs_mgr_init,
			NULL /* value_table */
		};

		self_type = g_type_register_static(G_TYPE_OBJECT, "VFSRemoteFSMgr", &self_info, 0);	}

	return self_type;
}

static void vfs_remote_fs_mgr_class_init(VFSRemoteFSMgrClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = vfs_remote_fs_mgr_finalize;

	parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);


    signals[ ADD_SIGNAL ] =
        g_signal_new ( "add",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSRemoteFSMgrClass, add ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ REMOVE_SIGNAL ] =
        g_signal_new ( "remove",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSRemoteFSMgrClass, remove ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ CHANGE_SIGNAL ] =
        g_signal_new ( "change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSRemoteFSMgrClass, change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ MOUNT_SIGNAL ] =
        g_signal_new ( "mount",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSRemoteFSMgrClass, mount ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ UNMOUNT_SIGNAL ] =
        g_signal_new ( "unmount",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSRemoteFSMgrClass, unmount ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
}

static void vfs_remote_fs_mgr_init(VFSRemoteFSMgr *self)
{
	load_config( self );
}


GObject* vfs_remote_fs_mgr_new(void)
{
	return (GObject*)g_object_new(VFS_REMOTE_FS_MGR_TYPE, NULL);
}


void vfs_remote_fs_mgr_finalize(GObject *object)
{
	VFSRemoteFSMgr *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_VFS_REMOTE_FS_MGR(object));

	self = VFS_REMOTE_FS_MGR(object);

	if (G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

VFSRemoteFSMgr* vfs_remote_fs_mgr_get()    /* manually creation is prohibited */
{
    if( G_UNLIKELY( ! mgr ) )
    {
        mgr = vfs_remote_fs_mgr_new();
        g_object_add_weak_pointer( mgr, &mgr );
        /* keep the object alive */
        g_object_ref( mgr );
    }
    else
        g_object_ref( mgr );
    return mgr;
}

GList* vfs_remote_fs_mgr_get_volumes( VFSRemoteFSMgr* mgr )
{
    return mgr->vols;
}

/* Format of the config file:
 * Section name is the URL of the site.
 * [PROTOCOL://[USER@]HOST[:PORT]/INIT_PATH]
 * name=NAME
 * encoding=ENCODING
*/
void load_config( VFSRemoteFSMgr* mgr )
{
    char* file = g_build_filename( g_get_user_config_dir(), "pcmanfm", CONFIG_FILE_NAME, NULL );
    GKeyFile* kf = g_key_file_new();
    VFSRemoteVolume* vol;

    if( g_key_file_load_from_file( kf, file, 0, NULL ) )
    {
        gsize n, i;
        char** urls = g_key_file_get_groups( kf, &n );
        for( i = 0; i < n; ++i )
        {
            vol = vfs_remote_volume_new_from_url( urls[i] );
            vol->name = g_key_file_get_string( kf, urls[i], "name", NULL );
            vol->passwd = g_key_file_get_string( kf, urls[i], "passwd", NULL );
            vol->encoding = g_key_file_get_string( kf, urls[i], "encoding", NULL );

            vol->proxy_type = g_key_file_get_string( kf, urls[i], "proxy_type", NULL );
            vol->proxy_host = g_key_file_get_string( kf, urls[i], "proxy_host", NULL );
            vol->proxy_port = g_key_file_get_integer( kf, urls[i], "proxy_port", NULL );
            vol->proxy_user = g_key_file_get_string( kf, urls[i], "proxy_user", NULL );
            vol->proxy_passwd = g_key_file_get_string( kf, urls[i], "proxy_passwd", NULL );

            mgr->vols = g_list_append( mgr->vols, vol );
            /*
            vfs_remote_fs_mgr_add_volume( mgr, vol );
            vfs_remote_volume_unref( vol );
            */
        }
        g_strfreev( urls );
    }
    g_key_file_free(kf);
    g_free(file);

    /* Add fusesmb automatically */
    vol = vfs_remote_volume_new();
    vol->name = g_strdup( _("Network Neighbourhood") );
    vol->type = g_strdup( "smb" );
    mgr->vols = g_list_append( mgr->vols, vol );
}

/* Format of the config file:
 * Section name is the URL of the site.
 * [PROTOCOL://[USER@]HOST[:PORT]/INIT_PATH]
 * name=NAME
 * encoding=ENCODING
*/
void save_config( VFSRemoteFSMgr* mgr )
{
    char* file;
    FILE* f;
    GList* l;

    file = g_build_filename( g_get_user_config_dir(), "pcmanfm", CONFIG_FILE_NAME, NULL );
    f = fopen( file, "w" );
    if( f )
        chmod( file, 0600 );

    g_free( file );

    if( f )
    {
        for( l = mgr->vols; l; l = l->next )
        {
            VFSRemoteVolume* vol = (VFSRemoteVolume*)l->data;
            /* smb is built-in */
            if( 0 == strcmp( vol->type, "smb" ) )
                continue;
            fprintf( f, "[%s://", vol->type );
            if( vol->user )
                fprintf( f, "%s@", vol->user );
            fputs( vol->host, f );
            if( vol->port > 0 )
                fprintf( f, ":%d", vol->port );
            if( vol->init_dir )
                fprintf( f, "%s]\n", vol->init_dir );
            else
                fputs( "/]\n", f );
            if( vol->name )
                fprintf( f, "name=%s\n", vol->name );
            if( vol->passwd )
                fprintf( f, "passwd=%s\n", vol->passwd );
            if( vol->encoding )
                fprintf( f, "encoding=%s\n", vol->encoding );
            if( vol->proxy_type )
                fprintf( f, "proxy_type=%s\n", vol->proxy_type );
            if( vol->proxy_host )
                fprintf( f, "proxy_host=%s\n", vol->proxy_host );
            if( vol->proxy_port > 0 )
                fprintf( f, "proxy_port=%d\n", vol->proxy_port );
            if( vol->proxy_user )
                fprintf( f, "proxy_user=%s\n", vol->proxy_user );
            if( vol->proxy_passwd )
                fprintf( f, "proxy_passwd=%s\n", vol->proxy_passwd );
            fputc( '\n', f );
        }

        fclose( f );
    }
}

void write_config( VFSRemoteFSMgr* mgr )
{
    char* file = g_build_filename( g_get_user_config_dir(), "pcmanfm", CONFIG_FILE_NAME, NULL );

    g_free(file);
}

VFSRemoteVolume* vfs_remote_volume_new()
{
    VFSRemoteVolume* vol = g_slice_new0( VFSRemoteVolume );
    vol->n_ref = 1;
    return vol;
}

VFSRemoteVolume* vfs_remote_volume_new_from_url( const char* url )
{
    const char* sep;
    VFSRemoteVolume* vol = g_slice_new0( VFSRemoteVolume );
    vol->n_ref = 1;

    if( sep = strstr( url, "://" ) )
    {
        vol->type = g_strndup( url, (sep - url) );
        url = sep + 3;
        if( sep = strchr(url, '@') )
        {
            vol->user = g_strndup( url, (sep - url) );
            url = sep + 1;
        }
        if( sep = strchr(url, '/') )
        {
            char* port_str;
            vol->host = g_strndup( url, (sep - url) );
            if( port_str = strchr(vol->host, ':') )
            {
                vol->port = (gushort)atoi(port_str+1);
                *port_str = '\0';
            }
            vol->init_dir = g_strdup(sep);
        }
        else
        {
            vol->host = g_strdup( url );
        }
        /*
        g_debug( vol->type );
        g_debug( vol->host );
        g_debug( vol->init_dir );
        g_debug( "%d", vol->port );
        */
    }
    return vol;
}

VFSRemoteVolume* vfs_remote_volume_ref( VFSRemoteVolume* vol )
{
    g_atomic_int_inc( &vol->n_ref );
    return vol;
}

void vfs_remote_volume_unref( VFSRemoteVolume* vol )
{
    if( g_atomic_int_dec_and_test( &vol->n_ref ) )
    {
        g_free( vol->name );
        g_free( vol->host );
        g_free( vol->user );
        g_free( vol->type );
        g_free( vol->init_dir );
        g_slice_free( VFSRemoteVolume, vol );
    }
}

void vfs_remote_fs_mgr_add_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol )
{
    GList* smb = g_list_last( mgr->vols );
    mgr->vols = g_list_insert_before( mgr->vols, smb, vfs_remote_volume_ref(vol) );
    save_config( mgr );
    g_signal_emit( mgr, signals[ADD_SIGNAL], 0, vol );
}

void vfs_remote_fs_mgr_remove_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol )
{
    mgr->vols = g_list_remove( mgr->vols, vol );
    save_config( mgr );
    g_signal_emit( mgr, signals[REMOVE_SIGNAL], 0, vol );
    vfs_remote_volume_unref( vol );
}

static void on_mount_completed( GPid pid, gint status, gpointer user_data )
{
    gint* pstatus = (gint*)user_data;
    *pstatus = status;
    g_spawn_close_pid( pid );
    gtk_main_quit();
}

void vfs_remote_fs_mgr_change_volume( VFSRemoteFSMgr* mgr, VFSRemoteVolume* vol )
{
    save_config( mgr );
    g_signal_emit( mgr, signals[CHANGE_SIGNAL], 0, vol );
}

gboolean vfs_remote_volume_mount( VFSRemoteVolume* vol, GError** err )
{
    extern char** environ;
    char **argv, **envp = NULL;
    GPid pid;
    GString* cmd;
    gboolean success;
    int status;

    if( vfs_remote_volume_is_mounted(vol) )
        return TRUE;

    cmd = g_string_sized_new(512);
    if( strcmp( vol->type , "ssh" ) == 0 || strcmp( vol->type , "sftp" ) == 0 )
    {
        /* need to modify environment variables */
        int i, n = g_strv_length( environ );
        envp = g_new0( char*, n + 2 );
        for( i = 0; i < n; ++i )
            envp[i] = environ[i];
        envp[i++] = "SSH_ASKPASS=pcmanfm-ask-pass";
        envp[i] = NULL;

        g_string_assign( cmd, "sshfs " );
        if( vol->user )
        {
            g_string_append( cmd, vol->user );
            g_string_append_c( cmd, '@' );
        }
        g_string_append( cmd, vol->host );
        g_string_append_c( cmd, ':');

        g_string_append( cmd, vol->init_dir ? vol->init_dir : "/" );
        g_string_append_c( cmd, ' ');
        if( vol->port )
            g_string_append_printf( cmd, "-p %d ",  vol->port );
        if( vol->encoding &&
            g_ascii_strcasecmp( vol->encoding, "utf8" ) &&
            g_ascii_strcasecmp( vol->encoding, "utf-8" ) )
        {
            g_string_append_printf( cmd, "-o modules=iconv,from_code=%s,to_code=utf8 ",  vol->encoding );
        }

        if( ! g_file_test( vfs_remote_volume_get_mount_point( vol ), G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( vfs_remote_volume_get_mount_point( vol ), 0700 );
        g_string_append_printf( cmd, "'%s'", vfs_remote_volume_get_mount_point( vol ) );
    }
    else if( strcmp( vol->type , "ftp" ) == 0 || strcmp( vol->type , "ftps" ) == 0 )
    {
        g_string_assign( cmd, "curlftpfs -o " );
        if( vol->user )
        {
            /*
             * How to handle ftp password and how to store it in ~/.netrc?
            g_string_append_printf( cmd, "user=%s:%s,", vol->user, vol->passwd );
            */
            /* FIXME: this is very insecure :-(. We need to patch curlftpfs. */
            g_string_append_printf( cmd, "user=%s:%s", vol->user, vol->passwd );
        }
        else
        {
            g_string_append( cmd, "user=anonymous:nobody@nohost" );
        }

        if( vol->proxy_host )
        {
            if( 0 == strcmp("http", vol->proxy_type) )
                g_string_append(cmd, ",httpproxy");
            else if( 0 == strcmp("socks5", vol->proxy_type) )
                g_string_append(cmd, ",socks5");
            else if( 0 == strcmp("socks4", vol->proxy_type) )
                g_string_append(cmd, ",socks4");

            g_string_append_printf( cmd, ",proxy_anyauth,proxy=%s", vol->proxy_host );
            if( vol->proxy_port > 0 )
                g_string_append_printf( cmd, ":%d", vol->proxy_port );

            if( vol->proxy_user )
                g_string_append_printf( cmd, ",proxy_user=%s:%s", vol->proxy_user, vol->proxy_passwd );
        }

        if( vol->encoding &&
            g_ascii_strcasecmp( vol->encoding, "utf8" ) &&
            g_ascii_strcasecmp( vol->encoding, "utf-8" ) )
        {
            g_string_append_printf( cmd, ",codepage=%s ",  vol->encoding );
        }

        g_string_append_printf( cmd, " %s://%s", vol->type, vol->host );

        if( vol->port )
            g_string_append_printf( cmd, ":%d",  vol->port );

        g_string_append_printf( cmd, "%s", vol->init_dir ? vol->init_dir : "/" );
        g_string_append_c( cmd, ' ');

        if( ! g_file_test( vfs_remote_volume_get_mount_point( vol ), G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( vfs_remote_volume_get_mount_point( vol ), 0700 );
        g_string_append_printf( cmd, "'%s'", vfs_remote_volume_get_mount_point( vol ) );
    }
    else if( strcmp( vol->type , "http" ) == 0 || strcmp( vol->type , "https" ) == 0 )
    {
        g_string_assign( cmd, "wdfs " );

        /* FIXME: this is very insecure */
        if( vol->user )
            g_string_append_printf( cmd, "-o 'username=%s,password=%s' ", vol->user, vol->passwd );

        g_string_append_c( cmd, '\'');
        g_string_append( cmd, vol->type );
        g_string_append( cmd, "://");
        g_string_append( cmd, vol->host );
        if( vol->port && vol->port != 80 )
            g_string_append_printf( cmd, ":%d",  vol->port );
        g_string_append( cmd, vol->init_dir ? vol->init_dir : "/" );
        g_string_append_c( cmd, '\'');
        g_string_append_c( cmd, ' ');

        if( vol->encoding &&
            g_ascii_strcasecmp( vol->encoding, "utf8" ) &&
            g_ascii_strcasecmp( vol->encoding, "utf-8" ) )
        {
            g_string_append_printf( cmd, "-o modules=iconv,from_code=%s,to_code=utf8",  vol->encoding );
        }

        if( ! g_file_test( vfs_remote_volume_get_mount_point( vol ), G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( vfs_remote_volume_get_mount_point( vol ), 0700 );
        g_string_append_printf( cmd, "'%s'", vfs_remote_volume_get_mount_point( vol ) );
    }
    else if( strcmp( vol->type , "smb" ) == 0 )
    {
        if( ! g_file_test( vfs_remote_volume_get_mount_point( vol ), G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( vfs_remote_volume_get_mount_point( vol ), 0700 );
        g_string_assign( cmd, "smbnetfs " );
        if( vol->encoding &&
            g_ascii_strcasecmp( vol->encoding, "utf8" ) &&
            g_ascii_strcasecmp( vol->encoding, "utf-8" ) )
        {
            g_string_append_printf( cmd, "-o modules=iconv,from_code=%s,to_code=utf8 ",  vol->encoding );
        }

        if( ! g_file_test( vfs_remote_volume_get_mount_point( vol ), G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( vfs_remote_volume_get_mount_point( vol ), 0700 );
        g_string_append_printf( cmd, "'%s'", vfs_remote_volume_get_mount_point( vol ) );
    }
    else
    {
        g_string_free( cmd, TRUE );
        g_set_error( err, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Unknown network protocol") );
        return FALSE;
    }

    g_debug( "mount command: %s", cmd->str );
    success = g_shell_parse_argv( cmd->str, NULL, &argv, NULL );
    g_string_free( cmd, TRUE );

    if( success )
    {
        char* program;
        int child_stderr;
        success = FALSE;
        program = g_find_program_in_path( argv[0] );

        if( program )
        {
            g_free( argv[0] );
            argv[0] = program;

            if( g_spawn_async_with_pipes( NULL, argv, envp,
                     G_SPAWN_DO_NOT_REAP_CHILD,
                     NULL, NULL, &pid, NULL, NULL, &child_stderr, &err ) )
            {
                g_child_watch_add( pid, on_mount_completed, &status );
                gtk_main(); /* run main loop to prevent blocking of GUI */
                if( 0 == status )
                    success = TRUE;
                else /* error */
                {
                    char err_msg[512];
                    int msg_len;
                    if( (msg_len = read( child_stderr, err_msg, 512 )) > 0 )
                    {
                        if( err_msg[msg_len - 1] == '\n' )
                            --msg_len;
                        err_msg[msg_len] = '\0';
                        g_set_error( err, G_FILE_ERROR, G_FILE_ERROR_FAILED, err_msg );
                    }
                    else
                        g_set_error( err, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Unknown error occured") );
                }
                close( child_stderr );
            }
        }
        else
        {
            g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_FAILED, PROG_NOT_FOUND_MSG, argv[0] );
        }
        g_strfreev( argv );
    }
    else
    {
        g_set_error( err, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Unknown error occured") );
    }

    g_free( envp ); /* don't use g_strfreev since the strings are constant */

    if( success )
        g_signal_emit( mgr, signals[MOUNT_SIGNAL], 0, vol );

    return success;
}

gboolean vfs_remote_volume_unmount( VFSRemoteVolume* vol, GError** err )
{
    if( vfs_remote_volume_is_mounted(vol) )
    {
        gboolean ret;
        int status;
        GDir* dir;
        char* argv[4];
        argv[0] = g_find_program_in_path( "fusermount" );
        argv[1] = "-u";
        argv[2] = vol->mount_point;
        argv[3] = '\0';
        char* err_msg;

        if( ! argv[0] )
        {
            g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_FAILED, PROG_NOT_FOUND_MSG, "fusermount" );
            return FALSE;
        }

        ret = g_spawn_sync( NULL, argv, NULL, 0, NULL, NULL, NULL, &err_msg, &status, NULL );
        g_free( argv[0] );

        if( ! ret || status != 0 )
        {
            g_set_error( err, G_FILE_ERROR, G_FILE_ERROR_FAILED, err_msg );
            g_free( err_msg );
            return FALSE;
        }
        g_free( err_msg );

        g_signal_emit( mgr, signals[UNMOUNT_SIGNAL], 0, vol );
        /* delete the folder if there is no file inside */
        if( dir = g_dir_open( vol->mount_point, 0, NULL ) )
        {
            char* file = g_dir_read_name(dir);
            g_dir_close( dir );
            if( ! file )
                g_rmdir( vol->mount_point );
        }
    }
    return TRUE;
}

gboolean vfs_remote_volume_is_mounted( VFSRemoteVolume* vol )
{
    /* FIXME: I think this is Linux only */
    FILE* f;

    if( f = fopen( "/proc/mounts", "r" ) )
    {
        char linebuf[512], *line;
        GString* mount_point = g_string_sized_new( 512 );
        /* escape the string for comparison with fstab
         * FIXME: Currently only spaces was escaped.
         * Are there other characters we should escape? */
        char* pstr;
        gboolean found = FALSE;

        for( pstr = vfs_remote_volume_get_mount_point(vol); *pstr; ++pstr )
        {
            if( *pstr == ' ' )
                g_string_append( mount_point, "\\040" );
            else
                g_string_append_c( mount_point, *pstr );
        }
        while( fgets( linebuf, G_N_ELEMENTS(linebuf), f ) )
        {
            line = strchr(linebuf, ' ');
            if( ! line )
                continue;
            while( *line == ' ' || *line == '\t' )
                ++line;
            line == strtok( line, " \t\n" );
            if( ! line )
                continue;
            if( strcmp( line, mount_point->str ) == 0 )
            {
                found = TRUE;  /* the dir is found in /proc/mounts */
                break;
            }
        }
        fclose( f );
        g_string_free( mount_point, TRUE );
        return found;
    }
    return FALSE;
}

const char* vfs_remote_volume_get_mount_point( VFSRemoteVolume* vol )
{
    if( G_UNLIKELY( ! vol->mount_point) )
        vol->mount_point = g_build_filename( g_get_home_dir(), ".Network",
                                             vol->type, vol->name ? vol->name : vol->host, NULL );
    return vol->mount_point;
}
