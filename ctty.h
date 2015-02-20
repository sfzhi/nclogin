/* ctty.h */
/******************************************************************************/
extern void nclogin_ctty_init(void);
extern bool nclogin_ctty_grab(void);
extern bool nclogin_ctty_pgrp(void);
/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
extern void nclogin_ctty_user(uid_t uid, gid_t gid, mode_t mode);
extern void nclogin_ctty_back(void);
/*============================================================================*/
