/* util.c */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
#include "main.h"
#include "util.h"
/*============================================================================*/
bool nclogin_util_canonpath(char *path)
{
  if (*path != '/')
    return false;

  size_t len = strlen(path);
  bool slash = false;
  char *cpos = path;
  char *mark = NULL;
  size_t cut = 0;

  do {
    char *mpos = ++cpos;
    unsigned int up = 0;
skip:
    while (*cpos == '/') cpos++;
    if (*cpos == '.')
    {
      unsigned int ddot = (cpos[1] == '.');
      switch (cpos[ddot + 1])
      {
      case '/':
        up += ddot;
        cpos += ddot + 2;
        goto skip;
      case '\0':
        up += ddot;
        cpos += ddot + 1;
        slash = true;
        break;
      default:;
      }
    }
    if (cpos > mpos)
    {
      if (cut > 0)
        memmove(mark, mark + cut, (mpos -= cut) - mark);
      if (up > 0)
      {
        mpos--;
        do {
          if (!(mpos > path) ||
              ((mpos = memrchr(path, '/', mpos - path)) == NULL))
            return false;
        } while (--up > 0);
        mpos++;
      }
      cut = cpos - mpos;
      mark = mpos;
    }
  } while ((cpos = memchr(cpos, '/', path + len - cpos)) != NULL);
  if (cut > 0)
    memmove(mark, mark + cut, path + (len -= cut) + 1 - mark);
  if (slash && (len > 1) && (path[len - 1] == '/'))
    path[len - 1] = '\0';
  return true;
}
/*============================================================================*/
