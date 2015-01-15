#ifndef _sysntpconfig_h_
#define _sysntpconfig_h_

#ifdef  __cplusplus
extern "C" {
#endif

int system_ntp_setenable(int enable);
int system_ntp_getstatus(int *enable);

#ifdef  __cplusplus
}
#endif
#endif /* !_sysntpconfig_h_ */
