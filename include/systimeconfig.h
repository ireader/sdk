#ifndef _systimeconfig_h_
#define _systimeconfig_h_

#ifdef  __cplusplus
extern "C" {
#endif

// 2012-08-17 16:50:43.345
int system_gettime(char time[24]);

// system_settime("2012-08-17 16:50:43");
// system_settime("2012-08-17 16:50:43.345");
int system_settime(const char* time);

#if !(defined(_WIN32) || defined(_WIN64))
int system_ntp_setconfigpath(const char* path);
int system_ntp_getconfigpath(char* path, int pathLen);
#endif

// servers: server1;server2
// servers: 0.cn.pool.ntp.org;1.cn.pool.ntp.org
int system_ntp_getserver(char* servers, int serversLen);
int system_ntp_setserver(const char *servers);

#ifdef  __cplusplus
}
#endif

#endif /* !_systimeconfig_h_ */
