#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>

int read_random(void* ptr, int bytes)
{
    HCRYPTPROV provider;
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return 0;
    
    CryptGenRandom(provider, bytes, (PBYTE)ptr);
    CryptReleaseContext(provider, 0);
    return bytes;
}

#elif defined(OS_LINUX) || defined(OS_MAC)
#include <stdio.h>

static int read_random_file(const char *file, void* ptr, int bytes)
{
    int n;
    FILE* fp;
    fp = fopen(file, "rb");
    if (NULL == fp)
        return -1;
    n = fread(ptr, bytes, 1, fp);
    fclose(fp);
    return n;
}

int read_random(void* ptr, int bytes)
{
    int r;
    r = read_random_file("/dev/urandom", ptr, bytes);
    if (-1 == r)
        r = read_random_file("/dev/random", ptr, bytes);
    return r;
}
#else

int read_random(void* ptr, int bytes)
{
    int i, v;
    for(i = 0; i < bytes / sizeof(int); i++)
    {
        v = rand();
        memcpy((char*)ptr + i * sizeof(int), &v, sizeof(int));
    }
    
    if(0 != bytes % sizeof(int))
    {
        v = rand();
        memcpy((char*)ptr + i * sizeof(int), &v, bytes % sizeof(int));
    }
    return bytes;
}

#endif
