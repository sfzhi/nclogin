/* utmp.h */
/******************************************************************************/
extern void nclogin_utmp_init(void);
extern void nclogin_utmp_user(const char *name, uid_t uid, pid_t pid);
extern void nclogin_utmp_self(bool live);
/*============================================================================*/
