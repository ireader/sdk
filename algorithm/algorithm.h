#ifndef _alogrithm_h_
#define _alogrithm_h_

#ifdef  __cplusplus
extern "C" {
#endif

///longest common sequence
///@param[in] s1 string source
///@param[in] s2 string source
///@param[out] seq result sequence
///@param[in] len seq buffer size(bytes)
///@return <0-error, 0-ok, >0=need more buffer
int lcs(const char* s1, const char* s2, char* seq, int len);

///longest common substring
///@param[in] s1 string source
///@param[in] s2 string source
///@param[out] sub common substring
///@param[in] len substring buffer size(bytes)
///@return <0-error, 0-ok, >0=need more buffer
int strsubstring(const char* s1, const char* s2, char* sub, int len);

///Knuth-Morris-Pratt Algorithm
///@param[in] s string
///@param[in] pattern substring
///@return 0-can't find substring, other-substring pointer
const char* kmp(const char* s, const char* pattern);

#ifdef  __cplusplus
}
#endif

#endif /* !_alogrithm_h_ */
