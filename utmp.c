/* utmp.c */
/******************************************************************************/
#include "main.h"
#include "utmp.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <utmp.h>
#include <unistd.h> // getpid()
#include <sys/time.h> // gettimeofday()
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
    strncpy(ut.ut_user, (user != NULL)? user: "nclogin", sizeof(ut.ut_user));
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
    failure("Failed to update utmp entry: %m\n");
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
void nclogin_utmp_init(void)
{
  if (nclogin_config.updateutmp)
  {
    //
    update_utmp_and_wtmp(true, NULL, 0);
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
