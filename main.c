/* main.c */
/******************************************************************************/
#include "main.h"
#include "ctty.h"
#include "form.h"
#include "utmp.h"
#include "user.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <unistd.h> // getopt(), chdir(), gethostname()
#include <stdlib.h> // setenv()
#include <signal.h>
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <locale.h>
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdio.h>
#include <errno.h> // program_invocation_short_name
#include <error.h> // error()
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <stdarg.h>
#include <syslog.h>
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <limits.h> // HOST_NAME_MAX
/*============================================================================*/
config_data_t nclogin_config;
/*============================================================================*/
void message(msg_type_t type, const char *format, ...)
{
  int priority;
  switch (type)
  {
  case msgt_INFOMSG:
    priority = LOG_INFO;
    break;
  case msgt_WARNING:
    priority = LOG_WARNING;
    break;
  case msgt_FAILURE:
    priority = LOG_ERR;
    break;
  case msgt_AUTHMSG:
    priority = LOG_AUTH|LOG_INFO;
    break;
  case msgt_AUTHERR:
    priority = LOG_AUTH|LOG_ERR;
    break;
  default:
    priority = LOG_INFO;
  }
  va_list params;
  va_start(params, format);
  vsyslog(priority, format, params);
  va_end(params);
}
/*============================================================================*/
static void ignore_signals(bool ignore)
{
  sighandler_t handler = ignore? SIG_IGN: SIG_DFL;
  signal(SIGHUP, handler);
  signal(SIGINT, handler);
  signal(SIGPIPE, handler);
  signal(SIGALRM, handler);
  signal(SIGUSR1, handler);
  signal(SIGUSR2, handler);
  signal(SIGTSTP, handler);
  signal(SIGTTIN, handler);
  signal(SIGTTOU, handler);
}
/*============================================================================*/
void nclogin_main_undo(void)
{
  ignore_signals(false);
  closelog();
}
/*============================================================================*/
static void setup_signals(void)
{
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
  ignore_signals(true);
}
/*============================================================================*/
static int execute(void)
{
  chdir("/");
  setup_signals();

  if (nclogin_config.term != NULL)
    setenv("TERM", nclogin_config.term, 0);
  else
    nclogin_config.term = getenv("TERM");

  setlocale(LC_ALL, (nclogin_config.lang != NULL)? nclogin_config.lang: "");
  openlog(program_invocation_short_name, LOG_PID|LOG_CONS, LOG_DAEMON);

  if (nclogin_config.skipsetuid && (nclogin_config.exec == NULL))
  {
    warning("Will not run default user shell as superuser\n");
    nclogin_config.skipsetuid = false;
  }

  if (nclogin_config.name == NULL)
  {
    static char hostname[HOST_NAME_MAX + 1];
    if ((gethostname(hostname, sizeof(hostname)) < 0) ||
        (memchr(hostname, '\0', sizeof(hostname)) == NULL))
      *hostname = '\0';
    nclogin_config.name = hostname;
  }

  nclogin_form_init();
  nclogin_ctty_init();
  nclogin_utmp_init();

  bool loop = false;
  login_info_t login_info;
  do {
    memset(&login_info, 0, sizeof(login_info));
    switch (nclogin_form_main(&login_info))
    {
    case fres_SUCCESS:
      loop = nclogin_user_exec(&login_info);
      break;
    case fres_SHUTDOWN:
      printf("Shutdown\n");
      loop = false;
      break;
    case fres_REBOOT:
      printf("Reboot\n");
      loop = false;
      break;
    default:;
      loop = false;
    }
    nclogin_utmp_self(loop);
  } while(loop);

  closelog();
  return 0;
}
/*============================================================================*/
int main(int argc, char *argv[])
{
  opterr = 0;
  int optchr;
  static const char optstr[] = "+:t:L:n:i:e:T:u::WmbBqrPwsSylk";
  while ((optchr = getopt(argc, argv, optstr)) != -1)
  {
    switch(optchr)
    {
    case 't':
      nclogin_config.term = optarg;
      break;
    case 'L':
      nclogin_config.lang = optarg;
      break;
    case 'n':
      nclogin_config.name = optarg;
      break;
    case 'i':
      nclogin_config.init = optarg;
      break;
    case 'e':
      nclogin_config.exec = optarg;
      break;
    case 'T':
      nclogin_config.ctty = optarg;
      break;
    case 'u':
      nclogin_config.utid = optarg;
      nclogin_config.updateutmp = true;
      break;
    case 'W':
      nclogin_config.wipescreen = true;
      break;
    case 'm':
      nclogin_config.monochrome = true;
      break;
    case 'b':
      nclogin_config.background = true;
      break;
    case 'B':
      nclogin_config.checkboard = true;
      break;
    case 'q':
      nclogin_config.enablequit = true;
      break;
    case 'r':
      nclogin_config.restricted = true;
      break;
    case 'P':
      nclogin_config.adjustperm = true;
      break;
    case 'w':
      nclogin_config.subprocess = true;
      break;
    case 's':
      nclogin_config.newsession = true;
      break;
    case 'S':
      nclogin_config.skipsetuid = true;
      break;
    case 'y':
      nclogin_config.exportctty = true;
      break;
    case 'l':
      nclogin_config.loginshell = true;
      break;
    case 'k':
      nclogin_config.killorphan = true;
      break;
    case '?':
      error(1, 0, "Unknown command line option: -%c", optopt);
      break;
    case ':':
      error(1, 0, "Missing parameter for -%c option", optopt);
      break;
    default:
      error(1, 0, "Failed to process command line options");
    }
  }
  nclogin_config.extraargv = argv + optind;
  nclogin_config.extraargc = argc - optind;
  return execute();
}
/*============================================================================*/
