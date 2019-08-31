#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

int read_random(void* ptr, int bytes);

int stun_transaction_id(uint8_t* id, int bytes)
{
	return read_random(id, bytes);
}
