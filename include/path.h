#ifndef _path_h_
#define _path_h_

#include "sys/path.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <string.h>

namespace path
{

#define FILE_PATH_SEPARATOR_WINDOWS	'\\'
#define FILE_PATH_SEPARATOR_LINUX	'/'
#define FILE_PATH_SEPARATOR_MAC		':'

#if defined(WIN32) || defined(WINCE)
	#define FILE_PATH_SEPARATOR	FILE_PATH_SEPARATOR_WINDOWS
#elif defined(__MAC__)
	#define FILE_PATH_SEPARATOR	FILE_PATH_SEPARATOR_MAC
#else
	#define FILE_PATH_SEPARATOR	FILE_PATH_SEPARATOR_LINUX
#endif


//static const char* FILE_PATH_SEPARATORS_A = "\\/";
//static const wchar_t* FILE_PATH_SEPARATORS_W = L"\\/";
//
//#if defined(UNICODE) || defined(_UNICODE)
//	#define FILE_PATH_SEPARATORS FILE_PATH_SEPARATORS_W
//#else
//	#define FILE_PATH_SEPARATORS FILE_PATH_SEPARATORS_A
//#endif

inline bool isdrive(const char* path)
{
#if defined(WIN32) || defined(WINCE)
	// C:\abc\def
	return path && isalpha((unsigned char)*path) && ':'==path[1];
#else
	// /abc/def
	return path && '/'==*path;
#endif
}

inline bool ishost(const char* path)
{
	// \\host\abc\def
	return path && '\\'==*path && '\\'==path[1];
}

#if defined(WIN32) || defined(WINCE)
inline void winnormalize(std::string& path)
{
	// replace '/' with '\\'
	std::string::size_type n = path.find('/');
	while(n != std::string::npos)
	{
		path.replace(n, 1, 1, '\\');
		n = path.find('/', n+1);
	}
}
#endif

inline std::string join(const char* path1, const char* path2)
{
	assert(path1 && path2);
	std::string path(path1);

	if(path2 && isdrive(path2)) 
		return path2;

	if(path2 && 0 != *path2)
	{
		if(path.length() > 0 
			&& NULL==strchr("/\\", *path.rbegin()))
			path += FILE_PATH_SEPARATOR;
		path += path2;
	}

#if defined(WIN32) || defined(WINCE)
	winnormalize(path);
#endif
	return path;
}

inline std::string join_va(const char* path1, ...)
{
	std::string path(path1);

	va_list va;
	va_start(va, path1);
	const char* p = va_arg(va, const char*);
	while(NULL != p)
	{
		path = join(path.c_str(), p);
		p = va_arg(va, const char*);
	}
	va_end(va);

#if defined(WIN32) || defined(WINCE)
	winnormalize(path);
#endif
	return path;
}

template<typename StdStrContainer>
inline int split(const char* path, StdStrContainer& container)
{
	assert(path);

#if defined(WIN32) || defined(WINCE)
	// C:\abc\def
	// \\host\abc\def
	// file:///C:/abc/def

	if(path && isdrive(path))
	{
		if(0!=path[2] && '\\'!=path[2] && '/'!=path[2])
			return -1;

		container.push_back(std::string(path, 2));
		path += 2;
	}
	else if(path && 0 == strncmp("\\\\", path, 2))
	{
		if('\\'==path[2] || '/'==path[2])
			return -1;

		container.push_back(std::string(path, 2));
		path += 2;
	}
	else if(path && 
		(0==strncmp("file:///", path, 8) 
		|| 0==strncmp("file:\\\\\\", path, 8)))
	{
		if('\\'==path[8] || '/'==path[8])
			return -1;

		container.push_back(std::string(path, 8));
		path += 8;
	}
	
	// ignore the first '\\', '/' characters
	while(path && ('\\'==*path || '/'==*path)) ++path;

	const char* p = path;
	while(p && *p)
	{
		if('\\'==*p || '/'==*p)
		{
			container.push_back(std::string(path, p-path));
			path = p+1;
		}
		++p;
	}

#else // linux

	// /abc/def -> (/, abc, def)
	// abc/def -> (abc, def)
	if(isdrive(path))
	{
		container.push_back(std::string(path, 1)); // root
		++path;
	}
	
	const char* p = path;
	while(*p)
	{
		if('/'==*p)
		{
			if(p > path)
				container.push_back(std::string(path, p-path));
			path = p+1;
		}
		++p;
	}

#endif	

	if(path && *path)
		container.push_back(std::string(path));
	return 0;
}

inline int makedirs(const char* path)
{
	assert(path && *path);
	std::vector<std::string> paths;
	split(path, paths);

	std::string pathname;
	for(size_t i=0; i<paths.size(); ++i)
	{
		pathname = join(pathname.c_str(), paths[i].c_str());
		if(0 == path_testdir(pathname.c_str()))
		{
			int r = path_makedir(pathname.c_str());
			if(0 != r)
				return r;
		}
	}
	return 0;
}

} // end namespace

#endif /* !_path_h_ */
