#ifndef _utp_window_h_
#define _utp_window_h_

#include <stdint.h>

// delay_factor = off_target / CCONTROL_TARGET;
// window_factor = outstanding_packet / max_window;
// scaled_gain = MAX_CWND_INCREASE_PACKETS_PER_RTT * delay_factor * window_factor;
// max_window += scaled_gain;
/// @return utp max window size
int32_t utp_max_window(int32_t base_delay, int32_t timestamp_diff, int32_t send_bytes, int32_t max_window)
{
	const int CCONTROL_TARGET = 100; // ms
	const int MAX_CWND_INCREASE_PACKETS_PER_RTT = 150;

	int32_t our_delay = timestamp_diff - base_delay;
	int32_t off_target = CCONTROL_TARGET - our_delay;
	double delay_factor = off_target * 1.0 / CCONTROL_TARGET;
	double window_factor = send_bytes * 1.0 / max_window;
	double scaled_gain = MAX_CWND_INCREASE_PACKETS_PER_RTT * delay_factor * window_factor;
	max_window += (int32_t)scaled_gain;

	// If max_window becomes less than 0, it is set to 0.
	// In this state, the socket will trigger a timeout and force the window size to one packet size
	if (max_window <= 0)
		max_window = 150;
	return max_window;
}

int32_t utp_packet_loss(int32_t max_window)
{
	max_window /= 2;
	assert(max_window >= 0);
	if (max_window <= 0)
		max_window = 150;
	return max_window;
}

#endif /* !_utp_window_h_ */
