/* lock.h */
/******************************************************************************/
/* Copyright 2026 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
extern const char *nclogin_lock_path(void);
extern bool nclogin_lock_init(int fd);
extern void nclogin_lock_done(void);
/*============================================================================*/
