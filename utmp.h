/* utmp.h */
/******************************************************************************/
extern void nclogin_utmp_init(void);
extern void nclogin_utmp_user(const char *name);
extern void nclogin_utmp_self(bool live);
/*============================================================================*/
