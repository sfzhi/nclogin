/* form.h */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
enum {fres_ABANDON, fres_FAILURE, fres_SUCCESS, fres_SHUTDOWN, fres_REBOOT};
/*----------------------------------------------------------------------------*/
extern void nclogin_form_init(void);
extern int nclogin_form_main(login_info_t *info);
/*============================================================================*/
