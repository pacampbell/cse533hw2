#ifndef CLIENT_H
#define CLIENT_H
// stdlib headers
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
// system headers
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
// Program headers
#include "utility.h"
#include "debug.h"

/**
 * Determine the IP to identify the server and the IP to identify the client.
 * If the address is on the same host then both IP's are set to 127.0.0.1
 * Otherwise server_ip is set to the config value and we check if one of our
 * interfaces is local. client_ip is set to the longest prefix matching IP.
 * If non are local we choose and arbitrary address for the client.
 *
 * @param config     The config from client.in
 * @param server_ip  The IP of the server
 * @param client_ip  The IP of the client
 * @return Returns true if the address is local
 */
bool chooseIPs(Config *config, struct in_addr *server_ip, struct in_addr *client_ip);


int createSocket(struct sockaddr_in *serv_addr, struct sockaddr_in *client_addr, bool local);

int handshake(Config *config, int sockfd);

int run(int conn_fd, Config *config);

#endif
