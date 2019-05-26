#include "channel.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 100
#define CAPACITY 200
#define COUNTER 10000
static int32_t s_counter = 0;

static int STDCALL channel_reader(void* param)
{
    int v;
    struct channel_t* q = (struct channel_t*)param;
    while(1)
    {
        channel_pop(q, &v);
        if(v == -1)
            break;
        atomic_increment32(&s_counter);
    }
    return 0;
}

static int STDCALL channel_writer(void* param)
{
    struct channel_t* q = (struct channel_t*)param;
    for(int i = 0; i < COUNTER; i++)
        channel_push(q, &i);
    return 0;
}

extern "C" void channel_test(void)
{
    struct channel_t* c;
    pthread_t writers[THREADS];
    pthread_t readers[THREADS];
    
    c = channel_create(CAPACITY, sizeof(int));
    
    for(int i = 0; i < THREADS; i++)
    {
        thread_create(&readers[i], channel_reader, c);
        thread_create(&writers[i], channel_writer, c);
    }
    
    for(int i = 0; i < THREADS; i++)
        thread_destroy(writers[i]);
   
    int quit = -1;
    for(int i = 0; i < THREADS; i++)
        channel_push(c, &quit);
    
    for(int i = 0; i < THREADS; i++)
        thread_destroy(readers[i]);
    
    assert(s_counter == THREADS * COUNTER);
    channel_destroy(&c);
    printf("channel test ok\n");
}
