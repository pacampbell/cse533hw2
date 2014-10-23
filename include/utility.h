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
	unsigned short port;
	unsigned int win_size;      /* Maximum window size (in datagram units) */
    /* additional config for the client */
    struct in_addr serv_addr;   /* the ipv4 server address        */
    char filename[BUFFER_SIZE]; /* the filename to be transferred */
    unsigned int seed;          /* Random generator seed value    */
    double loss;                /* Probability of datagram loss   */
    unsigned int mean;          /* Mean of exp. dist. in millisec */
} Config;

/**
 * Attempt to open the configuration file and parse it.
 * @param path Path to the config file
 * @param config
 * @return Returns true if the file was parsed successfully, else false.
 */
bool parseConfig(char *path, Config *config);

/**
 * Attempt to open the client configuration file and parse it.
 * @param path Path to the client's config file
 * @param config
 * @return Returns true if the file was parsed successfully, else false.
 */
bool parseClientConfig(char *path, Config *config);

/**
 * Binds a UDP socket to the provided port.
 * @param port Port number to bind server socket to.
 * @return Returns the fd for the socket if creation was a success, 
 * else SERVER_SOCKET_BIND_FAIL is returned.
 */
int createServer(unsigned int port);

#endif
