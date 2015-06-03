/* ctty.c */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "ctty.h"
#include "util.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h> // snprintf()
#include <fcntl.h> // open(), O_*
#include <sys/stat.h> // stat(), S_ISCHR
#include <sys/ioctl.h> // ioctl()
#include <termios.h> // tcgetsid()
#include <stdlib.h> // strtoll()
#include <unistd.h>
#include <errno.h>
/*============================================================================*/
static int cttyfd = -1;
static dev_t cttydev = 0;
static char *cttypath = NULL;
static char *cttyname = NULL;
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
static bool try_tty_path(const char *path)
{
  struct stat st;
  return (stat(path, &st) >= 0) &&
    S_ISCHR(st.st_mode) && (st.st_rdev == cttydev);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static char *try_tty_fd(int fd)
{
  if (tcgetsid(fd) == getsid(0))
  {
    char *path;
    struct stat st;
    if ((fstat(fd, &st) >= 0) && S_ISCHR(st.st_mode) &&
        (st.st_rdev == cttydev) && ((path = ttyname(fd)) != NULL) &&
        (strcmp(path, "/dev/tty") != 0) && try_tty_path(path))
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
static bool try_tty_stdio(void)
{
  char *path = try_tty_fd(STDIN_FILENO);
  if (path == NULL)
    path = try_tty_fd(STDOUT_FILENO);
  if (path == NULL)
    path = try_tty_fd(STDERR_FILENO);
  return (path != NULL) && set_ctty_path(path, false);
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static bool try_tty_links(void)
{
  char sbuf[64];
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
    if ((devpath != NULL) && try_tty_path(devpath))
    {
      if (cttyfd < 0)
        cttyfd = open(devpath, O_RDONLY|O_NOATIME|O_CLOEXEC);
      return set_ctty_path(devpath, standard_path);
    }
  }
  return false;
}
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
void nclogin_ctty_init(void)
{
  if (((cttydev = get_ctty_dev()) != 0) && (try_tty_stdio() || try_tty_links()))
    infomsg("Identified controlling TTY device: %s (%s)\n", cttyname, cttypath);
  else
    warning("Unable to identify controlling TTY device\n");
  if (cttyfd < 0)
    cttyfd = open("/dev/tty", O_RDONLY|O_NOATIME|O_CLOEXEC);
  if (nclogin_config.ctty == NULL)
    nclogin_config.ctty = cttyname;
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
      failure("Failed to adjust controlling TTY permissions: %m");
    if (chown(cttypath, uid, gid) < 0)
      failure("Failed to adjust controlling TTY ownership: %m");
  }
}
/*----------------------------------------------------------------------------*/
void nclogin_ctty_back(void)
{
  if (saved_state.valid)
  {
    if (chmod(cttypath, saved_state.mode) < 0)
      failure("Failed to restore controlling TTY permissions: %m");
    if (chown(cttypath, saved_state.uid, saved_state.gid) < 0)
      failure("Failed to restore controlling TTY ownership: %m");
    saved_state.valid = false;
  }
}
/*============================================================================*/
