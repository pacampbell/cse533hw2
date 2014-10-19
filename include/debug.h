#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>

#ifdef DEBUG
	#define debug(S, ...) do{fprintf(stderr, "DEBUG: %s:%d " S, __FILE__, __LINE__, ##__VA_ARGS__);} while(0)
#else
	#define debug(S, ...)
#endif

#endif
