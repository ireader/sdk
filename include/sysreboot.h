#ifndef _sysreboot_h_
#define _sysreboot_h_

#ifdef  __cplusplus
extern "C" {
#endif

int system_reboot(void);

int system_shutdown(void);

#ifdef  __cplusplus
}
#endif
#endif /* !_sysreboot_h_ */
