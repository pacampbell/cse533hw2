#ifndef UTILITY_H
#define UTILITY_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "debug.h"

#define BUFFER_SIZE 256

typedef struct {
	unsigned int port;
	unsigned int mtu;
} Config;

/**
 * Attempts to open the configuration file and
 * parsed it.
 * @param path Path to 
 * @param config
 * @return Returns true if the file was parsed successfully,
 * else false.
 */
bool parseConfig(char *path, Config *config);

#endif
