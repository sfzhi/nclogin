/* exec.h */
/******************************************************************************/
/* Copyright 2015 Sergei Zhirikov <sfzhi@yahoo.com>                           */
/* This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).        */
/* It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).     */
/*============================================================================*/
typedef enum {xcmd_SHUTDOWN, xcmd_REBOOT} exec_cmd_t;
/*----------------------------------------------------------------------------*/
extern bool nclogin_exec_command(exec_cmd_t cmd);
/*============================================================================*/
