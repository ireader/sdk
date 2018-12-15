#ifndef _ip_route_h_
#define _ip_route_h_

#ifdef __cplusplus
extern "C" {
#endif

/// get local network interface(IPv4 address) to target network
/// @param[in] destination target network address, ipv4 address only(don't be DNS)
/// @param[out] ip local ip address
/// @return 0-ok, -1-error(get errno)
int ip_route_get(const char* destination, char ip[40]);

int ip_local(char ip[65]);

#ifdef __cplusplus
}
#endif
#endif /* !_ip_route_h_ */
