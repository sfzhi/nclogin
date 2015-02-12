/* form.h */
/******************************************************************************/
enum {fres_FAILURE, fres_SUCCESS, fres_SHUTDOWN, fres_REBOOT};
/*----------------------------------------------------------------------------*/
extern void nclogin_form_init(void);
extern int nclogin_form_main(login_info_t *info);
/*============================================================================*/
