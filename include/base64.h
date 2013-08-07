#ifndef _base64_h_
#define _base64_h_

#ifndef IN
	#define IN 
#endif

#ifndef OUT
	#define OUT
#endif

#ifndef INOUT
	#define INOUT
#endif

#ifndef CONST
	#ifdef  __cplusplus
		#define CONST const
	#else
		#define CONST
	#endif
#endif

#ifdef  __cplusplus
extern "C" {
#endif


char* base64_encode(IN CONST unsigned char * bufin, IN unsigned int nbytes);

unsigned char* base64_decode(IN CONST char * bufcoded, IN unsigned int nbytes);

void base64_free(void* p);


#ifdef  __cplusplus
} // extern "C"
#endif

#endif /* !_base64_h_ */
