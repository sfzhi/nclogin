/* utmp.h */
/******************************************************************************/
extern void nclogin_utmp_init(void);
extern void nclogin_utmp_user(const char *name, pid_t pid);
extern void nclogin_utmp_self(bool live);
/*============================================================================*/
