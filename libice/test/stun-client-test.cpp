#include "stun-client.h"
#include "turn-client.h"
#include "tls-socket.h"
#include "sockutil.h"

// https://gist.github.com/zziuni/3741933
// https://gist.github.com/mondain/b0ec1cf5f60ae726202e
// http://olegh.ftp.sh/public-stun.txt
// https://stackoverflow.com/questions/20068944/webrtc-stun-stun-l-google-com19302

extern "C" void stun_message_test(void);
extern "C" void turn_client_test()
{
    int r;
    socket_t udp;
    tls_socket_t* ssl;
    socklen_t addrlen;
    sockaddr_in addr;
    
    uint8_t data[1400];
    turn_client_t* turn;
    struct turn_client_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    //handler.onbind = stun_client_onbind;
    //handler.onsharedsecret = stun_client_onsharedsecret;
    
    //stun_message_test();
    
    socket_init();
    tls_socket_init();
    udp = socket_udp();
    //ssl = tls_socket_connect("stun.stunprotocol.org", 3478, 5000);
    //r = socket_addr_from(&addr, &addrlen, "stunserver.org", 3478);
    //r = socket_addr_from_ipv4(&addr, "stun.stunprotocol.org", 3478);
    r = socket_addr_from_ipv4(&addr, "numb.viagenie.ca", 3478);
    //r = socket_addr_from_ipv4(&addr, "10.8.126.191", 3478);
    //r = socket_addr_from_ipv4(&addr, "192.158.29.39", 3478);
    addrlen = sizeof(struct sockaddr_in);
    assert(0 == r);
    
    turn = turn_client_create(&handler);
    
    r = turn_client_allocate(turn, (sockaddr*)&addr, addrlen, data, sizeof(data), NULL);
    assert(r == socket_sendto(udp, data, r, 0, (sockaddr*)&addr, addrlen));
    addrlen = sizeof(addr);
    r = socket_recvfrom(udp, data, sizeof(data), 0, (sockaddr*)&addr, &addrlen);
    r = turn_client_input(turn, data, r, (sockaddr*)&addr, addrlen);
    
    r = turn_client_allocate(turn, (sockaddr*)&addr, addrlen, data, sizeof(data), NULL);
    assert(r == socket_sendto(udp, data, r, 0, (sockaddr*)&addr, addrlen));
    addrlen = sizeof(addr);
    r = socket_recvfrom(udp, data, sizeof(data), 0, (sockaddr*)&addr, &addrlen);
    r = turn_client_input(turn, data, r, (sockaddr*)&addr, addrlen);
    
    turn_client_destroy(turn);
    tls_socket_cleanup();
    socket_cleanup();
}

extern "C" void stun_client_test()
{
    turn_client_test();
    
	int r;
	socket_t udp;
    tls_socket_t* ssl;
    socklen_t addrlen;
	sockaddr_in addr;
	
	uint8_t data[1400];
	stun_client_t* stun;
	struct stun_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	//handler.onbind = stun_client_onbind;
	//handler.onsharedsecret = stun_client_onsharedsecret;

	socket_init();
    tls_socket_init();
	udp = socket_udp();
    ssl = tls_socket_connect("stun.stunprotocol.org", 3478, 5000);
	//r = socket_addr_from(&addr, &addrlen, "stunserver.org", 3478);
	r = socket_addr_from_ipv4(&addr, "stun.stunprotocol.org", 3478);
    addrlen = sizeof(struct sockaddr_in);
	assert(0 == r);

	stun = stun_client_create(STUN_RFC_3489, &handler);
    r = stun_client_shared_secret(stun, (sockaddr*)&addr, addrlen, data, sizeof(data), NULL);
    int n = tls_socket_write(ssl, data, r);
    assert(r == n);
    r = tls_socket_read(ssl, data, sizeof(data));
    r = stun_client_input(stun, data, r, (sockaddr*)&addr, addrlen);

	r = stun_client_bind(stun, (sockaddr*)&addr, addrlen, data, sizeof(data), NULL);
    //assert(r == socket_sendto(udp, data, r, 0, (sockaddr*)&addr, addrlen));
    assert(r == tls_socket_write(ssl, data, r));
	addrlen = sizeof(addr);
	r = socket_recvfrom(udp, data, sizeof(data), 0, (sockaddr*)&addr, &addrlen);
	r = stun_client_input(stun, data, r, (sockaddr*)&addr, addrlen);

	stun_client_destroy(stun);
    tls_socket_cleanup();
	socket_cleanup();
}

