#ifndef SERVER_H
#define SERVER_H
// stdlib headers
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
// system headers
#include <sys/socket.h>
#include <sys/select.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
// Program headers
#include "utility.h"
#include "stcp.h"
#include "interfaces.h"
#include "child_process.h"
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
 * Forks a new child process.
 * @param interfaces List of all interfaces on this node.
 * @param process Process that is about to be spawned (need to populate pid field).
 * @param pkt The initial packet from the handshake.
 * @return returns the pid from the fork() call.
 */
int spawnchild(Interface *interfaces, Process *process, struct stcp_pkt *pkt);

/**
 * All code that the child process must execute is handled here.
 * @param the process that has information about all interfaces in use.
 * @param pkt The initial packet from the handshake.
 */
void childprocess(Process *process, struct stcp_pkt *pkt);

/**
 * Handles SIG_CHILD from forked processes. When this signal is
 * received the process from the Processes list will be removed.
 */
void sigchld_handler(int signum);

#endif
