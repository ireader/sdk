#ifndef _file_watcher_h_
#define _file_watcher_h_

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_WATCHER_EVENT_OPEN		0x0001 // *[linux only]
#define FILE_WATCHER_EVENT_CLOSE	0x0002 // *[linux only]
#define FILE_WATCHER_EVENT_READ		0x0004 // read / execv
#define FILE_WATCHER_EVENT_WRITE	0x0008 // write / truncate
#define FILE_WATCHER_EVENT_CHMOD	0x0010 // change file/directory attribution(metadata)

#define FILE_WATCHER_EVENT_CREATE	0x0100 // directory only
#define FILE_WATCHER_EVENT_DELETE	0x0200
#define FILE_WATCHER_EVENT_RENAME	0x0400


typedef void* file_watcher_t;
typedef int (*file_watcher_notify)(void* param, void* file, int events, const char* name, int bytes);

/// Create file watcher
file_watcher_t file_watcher_create(void);
void file_watcher_destroy(file_watcher_t watcher);

/// @return file handler, NULL if error
void* file_watcher_add(file_watcher_t watcher, const char* fileordir, int events);
/// @return 0-ok, other-error
int file_watcher_delete(file_watcher_t watcher, void* file);

/// @param[in] timeout timeout in ms
/// @return 0-OK
int file_watcher_process(file_watcher_t watcher, int timeout, file_watcher_notify onnotify, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_file_watcher_h_ */
