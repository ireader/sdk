#ifndef _ip_route_h_
#define _ip_route_h_

#ifdef __cplusplus
extern "C" {
#endif

int ip_route_get(const char* distination, char ip[40]);

int ip_local(char ip[40]);

#ifdef __cplusplus
}
#endif
#endif /* !_ip_route_h_ */
