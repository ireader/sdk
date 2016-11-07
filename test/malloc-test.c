#include <malloc.h>

void malloc_test(void)
{
	// since glibc 2.10
	mallopt(M_ARENA_MAX, 4 /*cpu*/); // limit multithread virtual memory
}
