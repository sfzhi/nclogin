/* exec.c */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "exec.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <wordexp.h>
#include <unistd.h> // execvp()
/*============================================================================*/
bool nclogin_exec_command(exec_cmd_t cmd)
{
  const char *command = NULL;
  switch (cmd)
  {
  case xcmd_SHUTDOWN:
    command = "/sbin/poweroff";
    break;
  case xcmd_REBOOT:
    command = "/sbin/reboot";
    break;
  default:
    return true;
  }
  wordexp_t cmdargs = {.we_offs = 0, .we_wordv = NULL, .we_wordc = 0};
  if (wordexp(command, &cmdargs, WRDE_NOCMD|WRDE_UNDEF) == 0)
  {
    if ((cmdargs.we_wordc == 0) || (*cmdargs.we_wordv[0] == '\0'))
      failure("Command expanded to empty argv[0]: '%s'\n", command);
    else
    {
      nclogin_main_undo();
      execvp(cmdargs.we_wordv[0], cmdargs.we_wordv);
      failure("Failed to execute command '%s': %m\n", command);
    }
    wordfree(&cmdargs);
  }
  else
    failure("Failed to parse command line '%s'\n", command);
  return false;
}
/*============================================================================*/
