// http://www.bittorrent.org/beps/bep_0029.html

#include "utp-header.h"
#include "byte-order.h"

#define UTP_VERSION 1

int utp_header_read(const uint8_t* data, int bytes, struct utp_header_t* utp)
{
	if (bytes < 20)
		return -1; // invalid packet

	if (UTP_VERSION != (data[0] & 0x0F))
		return -1; // invalid version

	utp->type = (data[0] >> 4) & 0x0F;
	utp->extension = data[1];
	be_read_uint16(data + 2, &utp->connection_id);
	be_read_uint32(data + 4, &utp->timestamp);
	be_read_uint32(data + 8, &utp->delay);
	be_read_uint32(data + 12, &utp->window_size);
	be_read_uint16(data + 16, &utp->seq_nr);
	be_read_uint16(data + 18, &utp->ack_nr);
	return 20;
}

int utp_header_write(const struct utp_header_t* utp, uint8_t* data, int bytes)
{
	if (bytes < 20)
		return -1;

	data[0] = (utp->type << 4) | UTP_VERSION;
	data[1] = utp->extension;
	be_write_uint16(data + 2, utp->connection_id);
	be_write_uint32(data + 4, utp->timestamp);
	be_write_uint32(data + 8, utp->delay);
	be_write_uint32(data + 12, utp->window_size);
	be_write_uint16(data + 16, utp->seq_nr);
	be_write_uint16(data + 18, utp->ack_nr);
	return 20;
}
