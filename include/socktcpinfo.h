#ifndef _socktcpinfo_h_
#define _socktcpinfo_h_

#include "sys/sock.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
struct tcp_info {
	uint8_t	tcpi_state;
	uint8_t	tcpi_ca_state;
	uint8_t	tcpi_retransmits;
	uint8_t	tcpi_probes;
	uint8_t	tcpi_backoff;
	uint8_t	tcpi_options;
	uint8_t	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
	uint8_t	tcpi_delivery_rate_app_limited : 1, tcpi_fastopen_client_fail : 2;

	uint32_t	tcpi_rto;
	uint32_t	tcpi_ato;
	uint32_t	tcpi_snd_mss;
	uint32_t	tcpi_rcv_mss;

	uint32_t	tcpi_unacked;
	uint32_t	tcpi_sacked;
	uint32_t	tcpi_lost;
	uint32_t	tcpi_retrans;
	uint32_t	tcpi_fackets;

	/* Times. */
	uint32_t	tcpi_last_data_sent;
	uint32_t	tcpi_last_ack_sent;     /* Not remembered, sorry. */
	uint32_t	tcpi_last_data_recv;
	uint32_t	tcpi_last_ack_recv;

	/* Metrics. */
	uint32_t	tcpi_pmtu;
	uint32_t	tcpi_rcv_ssthresh;
	uint32_t	tcpi_rtt;
	uint32_t	tcpi_rttvar;
	uint32_t	tcpi_snd_ssthresh;
	uint32_t	tcpi_snd_cwnd;
	uint32_t	tcpi_advmss;
	uint32_t	tcpi_reordering;

	uint32_t	tcpi_rcv_rtt;
	uint32_t	tcpi_rcv_space;

	uint32_t	tcpi_total_retrans;

	uint64_t	tcpi_pacing_rate;
	uint64_t	tcpi_max_pacing_rate;
	uint64_t	tcpi_bytes_acked;    /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
	uint64_t	tcpi_bytes_received; /* RFC4898 tcpEStatsAppHCThruOctetsReceived */
	uint32_t	tcpi_segs_out;	     /* RFC4898 tcpEStatsPerfSegsOut */
	uint32_t	tcpi_segs_in;	     /* RFC4898 tcpEStatsPerfSegsIn */

	uint32_t	tcpi_notsent_bytes;
	uint32_t	tcpi_min_rtt;
	uint32_t	tcpi_data_segs_in;	/* RFC4898 tcpEStatsDataSegsIn */
	uint32_t	tcpi_data_segs_out;	/* RFC4898 tcpEStatsDataSegsOut */

	uint64_t   tcpi_delivery_rate;

	uint64_t	tcpi_busy_time;      /* Time (usec) busy sending data */
	uint64_t	tcpi_rwnd_limited;   /* Time (usec) limited by receive window */
	uint64_t	tcpi_sndbuf_limited; /* Time (usec) limited by send buffer */

	uint32_t	tcpi_delivered;
	uint32_t	tcpi_delivered_ce;

	uint64_t	tcpi_bytes_sent;     /* RFC4898 tcpEStatsPerfHCDataOctetsOut */
	uint64_t	tcpi_bytes_retrans;  /* RFC4898 tcpEStatsPerfOctetsRetrans */
	uint32_t	tcpi_dsack_dups;     /* RFC4898 tcpEStatsStackDSACKDups */
	uint32_t	tcpi_reord_seen;     /* reordering events seen */

	uint32_t	tcpi_rcv_ooopack;    /* Out-of-order packets received */

	uint32_t	tcpi_snd_wnd;	     /* peer's advertised receive window after
								 * scaling (bytes)
								 */
};

#endif

static inline int socket_gettcpinfo(IN socket_t sock, OUT struct tcp_info* info)
{
#if defined(OS_WINDOWS)
#if defined(SIO_TCP_INFO)
	DWORD ver = 0;
	DWORD size = 0;
	struct TCP_INFO_v0 v0;
	memset(&v0, 0, sizeof(v0));
	if (0 != WSAIoctl(sock, SIO_TCP_INFO, &ver, sizeof(ver), &v0, sizeof(v0), &size, NULL, NULL))
		return -1;
	info->tcpi_state = v0.state;
	info->tcpi_rcv_mss = v0.Mss;
	info->tcpi_snd_mss = v0.Mss;
	info->tcpi_rtt = v0.RttUs;
	info->tcpi_min_rtt = v0.MinRttUs;
	info->tcpi_snd_wnd = v0.SndWnd;
	info->tcpi_rwnd_limited = v0.RcvWnd;
	info->tcpi_bytes_sent = v0.BytesOut;
	info->tcpi_bytes_received = v0.BytesIn;
	info->tcpi_bytes_retrans = v0.BytesRetrans;
	info->tcpi_reordering = v0.BytesReordered;
	info->tcpi_fackets = v0.FastRetrans;
	info->tcpi_dsack_dups = v0.DupAcksIn;
	return 0;
#else
	return -1;
#endif
#else
	socklen_t bytes;
	bytes = sizeof(struct tcp_info);
	return getsockopt(sock, IPPROTO_TCP, TCP_INFO, (void *)info, &bytes);
#endif
}

#endif
