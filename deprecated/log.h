#ifndef _log_h_
#define _log_h_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __FILE_NAME__
#define FILE_LOG_ERROR(fmt, ...)			log_error("(%s:%d) " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define FILE_LOG_INFO(fmt, ...)				log_info("(%s:%d) " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define FILE_LOG_DEBUG(fmt, ...)			log_debug("(%s:%d) " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#else
#define FILE_LOG_ERROR(fmt, ...)			log_error("(%s:%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define FILE_LOG_INFO(fmt, ...)				log_info("(%s:%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define FILE_LOG_DEBUG(fmt, ...)			log_debug("(%s:%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

	int log_getlevel();
	void log_setlevel(int level);

	const char* log_getfile();
	void log_setfile(const char* file);

	void log_debug(const char* format, ...);
	void log_warning(const char* format, ...);
	void log_info(const char* format, ...);
	void log_error(const char* format, ...);

	void log_log(int level, const char* format, ...);
	void log_log_va(int level, const char* format, va_list args);

	void log_flush();

#ifdef __cplusplus
}
#endif
#endif /* !_log_h_ */
