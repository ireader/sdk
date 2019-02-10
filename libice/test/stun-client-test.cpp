#include "stun-client.h"
#include "sockutil.h"

extern "C" void stun_client_test()
{
	int r;
	socket_t udp;
	socklen_t addrlen;
	sockaddr_storage addr;
	
	uint8_t data[1400];
	stun_client_t* stun;
	struct stun_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	//handler.onbind = stun_client_onbind;
	//handler.onsharedsecret = stun_client_onsharedsecret;

	socket_init();
	udp = socket_udp();
	//r = socket_addr_from(&addr, &addrlen, "stunserver.org", 3478);
	r = socket_addr_from(&addr, &addrlen, "stun.stunprotocol.org", 3478);
	assert(0 == r);

	stun = stun_client_create(STUN_RFC_3489, &handler);
	r = stun_client_bind(stun, (sockaddr*)&addr, addrlen, data, sizeof(data), NULL);
	assert(r > 0);

	assert(r == socket_sendto(udp, data, r, 0, (sockaddr*)&addr, addrlen));

	addrlen = sizeof(addr);
	r = socket_recvfrom(udp, data, sizeof(data), 0, (sockaddr*)&addr, &addrlen);
	r = stun_client_input(stun, data, r, (sockaddr*)&addr, addrlen);

	stun_client_destroy(stun);
	socket_cleanup();
}
