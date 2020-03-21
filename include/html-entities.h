#ifndef _html_entities_h_
#define _html_entities_h_

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Get HTML entities count
/// @return html entities count
int html_entities_count(void);

/// Get HTML entities
/// @param[in] index from 0 to count-1
/// @param[out] name entities name
/// @param[out] number entities number
void html_entities_get(int index, char name[16], wchar_t *number);

/// decode HTML(UTF-8) content("&lt;" -> "<")
/// @param[out] dst target string buffer, dst length must > src length
/// @param[in] src source string
/// @param[in] srcLen source string length(in bytes)
/// @return >=0-destination string length, <0-error
int html_entities_decode(char* dst, const char* src, int srcLen);

/// encode HTML(UTF-8) content("<" -> "&lt;")
/// @param[out] dst target string buffer, dst length must enough
/// @param[in] src source string
/// @param[in] srcLen source string length(in bytes)
/// @return >=0-destination string length, <0-error
int html_entities_encode(char* dst, const char* src, int srcLen);

#ifdef __cplusplus
}
#endif

#endif /* !_html_entities_h_ */
