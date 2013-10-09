#ifndef _unicode_h_
#define _unicode_h_

#include <wchar.h>

#ifndef IN
#define IN 
#endif

#ifndef OUT
#define OUT
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/// Unicode字符串转换成UTF-8字符串
/// @param[in] src Unicode字符串
/// @param[in] srcLen Unicode字符串长度(length=wcslen(src)), srcLen为0时转换整个字符串(src必须以字符'0'结尾)
/// @param[out] tgt utf8字符串缓冲区
/// @param[in] tgtBytes in-缓冲区长度, 单位字节
/// @return 转换后字符串长度
int unicode_to_utf8(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes);

/// UTF-8字符串转换成Unicode字符串
/// @param[in] src utf-8字符串
/// @param[in] srcLen utf-8字符串长度(length=wcslen(src)), srcLen为0时转换整个字符串(src必须以字符'0'结尾)
/// @param[out] tgt unicode字符串缓冲区
/// @param[in] tgtBytes in-缓冲区长度, 单位字节
/// @return 转换后字符串长度
int unicode_from_utf8(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes);

/// Unicode字符串转换成多字节字符串(Windows平台是Unicode UTF-16)
/// @param[in] src Unicode字符串
/// @param[in] srcLen 字符串长度, n为0时转换整个字符串(此时src必须以字符'0'结尾)
/// @param[out] tgt 多字节字符串缓冲区
/// @param[in] tgtBytes 多字节字符串缓冲区长度, 单位: 字节
/// @return 转换后字符串长度
int unicode_to_mbcs(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes);

/// 多字节字符串转换成Unicode字符串(Windows平台是Unicode UTF-16)
/// @param[in] src 多字节字符串
/// @param[in] srcLen 字符串长度, n为0时转换整个字符串(此时src必须以字符'0'结尾)
/// @param[out] tgt Unicode字符串缓冲区
/// @param[in] tgtBytes Unicode字符串缓冲区长度, 单位: 字节
/// @return 转换后字符串长度
int unicode_from_mbcs(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes);

/// 多字节字符串转换成Unicode字符串(Windows平台是Unicode UTF-16)
/// @param[in] charset 多字节字符串编码类型
/// @param[in] src 多字节字符串
/// @param[in] srcLen 字符串长度, n为0时转换整个字符串(此时src必须以字符'0'结尾)
/// @param[out] tgt Unicode字符串缓冲区
/// @param[in] tgtBytes Unicode字符串缓冲区长度, 单位: 字节
/// @return
//int unicode_encode(IN const char* charset, IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes);

/// Unicode字符串转换成多字节字符串(Windows平台是Unicode UTF-16)
/// @param[in] charset 多字节字符串编码类型
/// @param[in] src Unicode字符串
/// @param[in] srcLen 字符串长度, n为0时转换整个字符串(此时src必须以字符'0'结尾)
/// @param[out] tgt 多字节字符串缓冲区
/// @param[in] tgtBytes 多字节字符串缓冲区长度, 单位: 字节
/// @return
//int unicode_decode(IN const char* charset, IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes);

#define unicode_to_gb2312 unicode_to_gb18030
#define unicode_from_gb2312 unicode_from_gb18030
#define unicode_to_gbk unicode_to_gb18030
#define unicode_from_gbk unicode_from_gb18030

int unicode_to_gb18030(IN const wchar_t* src, IN size_t srcLen, OUT char* tgt, IN size_t tgtBytes);
int unicode_from_gb18030(IN const char* src, IN size_t srcLen, OUT wchar_t* tgt, IN size_t tgtBytes);

#ifdef  __cplusplus
} // extern "C" 
#endif

#endif /* _unicode_h_ */
