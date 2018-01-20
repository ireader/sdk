#ifndef _utp_header_h_
#define _utp_header_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum utp_type_t
{
	UTP_ST_DATA = 0,
	UTP_ST_FIN,
	UTP_ST_STATE,
	UTP_ST_RESET,
	UTP_ST_SYN,
};

struct utp_header_t
{
//	uint8_t ver;
	uint8_t type;
	uint8_t extension;
	uint16_t connection_id;
	uint32_t timestamp;
	uint32_t delay;
	uint32_t window_size;
	uint16_t seq_nr;
	uint16_t ack_nr;
};

int utp_header_read(const uint8_t* data, int bytes, struct utp_header_t* utp);
int utp_header_write(const struct utp_header_t* utp, uint8_t* data, int bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_utp_header_h_ */
