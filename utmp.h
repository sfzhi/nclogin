/* utmp.h */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
extern void nclogin_utmp_init(void);
extern void nclogin_utmp_user(const char *name, uid_t uid, pid_t pid);
extern void nclogin_utmp_self(bool live);
/*============================================================================*/
