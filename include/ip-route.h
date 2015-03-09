#ifndef _ip_route_h_
#define _ip_route_h_

#ifdef __cplusplus
extern "C" {
#endif

int ip_route_get(const char* distination, char ip[40]);

int ip_local(char ip[40]);

/// check valid network address(IPv4/IPv6)
/// IPv4: ddd.ddd.ddd.ddd
/// IPv6: x:x:x:x:x:x:x:x
/// IPv6: ::1
/// IPv4-mapped IPv6: x:x:x:x:x:x:d.d.d.d, such as: ::FFFF:204.152.189.116
/// @param[in] ip IPv4/IPv6 string.
int ip_valid(const char* ip);

#ifdef __cplusplus
}
#endif
#endif /* !_ip_route_h_ */
