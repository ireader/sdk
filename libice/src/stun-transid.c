#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#if defined(OS_WINDOWS)
#include <Windows.h>

int stun_transaction_id(uint8_t* id, int bytes)
{
	HCRYPTPROV provider;
	CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
	CryptGenRandom(provider, bytes, id);
	CryptReleaseContext(provider, 0);
	return 0;
}

#else
#include <unistd.h>
#include <fcntl.h>

static int read_random(uint8_t *id, int bytes, const char *file)
{
	int fd, err;
	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;
	err = read(fd, id, bytes);
	close(fd);
	return err;
}

int stun_transaction_id(uint8_t* id, int bytes)
{
	if (read_random(id, bytes, "/dev/urandom") == bytes)
		return 0;
	if (read_random(id, bytes, "/dev/random") == bytes)
		return 0;
    return -1;
}
#endif
