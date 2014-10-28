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
// Timeout mechanisms
#include <signal.h>
#include <setjmp.h>
// Program headers
#include "utility.h"
#include "stcp.h"
#include "interfaces.h"
#include "child_process.h"
#include "unpifiplus.h"
#include "debug.h"


#define SERVER_BUFFER_SIZE 2048
#define MAX_HANDSHAKE_ATTEMPTS 3

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
static void sigchld_handler(int signum, siginfo_t *siginfo, void *context);

/**
 * Handles the timout from sigalarm
 */
static void sigalrm_timeout(int signum, siginfo_t *siginfo, void *context);

/**
 * Sets the timeout for sigalrm.
 */
static void set_timeout();

/**
 * Clears the alarm timer and sets back
 * the original alarm function.
 */ 
static void clear_timeout();

/* Packet helper functions */

/**
 * Checks to see if the pkt received is a SYN packet.
 * @return Returns true if a SYN packet, else false.
 */
bool server_valid_syn(int size, struct stcp_pkt *pkt);

/**
 * Checks to see if the pkt received is an ACK packet.
 * @return Returns true if an ACK packet, else false.
 */
bool server_valid_ack(int size, struct stcp_pkt *pkt);

/**
 * Generic function to transmit server payloads on a non-connected udp socket.
 * @return Returns the number of bytes transmitted for error checking.
 */
int server_transmit_payload1(int socket, int seq, int ack, 
	struct stcp_pkt *pkt, Process *process, int flags, void *data, 
	int datalen, struct sockaddr_in client);

/**
 * Generic function to transmit server payloads to a connected udp socket.
 * @return Returns the number of bytes transmitted for checking.
 */
int server_transmit_payload2(int socket, int seq, int ack, 
	struct stcp_pkt *pkt, Process *process, int flags, void *data, 
	int datalen);

#endif
