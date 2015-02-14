/* utmp.c */
/******************************************************************************/
#include "main.h"
#include "utmp.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <utmp.h>
#include <unistd.h> // getpid(), getppid()
#include <sys/time.h> // gettimeofday()
/*============================================================================*/
#define UTMP_LOGIN "nclogin"
/*============================================================================*/
static char utid[sizeof(((struct utmp *)0)->ut_id) + 1];
/*============================================================================*/
static void update_utmp_and_wtmp(bool live, const char *user, pid_t pid)
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
  if (gettimeofday(&tv, NULL) >= 0)
  {
    ut.ut_tv.tv_sec = tv.tv_sec;
    ut.ut_tv.tv_usec = tv.tv_usec;
  }
  setutent();
  if (pututline(&ut) == NULL)
    failure("Failed to update utmp record: %m\n");
  endutent();
#ifdef _PATH_WTMP
  if (user != NULL)
  {
    if (!live && (ut.ut_id[0] != '\0'))
      memset(ut.ut_id, '\0', sizeof(ut.ut_id));
    updwtmp(_PATH_WTMP, &ut);
  }
#endif
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
        pid_t pid = getpid();
        int32_t sec = 0, usec = 0;
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
      update_utmp_and_wtmp(true, NULL, 0); // calls endutent()
    }
    else
      endutent();
  }
}
/*============================================================================*/
void nclogin_utmp_user(const char *name, pid_t pid)
{
  if (nclogin_config.updateutmp && (nclogin_config.ctty != NULL))
    update_utmp_and_wtmp(name != NULL, (name != NULL)? name: "", pid);
}
/*============================================================================*/
void nclogin_utmp_self(bool live)
{
  if (nclogin_config.updateutmp && (nclogin_config.ctty != NULL))
    update_utmp_and_wtmp(live, NULL, 0);
}
/*============================================================================*/
