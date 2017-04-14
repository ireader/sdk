#ifndef _dlog_h_
#define _dlog_h_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 写内存日志
int dlog_log(int level, const char* format, ...);
int dlog_log_va(int level, const char* format, va_list args);

/// 设置日志模块名
int dlog_setmodule(const char* module);

/// 设置日志管道文件全路径名称
/// 注意: 
///		1. 仅Linux有效
///		2. 必须在dlog_log调用前设置
///@param[in] name 日志管道文件名
int dlog_setpath(const char* name);

#ifdef __cplusplus
}
#endif
#endif
