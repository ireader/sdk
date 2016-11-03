#ifndef _todo_h_
#define _todo_h_

// #pragma TODO(something)

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "("__STR1__(__LINE__)"): "

#if _MSC_VER
	#define TODO(msg) message(__LOC__ "TODO: " #msg)
	#define FIXME(msg) message(__LOC__ "FIXME: " #msg)
#elif __GNUC__
	#define TODO(msg) message(__LOC__ "TODO: " #msg)
	#define FIXME(msg) message(__LOC__ "FIXME: " #msg)
#else // XCODE
	#define TODO(msg)	TODO msg
	#define FIXME(msg)	FIXME msg
#endif

#endif /* !_todo_h_ */
