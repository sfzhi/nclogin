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
#include <sys/prctl.h> // prctl()
#include <signal.h> // raise(), SIG*
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
  char *uenv[9];
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
    "CTTY", nclogin_config.exportctty? nclogin_config.ctty: NULL,
    NULL);
  if (tail == NULL)
    failure("Insufficient buffer space for user environment\n");

  const char *shbin = (nclogin_config.exec != NULL)?
    nclogin_config.exec: (info->exec != NULL)? info->exec: "/bin/sh";
  if (*shbin == '/')
  {
    const char *base = shbin;
    if (nclogin_config.loginshell)
      base = strrchr(base, '/');
    size_t len = strlen(base) + 1;
    char argv0[len];
    memcpy(argv0, base, len);
    if (nclogin_config.loginshell)
      argv0[0] = '-';
    int nargs = 1;
    if (nclogin_config.extraargc > 0)
      nargs += nclogin_config.extraargc;
    char *args[nargs + 1];
    args[0] = argv0;
    for (int i = 0; i < nclogin_config.extraargc; i++)
      args[i + 1] = nclogin_config.extraargv[i];
    args[nargs] = NULL;
    execve(shbin, args, uenv);
  }
  else
    errno = EINVAL;
  failure("Failed to execute user shell or wrapper '%s': %m\n", shbin);
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
        if (nclogin_config.killorphan)
        {
          if (prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0) < 0)
            failure("Failed to setup parent death signal: %m\n");
          if ((getppid() == 1) && (raise(SIGTERM) != 0))
            failure("Failed to send SIGTERM to itself: %m\n");
        }
      }
      else
      {
        if (pid > 0)
        {
          pid_t wpid;
          int status = 0;
          do {
            wpid = waitpid(pid, &status, 0);
          } while ((wpid < 0) && (errno == EINTR));

          if (wpid < 0)
            failure("Failed to wait for child process: %m\n");
          else if (wpid != pid)
            failure("Waiting returned unexpected PID: %d\n", (int)wpid);
          else if (WIFEXITED(status))
            infomsg("Child process terminated with exit code %d\n",
              WEXITSTATUS(status));
          else if (WIFSIGNALED(status))
            warning("Child process killed by signal #%d (%s)\n",
              WTERMSIG(status), strsignal(WTERMSIG(status)));
          else
            warning("Child process terminated, status=%d\n", status);

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
          nclogin_ctty_back();
          nclogin_utmp_user(NULL, 0, pid);
        }
        else
          failure("Failed to fork a child process: %m\n");
        return true;
      }
    }
    else
      failure("Must be session leader to wait for termination\n");
  }

  nclogin_utmp_user(info->name, info->uid, 0);
  nclogin_ctty_user(info->uid, info->gid, 0);

  if (!nclogin_config.skipsetuid)
  {
    if (initgroups(info->name, info->gid) < 0)
      failure("Failed to assign supplementary groups: %m\n");
    if (setgid(info->gid) < 0)
      failure("Failed to assign primary user group: %m\n");
    if (setuid(info->uid) < 0)
      failure("Failed to switch user to '%s': %m\n", info->name);
    if ((info->home != NULL) && (chdir(info->home) < 0))
      failure("Failed to change to home directory: %m\n");
  }

  nclogin_main_undo();
  execute(info);
  _exit(1);
}
/*============================================================================*/
