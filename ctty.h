/* ctty.h */
/******************************************************************************/
extern void nclogin_ctty_init(void);
extern bool nclogin_ctty_grab(void);
extern bool nclogin_ctty_pgrp(void);
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
extern void nclogin_ctty_user(uid_t uid, gid_t gid);
extern void nclogin_ctty_back(void);
/*============================================================================*/
