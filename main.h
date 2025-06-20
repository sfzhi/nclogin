/* main.h */
/******************************************************************************/
/* Copyright 2015-2025 Sergei Zhirikov <sfzhi@yahoo.com>                      */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#define _GNU_SOURCE
#include <stddef.h> // NULL, size_t
#include <string.h> // mem*(), str*()
#include <stdbool.h> // bool, true, false
#include <sys/types.h> // uid_t, gid_t, pid_t
/*============================================================================*/
typedef struct {
  const char *name;
  const char *info;
  const char *home;
  const char *exec;
  uid_t uid;
  gid_t gid;
} login_info_t;
/*----------------------------------------------------------------------------*/
typedef struct {
  const char *term;
  const char *lang;
  const char *name;
  const char *init;
  const char *exec;
  const char *ctty;
  const char *utid;
  char **extraargv;
  int   extraargc;
  bool changectty;
  bool wipescreen;
  bool monochrome;
  bool background;
  bool checkboard;
  bool enablequit;
  bool restricted;
  bool updateutmp;
  bool adjustperm;
  bool subprocess;
  bool newsession;
  bool skipsetuid;
  bool exportctty;
  bool loginshell;
  bool appenduser;
  bool killorphan;
} config_data_t;
/*============================================================================*/
extern config_data_t nclogin_config;
/*============================================================================*/
typedef enum {
  msgt_INFOMSG, msgt_WARNING, msgt_FAILURE, msgt_AUTHMSG, msgt_AUTHERR,
} msg_type_t;
/*----------------------------------------------------------------------------*/
__attribute__((__format__(__printf__, 2, 3)))
extern void message(msg_type_t type, const char *format, ...);
/*----------------------------------------------------------------------------*/
#define infomsg(fmt, ...) message(msgt_INFOMSG, fmt, ##__VA_ARGS__)
#define warning(fmt, ...) message(msgt_WARNING, fmt, ##__VA_ARGS__)
#define failure(fmt, ...) message(msgt_FAILURE, fmt, ##__VA_ARGS__)
#define authmsg(fmt, ...) message(msgt_AUTHMSG, fmt, ##__VA_ARGS__)
#define autherr(fmt, ...) message(msgt_AUTHERR, fmt, ##__VA_ARGS__)
/*============================================================================*/
void nclogin_main_undo(void);
/*============================================================================*/
