/* ctty.c */
/******************************************************************************/
/* Copyright 2015-2025 Sergei Zhirikov <sfzhi@yahoo.com>                      */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "ctty.h"
#include "util.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h> // snprintf()
#include <fcntl.h> // open(), O_*, fcntl(), F_?ETFD, FD_CLOEXEC
#include <sys/stat.h> // stat(), S_ISCHR
#include <sys/ioctl.h> // ioctl()
#include <sys/sysmacros.h> // major(), minor()
#include <termios.h> // tcgetsid()
#include <stdlib.h> // strtoll()
#include <unistd.h>
#include <errno.h>
/*----------------------------------------------------------------------------*/
#define DEV_PTS_MIN 136
#define DEV_PTS_MAX 143
/*----------------------------------------------------------------------------*/
#define INT_STR_BUF(t) (((sizeof(t) * 5) + 3) / 2)
/*============================================================================*/
static int cttyfd = -1;
static dev_t cttydev = 0;
static char *cttypath = NULL;
static char *cttyname = NULL;
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define CTTY_NAME \
  (((cttyname != NULL) && (*cttyname != '\0'))? cttyname: cttypath)
/*============================================================================*/
static dev_t get_ctty_dev(void)
{
  int fd = open("/proc/self/stat", O_RDONLY|O_NOCTTY|O_NOATIME|O_CLOEXEC);
  if (fd >= 0)
  {
    char sbuf[1024];
    ssize_t rsize = read(fd, sbuf, sizeof(sbuf));
    close(fd);
    if (rsize > 0)
    {
      char *cpos = sbuf + rsize;
      if ((size_t)rsize == sizeof(sbuf))
        while ((cpos > sbuf) && (*(--cpos) > ' '));
      *cpos = '\0';
      unsigned int countdown = 7;
      for (cpos = strtok(sbuf, " "); cpos != NULL; cpos = strtok(NULL, " "))
        if (--countdown == 0) break;
      if (cpos != NULL)
      {
        errno = 0;
        char *upto = NULL;
        long long value = strtoll(cpos, &upto, 10);
        if ((upto > cpos) && (*upto <= ' ') && (errno == 0))
          return (dev_t)value;
      }
    }
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static bool set_ctty_path(char *path, bool standard)
{
  if ((cttypath = strdup(path)) != NULL)
  {
    if (standard)
      cttyname = cttypath + strlen("/dev/");
    else if (!nclogin_util_canonpath(cttypath))
      strcpy(cttypath, path); // revert partial changes
    else if (memcmp(cttypath, "/dev/", strlen("/dev/")) == 0)
      cttyname = cttypath + strlen("/dev/");
  }
  return cttypath != NULL;
}
/*----------------------------------------------------------------------------*/
static bool try_dev_path(const char *path)
{
  struct stat st;
  return (stat(path, &st) >= 0) &&
    S_ISCHR(st.st_mode) && ((cttydev == 0) || (st.st_rdev == cttydev));
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static char *try_open_fd(int fd)
{
  if (tcgetsid(fd) == getsid(0))
  {
    char *path;
    struct stat st;
    if ((fstat(fd, &st) >= 0) && S_ISCHR(st.st_mode) &&
        (st.st_rdev == cttydev) && ((path = ttyname(fd)) != NULL) &&
        (strcmp(path, "/dev/tty") != 0) && try_dev_path(path))
    {
      cttyfd = fd;
      return path;
    }
    if (cttyfd < 0)
      cttyfd = fd;
  }
  return NULL;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_cmd_arg(void)
{
  if ((nclogin_config.ctty != NULL) && (*nclogin_config.ctty != '\0') &&
      (strcmp(nclogin_config.ctty, "tty") != 0))
  {
    size_t len = strlen(nclogin_config.ctty) + 1;
    char sbuf[strlen("/dev/") + len];
    memcpy(sbuf + strlen("/dev/"), nclogin_config.ctty, len);
    memcpy(sbuf, "/dev/", strlen("/dev/"));
    return try_dev_path(sbuf) && set_ctty_path(sbuf, false);
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_pts_path(void)
{
  if ((unsigned int)major(cttydev) == DEV_PTS_MIN)
  {
    char sbuf[strlen("/dev/pts/") + INT_STR_BUF(unsigned int)];
    int len = snprintf(sbuf, sizeof(sbuf),
      "/dev/pts/%u", (unsigned int)minor(cttydev));
    return (len > 0) && ((unsigned int)len < sizeof(sbuf)) &&
      try_dev_path(sbuf) && set_ctty_path(sbuf, true);
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_stdio_fds(void)
{
  char *path = try_open_fd(STDIN_FILENO);
  if (path == NULL)
    path = try_open_fd(STDOUT_FILENO);
  if (path == NULL)
    path = try_open_fd(STDERR_FILENO);
  return (path != NULL) && set_ctty_path(path, false);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_dev_links(void)
{
  char sbuf[strlen("/sys/dev/char/") + 2 * INT_STR_BUF(unsigned int)];
  int len = snprintf(sbuf, sizeof(sbuf), "/sys/dev/char/%u:%u",
    (unsigned int)major(cttydev), (unsigned int)minor(cttydev));
  if ((len > 0) && ((unsigned int)len < sizeof(sbuf)))
  {
    char lbuf[256];
    char *devpath = NULL;
    bool standard_path = false;
    const size_t offset = strlen("/dev/");
    ssize_t rsize = readlink(sbuf + strlen("/sys"), lbuf + offset,
      sizeof(lbuf) - offset);
    if ((rsize > 0) && ((size_t)rsize < sizeof(lbuf) - offset))
    {
      lbuf[offset + rsize] = '\0';
      if (lbuf[offset] == '/')
        devpath = lbuf + offset;
      else if (memcmp(lbuf + offset, "../",  strlen("../")) == 0)
        devpath = memcpy(lbuf + strlen("../"), "/dev", strlen("/dev"));
      else
        devpath = memcpy(lbuf, "/dev/", strlen("/dev/"));
    }
    else
    {
      rsize = readlink(sbuf, lbuf + offset, sizeof(lbuf) - offset);
      if ((rsize > 0) && ((size_t)rsize < sizeof(lbuf) - offset))
      {
        lbuf[offset + rsize] = '\0';
        char *name = memrchr(lbuf + offset, '/', rsize);
        if (name != NULL)
          memmove(lbuf + offset, name + 1, lbuf + offset + rsize - name);
        devpath = memcpy(lbuf, "/dev/", strlen("/dev/"));
        standard_path = true;
      }
    }
    return (devpath != NULL) &&
      try_dev_path(devpath) && set_ctty_path(devpath, standard_path);
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_find_ctty(void)
{
  return ((cttydev = get_ctty_dev()) != 0) && \
    (try_cmd_arg() || try_pts_path() || try_stdio_fds() || try_dev_links());
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool open_new_ctty(void)
{
  errno = 0;
  if (try_cmd_arg())
  {
    int fd = open(cttypath, O_RDWR|O_NOATIME|O_CLOEXEC);
    if (fd >= 0)
    {
      int flags;
      if (((fd >= 3) || (((flags = fcntl(fd, F_GETFD)) != -1) &&
            (fcntl(fd, F_SETFD, flags&~FD_CLOEXEC) >= 0))) &&
          ((fd == STDIN_FILENO) || (dup2(fd, STDIN_FILENO) >= 0)) &&
          ((fd == STDOUT_FILENO) || (dup2(fd, STDOUT_FILENO) >= 0)) &&
          ((fd == STDERR_FILENO) || (dup2(fd, STDERR_FILENO) >= 0)) &&
          ((getsid(0) == getpid()) || (setsid() > 0)) &&
          (ioctl(fd, TIOCSCTTY, 1) >= 0))
      {
        cttyfd = fd;
        infomsg("Changed controlling TTY to '%s'\n", CTTY_NAME);
        return true;
      }
      else
      {
        failure("Failed to switch controlling TTY: %m'\n");
        close(fd);
        return false;
      }
    }
  }
  else if (errno != ENOMEM)
  {
    errno = EINVAL;
  }
  failure("Failed to open controlling TTY: %m'\n");
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
bool nclogin_ctty_init(void)
{
  if (nclogin_config.changectty)
    return open_new_ctty();
  if (try_find_ctty())
    infomsg("Identified controlling TTY device: %s\n", CTTY_NAME);
  else
    warning("Unable to identify controlling TTY device\n");
  if (cttyfd < 0)
    cttyfd = open("/dev/tty", O_RDONLY|O_NOATIME|O_CLOEXEC);
  if (nclogin_config.ctty == NULL)
    nclogin_config.ctty = cttyname;
  return true;
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
static struct {uid_t uid; gid_t gid; mode_t mode; bool valid;} saved_state;
/*----------------------------------------------------------------------------*/
void nclogin_ctty_user(uid_t uid, gid_t gid)
{
  static const mode_t mode = 0600;
  if (nclogin_config.adjustperm && (cttypath != NULL))
  {
    if (!saved_state.valid)
    {
      struct stat st;
      if (stat(cttypath, &st) >= 0)
      {
        saved_state.uid = st.st_uid;
        saved_state.gid = st.st_gid;
        saved_state.mode = st.st_mode;
        saved_state.valid = true;
      }
      else
        return;
    }
    if ((mode != 0) && (chmod(cttypath, mode) < 0))
      failure("Failed to adjust controlling TTY permissions: %m\n");
    if (chown(cttypath, uid, gid) < 0)
      failure("Failed to adjust controlling TTY ownership: %m\n");
  }
}
/*----------------------------------------------------------------------------*/
void nclogin_ctty_back(void)
{
  if (saved_state.valid)
  {
    if (chmod(cttypath, saved_state.mode) < 0)
      failure("Failed to restore controlling TTY permissions: %m\n");
    if (chown(cttypath, saved_state.uid, saved_state.gid) < 0)
      failure("Failed to restore controlling TTY ownership: %m\n");
    saved_state.valid = false;
  }
}
/*============================================================================*/
