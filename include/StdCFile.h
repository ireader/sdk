#ifndef _StdCFile_h_
#define _StdCFile_h_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

class StdCFile
{
	StdCFile(const StdCFile&){}
	StdCFile& operator =(const StdCFile&){ return *this; }

public:
	StdCFile(const char* filename, const char* mode)
	{	m_fp = fopen(filename, mode); }

	~StdCFile()
	{	if(m_fp) fclose(m_fp); }

	bool IsOpened() const
	{	return NULL != m_fp;	}

	operator FILE*()
	{	return m_fp;	}

public:
	int Read(void* data, int bytes)
	{	return fread(data, 1, bytes, m_fp);	}

	int Write(const void* data, int bytes)
	{	return fwrite(data, 1, bytes, m_fp);	}

	int Flush()
	{	return fflush(m_fp); }

	int Seek(long offset)
	{	return fseek(m_fp, offset, SEEK_SET); }

	long GetFileSize() const
	{
		long c = ftell(m_fp);
		fseek(m_fp, 0, SEEK_END);
		long size = ftell(m_fp);
		fseek(m_fp, c, SEEK_SET);
		return size;
	}

	// 0 - no error
	int GetError()
	{	if(m_fp) return ferror(m_fp); return errno;	}

public:
	// need free!!!
	void* Read(size_t size=0)
	{
		if(0 == size)
			size = GetFileSize();
		void* p = malloc(size+1);
		if(NULL == p)
			return NULL;

		size_t n = fread(p, 1, size, m_fp);
		if(n < size && 0==feof(m_fp))
		{
			free(p);
			return NULL;
		}
		((char*)p)[size] = 0; // for c-string null-character
		return p;
	}

private:
	FILE* m_fp;
};

#endif /* !_StdCFile_h_ */
