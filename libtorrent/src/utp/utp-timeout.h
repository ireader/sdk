#ifndef _utp_timeout_h_
#define _utp_timeout_h_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// The initial timeout is set to 1000 milliseconds, and later updated according to the formula above. 
// For every packet consecutive subsequent packet that times out, the timeout is doubled.
#define UTP_TIMEOUT 1000

// delta = rtt - packet_rtt
// rtt_var += (abs(delta) - rtt_var) / 4;
// rtt += (packet_rtt - rtt) / 8;
// timeout = max(rtt + rtt_var * 4, 500);
/// @return timeout in ms
uint32_t utp_timeout(uint32_t rtt, int32_t rtt_var, uint32_t packet_rtt)
{
	uint32_t timeout;

	int32_t delta = rtt - packet_rtt;
	rtt_var += (abs(delta) - rtt_var) / 4;
	rtt += (packet_rtt - rtt) / 8;
	
	timeout = max(rtt + rtt_var * 4, 500);
	return timeout;
}

#endif /* !_utp_timeout_h_ */
