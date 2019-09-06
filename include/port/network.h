#ifndef _port_network_h_
#define _port_network_h_

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*network_getip_fcb)(void* param, const char* mac, const char* name, int dhcp, const char* ip, const char* netmask, const char* gateway);

int network_getip(network_getip_fcb fcb, void* param);
int network_setip(const char* name, int enableDHCP, const char* ip, const char* netmask, const char* gateway);

// dns: primary;secondary
// dns: 222.222.202.100;8.8.8.8
int network_getdns(const char* name, char primary[65], char secondary[65]);
int network_setdns(const char* name, const char* primary, const char *secondary);

int network_getgateway(char gateway[65]);
int network_setgateway(const char* gateway);

#ifdef  __cplusplus
}
#endif

#endif /* !_port_network_h_ */
