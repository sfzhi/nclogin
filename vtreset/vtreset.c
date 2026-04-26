/* vtreset.c */
/******************************************************************************/
/* Copyright 2026 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#define _GNU_SOURCE
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h>
#include <stddef.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <unistd.h>
#include <fcntl.h>
/*============================================================================*/
int main(int argc, char *argv[]) {
  int result = 0;
  if ((argc > 1) && (*argv[1] != '\0')) {
    int fd = open(argv[1], O_RDWR|O_NOCTTY|O_CLOEXEC);
    if (fd >= 0) {
      struct vt_mode vt_mode = {
        .mode = VT_AUTO,
      };
      if (ioctl(fd, VT_SETMODE, &vt_mode) == -1) {
        fprintf(stderr, "Failed to set VT auto-switch mode: %m\n");
        result |= 1;
      }
      if (ioctl(fd, KDSETMODE, KD_TEXT) == -1) {
        fprintf(stderr, "Failed to set output to text mode: %m\n");
        result |= 2;
      }
      if (ioctl(fd, KDSKBMODE, (long)K_UNICODE) == -1) {
        fprintf(stderr, "Failed to set keyboard to UNICODE: %m\n");
        result |= 4;
      }
      if (ioctl(fd, KDSKBMETA, (long)K_ESCPREFIX) == -1) {
        fprintf(stderr, "Failed to set meta mode to escape: %m\n");
        result |= 8;
      }
      close(fd);
    } else {
      fprintf(stderr, "Failed to open device '%s': %m\n", argv[1]);
      result = 16;
    }
  }
  return result;
}
/*============================================================================*/
