# WIN32/WIN64/Linux/MacOS

# AIO
1. IOCP (source/aio-socket-iocp.c)
2. epoll (source/aio-socket-epoll.c)
3. kqueue (source/aio-socket-kqueue.c)

# atomic  (include/sys/atomic.h)
1. increment32/increment64
2. decrement32/decrement64
3. add32/add64
4. cas32/cas64
5. cas_ptr

# socket (include/sys/socket.h)
1. IPv4/IPv6 support
2. connect auto try IPv4/IPv6
3. common socket options
4. ip/dns convert

# thread (include/sys/thread.h)
1. thread_create/thread_destroy
2. get/set priority
3. thread id
4. yield

# process  (include/sys/process.h)
1. process_create/process_destroy/process_kill
2. get process name
3. close parent process handle/fd

# locker  (include/sys/locker.h)
1. create/destroy
2. lock/unlock
3. trylock
