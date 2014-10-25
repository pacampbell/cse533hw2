#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>

// Colors
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KMAG  "\x1B[1;35m"
#define KCYN  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"

#ifdef DEBUG
	#define debug(S, ...) do{fprintf(stderr, KBLU "DEBUG: %s:%d " S KNRM, __FILE__, __LINE__, ##__VA_ARGS__);} while(0)
	#define error(S, ...) do{fprintf(stderr, KRED "ERROR: %s:%d " S KNRM, __FILE__, __LINE__, ##__VA_ARGS__);} while(0)
	#define warn(S, ...) do{fprintf(stderr, KYEL "WARN: %s:%d " S KNRM, __FILE__, __LINE__, ##__VA_ARGS__);} while(0)
	#define info(S, ...) do{fprintf(stderr, KCYN "INFO: %s:%d " S KNRM, __FILE__, __LINE__, ##__VA_ARGS__);} while(0)
#else
	#define debug(S, ...)
	#define error(S, ...)
	#define warn(S, ...)
	#define info(S, ...)
#endif

#endif
