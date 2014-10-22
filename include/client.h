#ifndef CLIENT_H
#define CLIENT_H
// stdlib headers 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// system headers
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
// Program headers
#include "utility.h"
#include "debug.h"

int handshake(Config *config);

int run(int conn_fd, Config *config);

#endif
