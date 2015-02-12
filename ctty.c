/* ctty.c */
/******************************************************************************/
#include "main.h"
#include "ctty.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <fcntl.h> // open(), O_*
#include <sys/ioctl.h> // ioctl()
#include <unistd.h> // tcsetpgrp()
#include <errno.h>
/*============================================================================*/
static int cttyfd = -1;
/*============================================================================*/
void nclogin_ctty_init(void)
{
  if (cttyfd < 0)
    cttyfd = open("/dev/tty", O_RDONLY|O_NOATIME|O_CLOEXEC);
}
/*============================================================================*/
bool nclogin_ctty_grab(void)
{
  if (cttyfd >= 0)
    return ioctl(cttyfd, TIOCSCTTY, 1) >= 0;
  errno = ENOENT;
  return false;
}
/*============================================================================*/
bool nclogin_ctty_pgrp(void)
{
  if (cttyfd >= 0)
    return tcsetpgrp(cttyfd, getpid()) >= 0;
  errno = ENOENT;
  return false;
}
/*============================================================================*/
