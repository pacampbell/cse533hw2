#ifndef UTILITY_H
#define UTILITY_H
// stdlib headers
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
// system headers
#include <sys/socket.h>
#include <arpa/inet.h>
// project headers
#include "debug.h"
// Utility constants
#define BUFFER_SIZE 256
#define SERVER_SOCKET_BIND_FAIL -1
// Utility structs
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

/**
 * Binds a UDP socket to the provided port.
 * @param port Port number to bind server socket to.
 * @return Returns the fd for the socket if creation was a success, 
 * else SERVER_SOCKET_BIND_FAIL is returned.
 */
int createServer(unsigned int port);

#endif
