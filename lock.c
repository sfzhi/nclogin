/* lock.c */
/******************************************************************************/
/* Copyright 2026 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "lock.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h> // snprintf()
#include <fcntl.h> // open(), O_*
#include <unistd.h> // close()
#include <sys/ioctl.h> // ioctl()
#include <linux/vt.h> // VT_*
#include <signal.h> // sig*(), SIG*
/*============================================================================*/
static int old_vt = 0, new_vt = 0, vt_fd = -1;
static sig_atomic_t enable_switch = 0;
/*============================================================================*/
static void signal_handler(int signo)
{
  int param;
  switch (signo) {
  case SIGUSR1:
    param = VT_ACKACQ;
    break;
  case SIGUSR2:
    param = enable_switch;
    break;
  default:
    return;
  }
  ioctl(vt_fd, VT_RELDISP, param);
}
/*----------------------------------------------------------------------------*/
const char *nclogin_lock_path(void)
{
  static char sbuf[sizeof("/dev/tty") + INT_STR_BUF(unsigned int) - 1];
  if (nclogin_config.screenlock &&
      (nclogin_config.ctty != NULL) && (*nclogin_config.ctty == '\0'))
  {
    const char *path = NULL;
    int tty0 = open("/dev/tty0",
        O_RDONLY|O_NOCTTY|O_NOATIME|O_NONBLOCK|O_CLOEXEC);
    if (tty0 >= 0)
    {
      if ((ioctl(tty0, VT_OPENQRY, &new_vt) >= 0) && (new_vt > 0))
      {
        infomsg("Will use virtual console %d for locking\n", new_vt);
        int len = snprintf(sbuf, sizeof(sbuf), "/dev/tty%d", new_vt);
        if ((len > 0) && ((unsigned int)len < sizeof(sbuf)))
          path = sbuf;
      }
      close(tty0);
    }
    else
      failure("Failed to open current virtual console: %m\n");
    return path;
  }
  return "";
}
/*============================================================================*/
bool nclogin_lock_init(int fd)
{
  if (nclogin_config.screenlock)
  {
    struct vt_stat vts;
    if (ioctl(fd, VT_GETSTATE, &vts) >= 0)
    {
      vt_fd = fd;
      old_vt = vts.v_active;
      struct sigaction sig_act;
      memset(&sig_act, 0, sizeof(sig_act));
      sig_act.sa_handler = &signal_handler;
      sig_act.sa_flags = SA_RESTART;
      sigemptyset(&sig_act.sa_mask);
      if ((sigaction(SIGUSR1, &sig_act, NULL) >= 0) &&
          (sigaction(SIGUSR2, &sig_act, NULL) >= 0))
      {
        sigaddset(&sig_act.sa_mask, SIGUSR1);
        sigaddset(&sig_act.sa_mask, SIGUSR2);
        if (sigprocmask(SIG_UNBLOCK, &sig_act.sa_mask, NULL) < 0)
          failure("Failed to unblock console switching signals: %m\n");
        struct vt_mode vtm = {
          .mode = VT_PROCESS,
          .acqsig = SIGUSR1,
          .relsig = SIGUSR2,
        };
        if (ioctl(fd, VT_SETMODE, &vtm) >= 0)
        {
          if ((new_vt <= 0) ||
              (ioctl(fd, VT_ACTIVATE, new_vt) >= 0))
            return true;
          else
            failure("Failed to activate locked virtual console: %m\n");
        }
        else
          failure("Failed to set virtual console switch mode: %m\n");
      }
      else
        failure("Failed to install switch signal handler: %m\n");
    }
    else
      failure("Failed to query virtual console state: %m\n");
    return false;
  }
  return true;
}
/*============================================================================*/
void nclogin_lock_done(void)
{
  if (nclogin_config.screenlock)
  {
    enable_switch = 1;
    if (vt_fd >= 0)
    {
      struct vt_mode vtm = {.mode = VT_AUTO};
      if (ioctl(vt_fd, VT_SETMODE, &vtm) < 0)
        failure("Failed to restore virtual console mode: %m\n");
      if ((old_vt > 0) && (new_vt > 0) && (old_vt != new_vt) &&
          (ioctl(vt_fd, VT_ACTIVATE, old_vt) < 0))
        failure("Failed to switch back to saved console: %m\n");
      if (nclogin_config.changectty && (ioctl(vt_fd, TIOCNOTTY) < 0))
        failure("Failed to release controlling terminal: %m\n");
      vt_fd = -1;
    }
  }
}
/*============================================================================*/
