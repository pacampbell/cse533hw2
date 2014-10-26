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
	#define debug(S, ...) do{fprintf(stderr, KBLU "DEBUG: %s:%s:%d " S KNRM, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);} while(0)
	#define error(S, ...) do{fprintf(stderr, KRED "ERROR: %s:%s:%d " S KNRM, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);} while(0)
	#define warn(S, ...) do{fprintf(stderr, KYEL "WARN: %s:%s:%d " S KNRM, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);} while(0)
	#define info(S, ...) do{fprintf(stderr, KCYN "INFO: %s:%s:%d " S KNRM, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);} while(0)
#else
	#define debug(S, ...)
	#define error(S, ...) do{fprintf(stderr, KRED "ERROR: " S KNRM, ##__VA_ARGS__);} while(0)
	#define warn(S, ...) do{fprintf(stderr, KYEL "WARN: " S KNRM, ##__VA_ARGS__);} while(0)
	#define info(S, ...) do{fprintf(stderr, KCYN "INFO: " S KNRM, ##__VA_ARGS__);} while(0)
#endif

#endif
