/*
 * Configuration parameters shared between Wine server and clients
 *
 * Copyright 2002 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "wine/port.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char * const server_config_dir = "/.wine";        /* config dir relative to $HOME */
static const char * const server_root_prefix = "/tmp/.wine-";  /* prefix for server root dir */
static const char * const server_dir_prefix = "/server-";      /* prefix for server dir */

static char *config_dir;
static char *server_dir;

#ifdef __GNUC__
static void fatal_error( const char *err, ... )  __attribute__((noreturn,format(printf,1,2)));
static void fatal_perror( const char *err, ... )  __attribute__((noreturn,format(printf,1,2)));
#endif

/* die on a fatal error */
static void fatal_error( const char *err, ... )
{
    va_list args;

    va_start( args, err );
    fprintf( stderr, "wine: " );
    vfprintf( stderr, err, args );
    va_end( args );
    exit(1);
}

/* die on a fatal error */
static void fatal_perror( const char *err, ... )
{
    va_list args;

    va_start( args, err );
    fprintf( stderr, "wine: " );
    vfprintf( stderr, err, args );
    perror( " " );
    va_end( args );
    exit(1);
}

/* malloc wrapper */
static void *xmalloc( size_t size )
{
    void *res;

    if (!size) size = 1;
    if (!(res = malloc( size ))) fatal_error( "virtual memory exhausted\n");
    return res;
}

/* remove all trailing slashes from a path name */
inline static void remove_trailing_slashes( char *path )
{
    int len = strlen( path );
    while (len > 1 && path[len-1] == '/') path[--len] = 0;
}

/* initialize all the paths values */
static void init_paths(void)
{
    struct stat st;
    char uid_str[32], *p;

    const char *home = getenv( "HOME" );
    const char *user = NULL;
    const char *prefix = getenv( "WINEPREFIX" );
    struct passwd *pwd = getpwuid( getuid() );

    if (pwd)
    {
        user = pwd->pw_name;
        if (!home) home = pwd->pw_dir;
    }
    if (!user)
    {
        sprintf( uid_str, "%d", getuid() );
        user = uid_str;
    }

    /* build config_dir */

    if (prefix)
    {
        if (!(config_dir = strdup( prefix ))) fatal_error( "virtual memory exhausted\n");
        remove_trailing_slashes( config_dir );
        if (config_dir[0] != '/')
            fatal_error( "invalid directory %s in WINEPREFIX: not an absolute path\n", prefix );
        if (stat( config_dir, &st ) == -1)
            fatal_perror( "cannot open %s as specified in WINEPREFIX", config_dir );
    }
    else
    {
        if (!home) fatal_error( "could not determine your home directory\n" );
        if (home[0] != '/') fatal_error( "your home directory %s is not an absolute path\n", home );
        config_dir = xmalloc( strlen(home) + strlen(server_config_dir) + 1 );
        strcpy( config_dir, home );
        remove_trailing_slashes( config_dir );
        strcat( config_dir, server_config_dir );
        if (stat( config_dir, &st ) == -1)
            fatal_perror( "cannot open %s", config_dir );
    }
    if (!S_ISDIR(st.st_mode)) fatal_error( "%s is not a directory\n", config_dir );

    /* build server_dir */

    server_dir = xmalloc( strlen(server_root_prefix) + strlen(user) + strlen( server_dir_prefix ) +
                          2*sizeof(st.st_dev) + 2*sizeof(st.st_ino) + 2 );
    strcpy( server_dir, server_root_prefix );
    p = server_dir + strlen(server_dir);
    strcpy( p, user );
    while (*p)
    {
        if (*p == '/') *p = '!';
        p++;
    }
    strcpy( p, server_dir_prefix );

    if (sizeof(st.st_dev) > sizeof(unsigned long))
        sprintf( server_dir + strlen(server_dir), "%llx-", (unsigned long long)st.st_dev );
    else
        sprintf( server_dir + strlen(server_dir), "%lx-", (unsigned long)st.st_dev );

    if (sizeof(st.st_ino) > sizeof(unsigned long))
        sprintf( server_dir + strlen(server_dir), "%llx", (unsigned long long)st.st_ino );
    else
        sprintf( server_dir + strlen(server_dir), "%lx", (unsigned long)st.st_ino );
}

/* return the configuration directory ($WINEPREFIX or $HOME/.wine) */
const char *wine_get_config_dir(void)
{
    if (!config_dir) init_paths();
    return config_dir;
}

/* return the full name of the server directory (the one containing the socket) */
const char *wine_get_server_dir(void)
{
    if (!server_dir) init_paths();
    return server_dir;
}
