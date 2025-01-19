/* ctty.h */
/******************************************************************************/
/* Copyright 2015-2025 Sergei Zhirikov <sfzhi@yahoo.com>                      */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
extern bool nclogin_ctty_init(void);
extern bool nclogin_ctty_grab(void);
extern bool nclogin_ctty_pgrp(void);
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
extern void nclogin_ctty_user(uid_t uid, gid_t gid);
extern void nclogin_ctty_back(void);
/*============================================================================*/
