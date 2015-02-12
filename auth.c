/* auth.c */
/******************************************************************************/
#include "main.h"
#include "auth.h"
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include <pwd.h> // setpwent(), getpwnam(), endpwent()
#include <shadow.h> // setspent(), getspnam(), endspent()
#include <unistd.h> // crypt()
#include <errno.h>
/*============================================================================*/
int nclogin_auth_user(login_info_t *info, const char *login, const char *passw)
{
  int result = -1;
  struct passwd *pw;
  setpwent();
  do {
    errno = 0;
    pw = getpwnam(login);
  } while ((pw == NULL) && (errno == EINTR));
  if (pw != NULL)
  {
    setspent();
    errno = 0;
    struct spwd *sp = getspnam(login);
    if (sp != NULL)
    {
      char *encpw = crypt(passw, sp->sp_pwdp);
      if ((encpw != NULL) && (result = (strcmp(encpw, sp->sp_pwdp) == 0)))
      {
        info->name = pw->pw_name;
        info->info = pw->pw_gecos;
        info->home = pw->pw_dir;
        info->exec = pw->pw_shell;
        info->uid = pw->pw_uid;
        info->gid = pw->pw_gid;
      }
    }
    else if (errno == 0)
      authmsg("Failed to lookup shadow entry for '%s'\n", login);
    else
      authmsg("Failed to lookup shadow entry for '%s': %m\n", login);
    endspent();
  }
  else if (errno == 0)
    authmsg("User account '%s' does not exist\n", login);
  else
    authmsg("Failed to lookup user '%s': %m\n", login);
  endpwent();
  return result;
}
/*============================================================================*/
