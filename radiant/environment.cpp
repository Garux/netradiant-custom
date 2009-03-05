/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "environment.h"

#include "stream/textstream.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "debugging/debugging.h"
#include "os/path.h"
#include "os/file.h"
#include "cmdlib.h"

int g_argc;
char** g_argv;

void args_init(int argc, char* argv[])
{
  int i, j, k;

  for (i = 1; i < argc; i++)
  {
    for (k = i; k < argc; k++)
      if (argv[k] != 0)
        break;

    if (k > i)
    {
      k -= i;
      for (j = i + k; j < argc; j++)
        argv[j-k] = argv[j];
      argc -= k;
    }
  }

  g_argc = argc;
  g_argv = argv;
}

char *gamedetect_argv_buffer[1024];
void gamedetect_found_game(char *game, char *path)
{
  int argc;
  static char buf[128];

  if(g_argv == gamedetect_argv_buffer)
    return;

  globalOutputStream() << "Detected game " << game << " in " << path << "\n";

  sprintf(buf, "-%s-EnginePath", game);
  argc = 0;
  gamedetect_argv_buffer[argc++] = "-global-gamefile";
  gamedetect_argv_buffer[argc++] = game;
  gamedetect_argv_buffer[argc++] = buf;
  gamedetect_argv_buffer[argc++] = path;
  if((size_t) (argc + g_argc) >= sizeof(gamedetect_argv_buffer) / sizeof(*gamedetect_argv_buffer) - 1)
    g_argc = sizeof(gamedetect_argv_buffer) / sizeof(*gamedetect_argv_buffer) - g_argc - 1;
  memcpy(gamedetect_argv_buffer + 4, g_argv, sizeof(*gamedetect_argv_buffer) * g_argc);
  g_argc += argc;
  g_argv = gamedetect_argv_buffer;
}

void gamedetect()
{
  // if we're inside a Nexuiz install
  // default to nexuiz.game (unless the user used an option to inhibit this)
  bool nogamedetect = false;
  int i;
  for(i = 1; i < g_argc - 1; ++i)
    if(g_argv[i][0] == '-')
	{
      if(!strcmp(g_argv[i], "-gamedetect"))
	    nogamedetect = !strcmp(g_argv[i+1], "false");
	  ++i;
	}
  if(!nogamedetect)
  {
	static char buf[1024 + 64];
	strncpy(buf, environment_get_app_path(), sizeof(buf));
	buf[sizeof(buf) - 1 - 64] = 0;
	if(!strlen(buf))
	  return;

	char *p = buf + strlen(buf) - 1; // point directly on the slash of get_app_path
	while(p != buf)
	{
	  // TODO add more games to this
	  // try to detect Nexuiz installs
	  strcpy(p, "/data/common-spog.pk3");
	  globalOutputStream() << "Checking for a game file in " << buf << "\n";
	  if(file_exists(buf))
	  {
#if defined(WIN32)
	    strcpy(p, "/nexuiz.exe");
#elif defined(__APPLE__)
	    strcpy(p, "/Nexuiz.app/Contents/Info.plist");
#else
	    strcpy(p, "/nexuiz-linux-glx.sh");
#endif
		if(file_exists(buf))
		{
		  p[1] = 0;
		  gamedetect_found_game("nexuiz.game", buf);
		  return;
		}
      }

	  // we found nothing
	  // go backwards
	  --p;
	  while(p != buf && *p != '/' && *p != '\\')
	    --p;
	}
  }
}

namespace
{
  CopiedString home_path;
  CopiedString app_path;
}

const char* environment_get_home_path()
{
  return home_path.c_str();
}

const char* environment_get_app_path()
{
  return app_path.c_str();
}


#if defined(POSIX)

#include <stdlib.h>
#include <pwd.h>
#include <unistd.h> 

#include <glib/gutils.h>

const char* LINK_NAME =
#if defined (__linux__)
  "/proc/self/exe"
#else // FreeBSD and OSX
  "/proc/curproc/file"
#endif
;

/// brief Returns the filename of the executable belonging to the current process, or 0 if not found.
char* getexename(char *buf)
{
  /* Now read the symbolic link */
  int ret = readlink(LINK_NAME, buf, PATH_MAX);

  if(ret == -1)
  {
    globalOutputStream() << "getexename: falling back to argv[0]: " << makeQuoted(g_argv[0]);
    const char* path = realpath(g_argv[0], buf);
    if(path == 0)
    {
      /* In case of an error, leave the handling up to the caller */
      return "";
    }
  }

  /* Ensure proper NUL termination */
  buf[ret] = 0;

  /* delete the program name */
  *(strrchr(buf, '/')) = '\0';

  // NOTE: we build app path with a trailing '/'
  // it's a general convention in Radiant to have the slash at the end of directories
  if (buf[strlen(buf)-1] != '/')
  {
    strcat(buf, "/");
  }

  return buf;
}

void environment_init(int argc, char* argv[])
{
  // Give away unnecessary root privileges.
  // Important: must be done before calling gtk_init().
  char *loginname;
  struct passwd *pw;
  seteuid(getuid());
  if (geteuid() == 0 && (loginname = getlogin()) != 0 &&
      (pw = getpwnam(loginname)) != 0)
    setuid(pw->pw_uid);

  args_init(argc, argv);

  {
    StringOutputStream home(256);
    home << DirectoryCleaned(g_get_home_dir()) << ".netradiant/";
    Q_mkdir(home.c_str());
    home_path = home.c_str();
  }
  {
    char real[PATH_MAX];
    app_path = getexename(real);
    ASSERT_MESSAGE(!string_empty(app_path.c_str()), "failed to deduce app path");
  }
  gamedetect();
}

#elif defined(WIN32)

#include <windows.h>

void environment_init(int argc, char* argv[])
{
  args_init(argc, argv);

  {
    char *appdata = getenv("APPDATA");

    StringOutputStream home(256);
    if(!appdata || string_empty(appdata))
    {
      ERROR_MESSAGE("Application Data folder not available.\n"
        "Radiant will use C:\\ for user preferences.\n");
      home << "C:";
    }
    else
    {
      home << PathCleaned(appdata);
    }
    home << "/NetRadiantSettings/";
    Q_mkdir(home.c_str());
    home_path = home.c_str();
  }
  {
    // get path to the editor
    char filename[MAX_PATH+1];
    GetModuleFileName(0, filename, MAX_PATH);
    char* last_separator = strrchr(filename, '\\');
    if(last_separator != 0)
    {
      *(last_separator+1) = '\0';
    }
    else
    {
      filename[0] = '\0';
    }
    StringOutputStream app(256);
    app << PathCleaned(filename);
    app_path = app.c_str();
  }
  gamedetect();
}

#else
#error "unsupported platform"
#endif
