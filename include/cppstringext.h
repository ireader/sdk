#ifndef _cppstringext_h_
#define _cppstringext_h_

#include <vector>
#include <string>
#include <assert.h>
#include <string.h>

static const std::string sc_emptystring;
static const std::wstring sc_emptywstring;

//static const std::string sc_testassign("test", size_t(0));

//////////////////////////////////////////////////////////////////////////
///							Functions
//////////////////////////////////////////////////////////////////////////
///	Split(const char* s, char c, std::vector<std::string>& vec)
///	SplitLines(const char* s, std::vector<std::string>& vec)
///	Strip(const char* s, const char* chars=" ")
/// SplitPair(const char* s, char c, std::string& first, std::string sencond, const char* chars=" ")
//////////////////////////////////////////////////////////////////////////


/// Split("abc", 'x') -> ("abc")
/// Split("abc", 'b') -> ("a", "c")
/// Split("abc", 'c') -> ("ab", "")
/// Split("abc", 'a') -> ("", "bc")
/// Split("", 'b') -> ("")
template<typename StdStrContainer>
void Split(const char* s, char c, StdStrContainer& container)
{
	assert(s);
	const char* p = s;
	while(p && *p)
	{
		const char* pc = strchr(p, c);
		if(!pc)
			break;

		container.push_back(std::string(p, pc-p));
		p = pc+1;
	}

	if(p)
		container.push_back(std::string(p));
}

/// Split("abc||def||ghi", "||") -> ("abc", "def", "ghi")
/// Split("abc||", "||") -> ("abc", "")
/// Split("||abc||", "||") -> ("", "abc", "")
/// Split("", "||") -> ("")
template<typename StdStrContainer>
void Split(const char* s, const char* substring, StdStrContainer& container)
{
	assert(s && substring);
	size_t n = strlen(substring);
	
	const char* p = s;
	while(p && *p)
	{
		const char* psubs = strstr(p, substring);
		if(!psubs)
			break;

		container.push_back(std::string(p, psubs-p));
		p = psubs+n;
	}

	if(p)
		container.push_back(std::string(p));
}

/// SplitLines("abc\r\ndef\nghi") -> ("abc", "def", "ghi")
/// SplitLines("abc\n") -> ("abc", "")
/// SplitLines("") -> ("")
template<typename StdStrContainer>
void SplitLines(const char* s, StdStrContainer& container)
{
	assert(s);
	const char* p = s;
	while(p && *p)
	{
		const char* pc = strchr(p, '\n');
		if(!pc)
			break;

		if(pc!=p && *(pc-1)=='\r')
		{
			// filter \r
			container.push_back(std::string(p, pc-1-p));
		}
		else
		{
			container.push_back(std::string(p, pc-p));
		}
		p = pc+1;
	}

	if(p)
		container.push_back(std::string(p));
}


/// SplitP("abc;ndef;ghi") -> ("abc", "def", "ghi")
/// SplitP("abc;ndef,ghi") -> ("abc", "def", "ghi")
/// SplitP("abc;,ndef") -> ("abc", "", "def")
/// SplitP("abc;") -> ("abc", "")
/// SplitP("") -> ("")
template<typename StdStrContainer>
void SplitP(const char* s, const char* pattern, StdStrContainer& container)
{
	const char* p = s;
	while(s && *s)
	{
		if(strchr(pattern, *s))
		{
			container.push_back(std::string(p, s-p));
			p = s+1;
		}
		++s;
	}

	// the remain
	if(p)
		container.push_back(p);
}

/// Return a copy of the string with the leading and trailing characters removed.
/// Strip("   spacious   ") -> "spacious"
/// Strip("www.example.com", "cmowz.") -> "example"
inline std::string Strip(const char* s, const char* chars=" ")
{
	assert(s && chars);
	
	// strip head
	const char* p = s;
	while(*p && strchr(chars, *p))
		++p;

	// remain length
	size_t n = strlen(p);
	if(n < 1)
		return sc_emptystring;

	// strip tail
	const char* pe = p+n-1;
	while(pe>p && strchr(chars, *pe))
		--pe;

	return std::string(p, pe-p+1);
}

/// Return a copy of the string with the leading and trailing characters removed.
/// SplitPair("name=value", '=', " ") -> ("name", "value")
inline bool SplitPair(const char* s, char c, std::string& first, std::string& sencond, const char* chars=" ")
{
	assert(s && chars);
	const char* pc = strchr(s, c);
	if(!pc)
		return false;

	std::string head(s, pc-s);
	first = Strip(head.c_str(), chars);
	sencond = Strip(pc+1, chars);
	return true;
}

template<typename StdStrContainer>
inline std::string Join(const StdStrContainer& container, char c)
{
	std::string s;
	typename StdStrContainer::const_iterator it;
	for(it = container.begin(); it != container.end(); ++it)
	{
		if(!s.empty())
			s += c;
		s += *it;
	}
	return s;
}

template<typename StdStrContainer>
inline std::string Join(const StdStrContainer& container, const char* sep)
{
	std::string s;
	typename StdStrContainer::const_iterator it;
	for(it = container.begin(); it != container.end(); ++it)
	{
		if(!s.empty())
			s += sep;
		s += *it;
	}
	return s;
}

#endif /* !_cppstringext_h_ */
