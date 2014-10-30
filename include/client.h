#ifndef CLIENT_H
#define CLIENT_H
// stdlib headers
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
// system headers
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
// Program headers
#include "stcp.h"
#include "interfaces.h"
#include "unpifiplus.h"
#include "utility.h"
#include "debug.h"

struct consumer_args {
	struct stcp_sock *stcp;
	unsigned int seed;
	unsigned int mean;
};

/**
 * Determine the IP to identify the server and the IP to identify the client.
 * If the address is on the same host then both IP's are set to 127.0.0.1
 * Otherwise server_ip is set to the config value and we check if one of our
 * interfaces is local. client_ip is set to the longest prefix matching IP.
 * If none are local we choose any address from the client's interface list.
 *
 * @param config     The config from client.in
 * @param server_ip  The IP of the server
 * @param client_ip  The IP of the client
 * @return Returns true if the address is local
 */
bool chooseIPs(Config *config, struct in_addr *server_ip, struct in_addr *client_ip);

/**
 * Start producer behavior
 */
int runProducer(struct stcp_sock *sock);

/**
 * Start consumer behavior, this is the start routine in a pthread
 *
 * @param args A struct containing the seed, mean, and reference to stcp struct
 */
void *runConsumer(void *args);

int bitsSet(unsigned long mask);

#endif
