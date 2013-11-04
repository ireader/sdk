#ifndef _libsys_h_
#define _libsys_h_

#include "dllexport.h"

#if defined(LIBSYS_EXPORTS)
	#define LIBSYS_API DLL_EXPORT_API
#else
	#define LIBSYS_API DLL_IMPORT_API
#endif

#endif /* !_libsys_h_ */
