/* utmp.c */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "utmp.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <utmp.h>
#include <fcntl.h> // open(), O_*
#include <unistd.h> // getpid(), getppid(), pwrite(), close()
#include <lastlog.h> // struct lastlog
#include <sys/time.h> // gettimeofday()
#include <errno.h>
/*============================================================================*/
#define UTMP_LOGIN "nclogin"
/*============================================================================*/
static char utid[sizeof(((struct utmp *)0)->ut_id) + 1];
/*============================================================================*/
static inline void update_lastlog(uid_t uid, time_t when)
{
#ifdef _PATH_LASTLOG
  int fd = open(_PATH_LASTLOG, O_WRONLY);
  if (fd >= 0)
  {
    struct lastlog ll = {.ll_host = "localhost", .ll_time = when};
    if (nclogin_config.ctty != NULL)
      strncpy(ll.ll_line, nclogin_config.ctty, sizeof(ll.ll_line));
    ssize_t res = pwrite(fd, &ll, sizeof(ll), sizeof(ll) * uid);
    if (res < 0)
      failure("Failed to update lastlog record: %m\n");
    else if (res != sizeof(ll))
      warning("Partially written lastlog record: %zd", res);
    close(fd);
  }
  else if (errno != ENOENT)
    failure("Failed to open lastlog file (%s): %m\n", _PATH_LASTLOG);
#endif
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void update_records(bool live, const char *user, uid_t uid, pid_t pid)
{
  struct utmp ut = {
    .ut_type = live? (user != NULL)? USER_PROCESS: LOGIN_PROCESS: DEAD_PROCESS,
    .ut_pid = (pid != 0)? pid: getpid(),
  };
  if (nclogin_config.utid != NULL)
    strncpy(ut.ut_id, nclogin_config.utid, sizeof(ut.ut_id));
  if (nclogin_config.ctty != NULL)
    strncpy(ut.ut_line, nclogin_config.ctty, sizeof(ut.ut_line));
  if (live)
  {
    strncpy(ut.ut_user, (user != NULL)? user: UTMP_LOGIN, sizeof(ut.ut_user));
    pid_t sid = getsid(0);
    if (sid > 0)
      ut.ut_session = sid;
  }
  struct timeval tv;
  bool have_time = false;
  if (gettimeofday(&tv, NULL) >= 0)
  {
    ut.ut_tv.tv_sec = tv.tv_sec;
    ut.ut_tv.tv_usec = tv.tv_usec;
    have_time = true;
  }
  setutent();
  if (pututline(&ut) == NULL)
    failure("Failed to update utmp record: %m\n");
  endutent();
  if (user != NULL)
  {
#ifdef _PATH_WTMP
    if (!live && (ut.ut_id[0] != '\0'))
      memset(ut.ut_id, '\0', sizeof(ut.ut_id));
    updwtmp(_PATH_WTMP, &ut);
#endif
    if (live && have_time)
      update_lastlog(uid, ut.ut_tv.tv_sec);
  }
}
/*============================================================================*/
static inline void save_utid(struct utmp *ut)
{
  if (memccpy(utid, ut->ut_id, '\0', sizeof(ut->ut_id)) == NULL)
    utid[sizeof(ut->ut_id)] = '\0';
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void nclogin_utmp_init(void)
{
  if (nclogin_config.updateutmp)
  {
    setutent();
    struct utmp *ut;
    size_t len = (nclogin_config.ctty != NULL)? strlen(nclogin_config.ctty): 0;
    if (nclogin_config.utid == NULL)
    {
      pid_t ppid = getppid();
      bool check = (len > 0) && (len <= sizeof(ut->ut_line));
      if (check || (ppid == 1))
      {
        time_t sec = 0;
        int32_t usec = 0;
        pid_t pid = getpid();
        while ((ut = getutent()) != NULL)
        {
          switch (ut->ut_type)
          {
          case INIT_PROCESS:
            if ((ppid == 1) && (ut->ut_pid == pid))
            {
              save_utid(ut);
              goto found;
            }
            break;
          case LOGIN_PROCESS:
          case USER_PROCESS:
            if (check && (strncmp(nclogin_config.ctty, ut->ut_line,
                sizeof(ut->ut_line)) == 0) && ((ut->ut_tv.tv_sec > sec) ||
                ((ut->ut_tv.tv_sec == sec) && (ut->ut_tv.tv_usec > usec))))
            {
              sec = ut->ut_tv.tv_sec;
              usec = ut->ut_tv.tv_usec;
              save_utid(ut);
            }
            break;
          default:;
          }
        }
        if (utid[0] != '\0')
found:    nclogin_config.utid = utid;
      }
    }
    if (len > 0)
    {
      if (nclogin_config.utid == NULL)
      {
        if (len <= sizeof(ut->ut_id))
          nclogin_config.utid = nclogin_config.ctty;
        else if ((len <= sizeof(ut->ut_id) + strlen("pts")) &&
            (memcmp(nclogin_config.ctty, "pts/", strlen("pts/")) == 0))
          nclogin_config.utid = nclogin_config.ctty + strlen("pts");
        else
          nclogin_config.utid = nclogin_config.ctty + len - sizeof(ut->ut_id);
      }
      update_records(true, NULL, 0, 0); // calls endutent()
    }
    else
      endutent();
  }
}
/*============================================================================*/
void nclogin_utmp_user(const char *name, uid_t uid, pid_t pid)
{
  if (nclogin_config.updateutmp && (nclogin_config.ctty != NULL))
    update_records(name != NULL, (name != NULL)? name: "", uid, pid);
}
/*============================================================================*/
void nclogin_utmp_self(bool live)
{
  if (nclogin_config.updateutmp && (nclogin_config.ctty != NULL))
    update_records(live, NULL, 0, 0);
}
/*============================================================================*/
