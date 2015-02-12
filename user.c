/* user.c */
/******************************************************************************/
#include "main.h"
#include "ctty.h"
#include "utmp.h"
#include "user.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h> // initgroups()
#include <sys/wait.h>
/*============================================================================*/
static char *makeenv(char *envbuf, size_t size, char **envp, ...)
{
  va_list args;
  va_start(args, envp);
  char *pos = envbuf;
  bool name = true;
  for (;;)
  {
    char *arg = va_arg(args, char *);
    if (arg != NULL)
    {
      if (pos < envbuf + size)
      {
        char *last = pos;
        pos = memccpy(pos, arg, '\0', size - (pos - envbuf));
        if (pos == NULL)
          goto stop;
        else if (name)
        {
          *(envp++) = last;
          pos[-1] = '=';
        }
      }
      else
      {
        pos = NULL;
stop:   if (!name)
          envp--;
        break;
      }
    }
    else if (name)
      break;
    else
      pos = *(--envp);
    name = !name;
  }
  va_end(args);
  *envp = NULL;
  return pos;
}
/*----------------------------------------------------------------------------*/
static void execute(login_info_t *info)
{
  char *uenv[8];
  char envbuf[1024];
  const char *userpath =
    (info->uid == 0)? "/usr/bin:/bin:/usr/sbin:/sbin": "/usr/bin:/bin";
  char *tail = makeenv(envbuf, sizeof(envbuf), uenv,
    "LOGNAME", info->name,
    "SHELL", info->exec,
    "USER", info->name,
    "HOME", info->home,
    "PATH", userpath,
    "TERM", nclogin_config.term,
    "LANG", nclogin_config.lang,
    NULL);
  if (tail == NULL)
    failure("Insufficient buffer space for user environment\n");

  const char *shbin = (info->exec != NULL)? info->exec: "/bin/sh";
  if (*shbin == '/')
  {
    char *args[] = {(char *)shbin, NULL};
    execve(shbin, args, uenv);
  }
  else
    errno = EINVAL;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
bool nclogin_user_exec(login_info_t *info)
{
  if (nclogin_config.subprocess)
  {
    if (getsid(0) == getpid())
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        if (nclogin_config.newsession)
        {
          if (setsid() < 0)
            failure("Failed to create new session: %m\n");
          else if (!nclogin_ctty_grab())
            failure("Failed to acquire controlling tty: %m\n");
        }
        else
        {
          if (setpgid(0, 0) < 0)
            failure("Failed to create new process group: %m\n");
          else if (!nclogin_ctty_pgrp())
            failure("Failed to activate user process group: %m\n");
        }
      }
      else
      {
        if (pid > 0)
        {
          int status = 0;
          waitpid(pid, &status, 0);
          if (nclogin_config.newsession)
          {
            if (!nclogin_ctty_grab())
              failure("Failed to re-acquire controlling tty: %m\n");
          }
          else
          {
            if (!nclogin_ctty_pgrp())
              failure("Failed to activate main process group: %m\n");
          }
          nclogin_utmp_user(NULL, pid);
        }
        else
          failure("Failed to fork a child process: %m\n");
        return true;
      }
    }
    else
      failure("Must be session leader to wait for termination\n");
  }

  nclogin_utmp_user(info->name, 0);

  if (initgroups(info->name, info->gid) < 0)
    failure("Failed to assign supplementary groups: %m\n");
  if (setgid(info->gid) < 0)
    failure("Failed to assign primary user group: %m\n");
  if (setuid(info->uid) < 0)
    failure("Failed to switch user to '%s': %m\n", info->name);
  if ((info->home != NULL) && (chdir(info->home) < 0))
    failure("Failed to change to home directory: %m\n");

  execute(info);
  _exit(1);
}
/*============================================================================*/
