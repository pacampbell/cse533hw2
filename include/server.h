#ifndef SERVER_H
#define SERVER_H
// stdlib headers
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
// system headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// Program headers
#include "utility.h"
#include "stcp.h"
#include "interfaces.h"
#include "unpifiplus.h"
#include "debug.h"


#define SERVER_BUFFER_SIZE 2048

/**
 * Starts the main loop of the server.
 * @param interfaces List of network interfaces to use.
 * @param server_fd fd of the server socket.
 * @param config Config information passed in from file.
 */
void run(Interface *interfaces, Config *config);

/**
 * Discovers unicast interfaces on the system.
 * @param config Contains configuration info from file.
 * @return Returns all the unicast interfaces on the system.
 */
Interface*  discoverInterfaces(Config *config);

#endif
