#ifndef _async_pipe_h_
#define _async_pipe_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void* async_pipe_t;
typedef void (*async_pipe_onread)(void* param, int code, int bytes);
typedef void (*async_pipe_onwrite)(void* param, int code, int bytes);

// Windows: \\.\pipe\pipename
// Linux: create pipe and pass pipe fd by name(%d.%d)
async_pipe_t async_pipe_server(const char* name);

async_pipe_t async_pipe_open(const char* name);

int async_pipe_close(async_pipe_t pipe);

// msg must be valid until callback be called
// return: 0-success, other-error
int async_pipe_read(async_pipe_t pipe, void* msg, int len, async_pipe_onread callback, void* param);

// msg must be valid until callback be called
// return: 0-success, other-error
int async_pipe_write(async_pipe_t pipe, const void* msg, int len, async_pipe_onwrite callback, void* param);

#ifdef __cplusplus
}
#endif

#endif /* !_aync_pipe_h_ */
