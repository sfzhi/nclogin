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
  va_list params;
  va_start(params, format);
  vsyslog(LOG_ERR, format, params);
  va_end(params);
}
/*============================================================================*/
static void setup_signals(void)
{
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGALRM, SIG_IGN);
  signal(SIGUSR1, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
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
  static const char optstr[] = "+:t:L:n:i:T:u::WmbBawslk";
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
    case 'a':
      nclogin_config.adjustctty = true;
      break;
    case 'w':
      nclogin_config.subprocess = true;
      break;
    case 's':
      nclogin_config.newsession = true;
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
  if (optind < argc)
    error(0, 0, "Unexpected command line argument: '%s'", argv[optind]);

  return execute();
}
/*============================================================================*/