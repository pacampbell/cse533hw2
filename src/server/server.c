#include "server.h"

static Process *processes = NULL;
static sigjmp_buf env;

int main(int argc, char *argv[]) {
	char *path = "server.in";
	Config config;
	debug("Begin parsing %s\n", path);
	/* Attempt to parse the config */
	if(parseServerConfig(path, &config)) {
		Interface *interfaces = NULL;
		debug("Port: %u\n", config.port);
		debug("Window Size: %u\n", config.win_size);
		// Get a list interfaces
		interfaces = discoverInterfaces(&config, BIND_INTERFACE);
		if(size(interfaces)) {
			/* Setup some basic signals */
			struct sigaction sigac_child;
			struct sigaction sigac_alarm;
			sigset_t child_handler_mask;
			sigset_t alarm_handler_mask;

			sigemptyset (&child_handler_mask);
			sigemptyset (&alarm_handler_mask);
			/* Block other terminal-generated signals while handler runs. */
			sigaddset (&child_handler_mask, SIGALRM);
			sigaddset (&alarm_handler_mask, SIGCHLD);
			/* Zero out memory */
			memset(&sigac_child, '\0', sizeof(sigac_child));
			memset(&sigac_alarm, '\0', sizeof(sigac_alarm));
			/* Set values */
			sigac_child.sa_sigaction = &sigchld_handler;
			sigac_child.sa_mask = child_handler_mask;
			sigac_child.sa_flags = SA_SIGINFO;
			sigac_alarm.sa_sigaction = &sigalrm_timeout;
			sigac_alarm.sa_mask = alarm_handler_mask;
			sigac_alarm.sa_flags = SA_SIGINFO;
			/* Set the sigactions */
			if(sigaction(SIGCHLD, &sigac_child, NULL) < 0) {
				error("Unable to set SIGCHLD sigaction.\n");
				goto clean_up;
			}
			if(sigaction(SIGALRM, &sigac_alarm, NULL)) {
				error("Unable to set SIGALRM sigaction.\n");
				goto clean_up;
			}
			/* Start the servers main loop */
			run(interfaces, &config);
		} else {
			warn("No interfaces were bound to. Aborting program.\n");
		}
clean_up:
		/* Clean up memory */
		debug("Freeing interfaces list - %d.\n", (int)getpid());
		destroy_interfaces(&interfaces);
		/* Free up any process information that is left over */
		debug("Freeing processes list - %d.\n", (int)getpid());
		destroy_processes(&processes);
	} else {
		/* The config parsing failed */
		debug("Failed to parse: %s\n", path);
	}
	return EXIT_SUCCESS;
}

void run(Interface *interfaces, Config *config) {
	fd_set rset;
	Interface *node;
	Process *process = NULL;
	int largest_fd = 0;
	bool running = true;

	// Packet stuff that should probably be in child.
	int valid_pkt;
	struct sockaddr_in connection_addr;
    socklen_t connection_len = sizeof(connection_addr);

	debug("Server waiting on port %u\n", config->port);
	while(running) {
		FD_ZERO(&rset);
		node = interfaces;
		// Set the select set
		while(node != NULL) {
			FD_SET(node->sockfd, &rset);
			largest_fd = largest_fd < node->sockfd ? node->sockfd : largest_fd;
			// Get the next node in the list
			node = node->next;
		}
		// Set on FD's
		if(select(largest_fd + 1, &rset, NULL, NULL, NULL) < 0) {
            if(errno != EINTR) {
                error("Fatal Error on select: %s\n", strerror(errno));
                break;
            }
        }
		// Check to see if any are set
		node = interfaces;
		while(node != NULL) {
			if(FD_ISSET(node->sockfd, &rset)) {
				struct stcp_pkt pkt;
				debug("Detected connection on interface: <%s> %s\n", node->name, node->ip_address);
				valid_pkt = recvfrom_pkt(node->sockfd, &pkt, 0, (struct sockaddr *)&connection_addr, &connection_len);
				if(valid_pkt < 0) {
					if(errno == EAGAIN || errno == EWOULDBLOCK) {
						/* False alarm, there was nothing to recv */
						debug("Socket would have blocked. Skipping socket\n");
						node = node->next;
						continue;
					} else {
						error("Error on recvfrom: %s\n", strerror(errno));
						node = node->next;
						continue;
					}
				}
				if(server_valid_syn(valid_pkt, &pkt)) {
					sigset_t sigchld_mask;

					sigemptyset(&sigchld_mask);
					sigaddset(&sigchld_mask, SIGCHLD);
					/* Block SIGCHLD unitl we add the child procces to the list */
					if(sigprocmask(SIG_BLOCK, &sigchld_mask, NULL) < 0){
						error("Error blocking SIGCHLD sigprocmask: %s\n", strerror(errno));
					}
					// Packet was valid
					// Check if process already exists for child
					if((process = get_process(processes, node->ip_address, connection_addr.sin_port)) == NULL) {
						// Process doesn't exist so create a new one
						info("No process exists for %s:%d\n", node->ip_address, connection_addr.sin_port);
						Process *new_process = malloc(sizeof(Process));
						if(new_process != NULL) {
							char buffer[SERVER_BUFFER_SIZE];
							int pid;
							/* Convert client ipaddress to string */
							inet_ntop(AF_INET, &(connection_addr.sin_addr), buffer, INET_ADDRSTRLEN);
							/* Process fields */
							new_process->pid = 0;
							/* Client Fields */
							new_process->port = connection_addr.sin_port;
							strcpy(new_process->ip_address, buffer);
							/* Interface fields */
							new_process->interface_win_size = config->win_size;
							new_process->interface_fd = node->sockfd;
							new_process->interface_port = config->port;
							strcpy(new_process->interface_ip_address, node->ip_address);
							strcpy(new_process->interface_network_mask, node->network_mask);
							/* List fields */
							new_process->next = NULL;
							new_process->prev = NULL;
							// Add the process to the list
							add_process(&processes, new_process);
							// Fork a new child
							pid = spawnchild(interfaces, new_process, &pkt);
							if(pid == -1 || pid == 0) {
								// Either fork failed or we are in the child process
								// Set running to false and break out of this loop
								warn("Failed fork or runaway child process - %d\n", pid);
								running = false;
								break;
							}
						} else {
							error("CRITICAL ERROR: Unable to allocate memory for a new process.\n");
						}
					} else {
						// TODO: The process already exists; What to do?
						info("Process already exists for %s:%d\n", process->ip_address, process->port);
					}
					/* Unblock SIGCHLD In the Parent Server */
					if(sigprocmask(SIG_UNBLOCK, &sigchld_mask, NULL) < 0){
						error("Error unblocking SIGCHLD sigprocmask: %s\n", strerror(errno));
					}
				} else {
					debug("Ignoring invalid SYN pakcet.\n");
				}
			}
			node = node->next;
		}
	}
}

int spawnchild(Interface *interfaces, Process *process, struct stcp_pkt *pkt) {
	int pid;
	Interface *interface = NULL;

	switch(pid = fork()) {
		case -1:
			/* Failed */
			error("Failed to fork child env.\n");
			break;
		case 0:
			/* In child */
			info("Server Child - PID: %d\n", (int)getpid());
			// TODO: Close connection to other interfaces
			// other FDs, etc.
			interface = interfaces;
			while(interface != NULL) {
				if(interface->sockfd != process->interface_fd) {
					if(close(interface->sockfd) != 0) {
						warn("Unable to close interface: <%s> %d\n", interface->name, interface->sockfd);
					} else {
						debug("Closed interface: <%s> %d\n", interface->name, interface->sockfd);
					}
				}
				interface = interface->next;
			}
			childprocess(process, pkt);
			info("Child process has finished; pid = %d\n", (int)getpid());
			/* Clean up memory */
			debug("Freeing interfaces list - %d.\n", (int)getpid());
			destroy_interfaces(&interfaces);
			/* Free up any process information that is left over */
			debug("Freeing processes list - %d.\n", (int)getpid());
			destroy_processes(&processes);
			// Quit the child
			exit(EXIT_SUCCESS);
			break;
		default:
			/* TODO: ADD HERE */
			/* In parent */
			process->pid = pid;
			info("Main Server - Child PID: %d\n", pid);
			break;
	}
	return pid;
}

void childprocess(Process *process, struct stcp_pkt *pkt) {
	char file[1024];
	// Extract file name
	strncpy(file, pkt->data, pkt->dlen);
	file[pkt->dlen] = '\0';
	info("Searching for file: '%s'\n", file);
	// Attempt to open the file
	FILE *fp = fopen(file, "r");
	if(fp != NULL) {
		int sock = -1;
		int read = 0;
		bool samesub = false;
		struct stcp_pkt ack;
		struct sockaddr_in client_addr, server_addr;
		char buffer[STCP_MAX_DATA];
		
		// Zero out memory
		memset((char *)&client_addr, 0, sizeof(client_addr));
		memset((char *)&server_addr, 0, sizeof(server_addr));
		
		// Set client fields
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = process->port;
		inet_aton(process->ip_address, &client_addr.sin_addr);
		
		// Set server fields
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(0);
		inet_aton(process->interface_ip_address, &server_addr.sin_addr);

		// Attach socket depending on if local or not
		samesub = isSameSubnet(process->interface_ip_address, 
							   process->ip_address, 
							   process->interface_network_mask);
		// Create socketfd
		sock = createClientSocket(&client_addr, &server_addr, samesub);

		if(sock >= 0) {
			int len = 0;
			int handshake_attempts = 0;
			do {
				// resend = false;
				if(setjmp(env) != 0) {
					// SYN_ACK HANDSHAKE TIMED OUT
					handshake_attempts++;
					warn("Handshake SYN_ACK timed out on attempt %d\n", handshake_attempts);
					// TODO: Send on new and old
					len = server_transmit_payload1(process->interface_fd, 0, 
						pkt->hdr.seq + 1, pkt, process, STCP_SYN | STCP_ACK, 
						&server_addr.sin_port, sizeof(server_addr.sin_port), 
						client_addr);
					len = server_transmit_payload2(sock, 0, 
						pkt->hdr.seq + 1, pkt, process, STCP_SYN | STCP_ACK, 
						&server_addr.sin_port, sizeof(server_addr.sin_port));
				} else {
					// Send SYN | ACK for new socket
					len = server_transmit_payload1(process->interface_fd, 0, 
						pkt->hdr.seq + 1, pkt, process, STCP_SYN | STCP_ACK, 
						&server_addr.sin_port, sizeof(server_addr.sin_port), 
						client_addr);
					// Log on the serer the error
					if(len < 0) {
						error("Failed to send packet to %s:%d\n", process->ip_address, process->port);
					}
				}
				/* TODO: how to calculate correct timeout values ? */
				//set_timeout(10);
				/* Wait for client's ACK */
				len = recv_pkt(sock, &ack, 0);
				//clear_timeout();
			} while(!server_valid_ack(len, &ack) && handshake_attempts < MAX_HANDSHAKE_ATTEMPTS);
			
			// Check to make sure we didnt have too many handshake attempts
			if(handshake_attempts >= MAX_HANDSHAKE_ATTEMPTS) {
				error("Unable to complete three-way handshake on SYN_ACK step.\n");
				goto clean_up;
			}

			/* Got a good ack packet */
			#ifdef DEBUG
				debug("Received pkt from client ");
				print_hdr(&ack.hdr);
			#endif

			/* Close original interface socket */
			if(close(process->interface_fd) != 0) {
				warn("Failed to close the interface fd: %d\n", processes->interface_fd);
			}

			/* Connection established start sending file */
			while((read = fread(buffer, sizeof(unsigned char), STCP_MAX_DATA, fp)) > 0) {
				debug("Read %d bytes from the file '%s'\n", read, file);
				// Transmit payload to server
				len = server_transmit_payload2(sock, pkt->hdr.seq + 1, 0, pkt,
					process, 0, buffer, read);
				// If the read length and the sent length are not the same
				// Something probably went wrong

//TODO: Fix server_transmit_payload can fail with negative len
//DEBUG: src/server/server.c:server_transmit_payload:352 Sent -1 bytes to the client
//ERROR: src/server/server.c:childprocess:248 Read len = 9, Sent len = -13 (they should match)

				if(read != (len - sizeof(pkt->hdr))) {
					error("Read len = %d, Sent len = %d (they should match)\n",
						read, (len - (int)sizeof(pkt->hdr)));
					break;
				}
			}
			// Send the fin packet
			len = server_transmit_payload2(sock, pkt->hdr.seq + 1, 0, pkt, 
				process, STCP_FIN, buffer, read);
			// TODO: Receive the FIN_ACK from cleint
			recv_pkt(sock, &ack, 0);
		} else {
			error("Failed to create a socket.\n");
		}
clean_up:
		/* Close open socket */
		if(sock >= 0) {
			close(sock);
		}
		/* Close the opened file */
		fclose(fp);
	} else {
		warn("The file '%s' does not exist on the server.\n", file);
	}
}

static void sigchld_handler(int signum, siginfo_t *siginfo, void *context) {
    int pid;
    Process *process = NULL;

    while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		process = get_process_by_pid(processes, pid);
		if(process != NULL) {
			if(remove_process(&processes, process)) {
				info("Successfuly removed the process %d\n", (int)pid);
			}
		} else {
			error("Unable to find process with pid: %d\n", (int)pid);
		}
    }
   	if(pid == -1) {
		if(errno == EINTR) {
			// Got interrupted by another signal
			warn("SIGCHLD handler got interrupted\n");
		} else if(errno != ECHILD){
			// Otherwise something bad happened so break out of loop
			error("Wait failed with error: %s\n", strerror(errno));
		}
	}
}

// static void set_timeout(int nsec) {
// 	if(alarm(nsec) != 0) {
// 		warn("Alarm was already set with sec = %d\n", nsec);
// 	}
// }

// static void clear_timeout() {
// 	alarm(0);
// }

static void sigalrm_timeout(int signum, siginfo_t *siginfo, void *context) {
	siglongjmp(env, 1);
}

bool server_valid_syn(int size, struct stcp_pkt *pkt) {
	bool valid = false;
	if(pkt != NULL) {
		if(size < 0 && errno != EINTR) {
			/* Error on recv system call */
			error("recvfrom: %s\n", strerror(errno));
			//TODO: What do here? exit failure? ignore?
		} else if(size == 0) {
			debug("Ignoring message: too small to be STCP packet.\n");
		} else if(pkt->hdr.flags & STCP_SYN) {
			valid = true;
		} else {
			warn("Invalid SYN packet on handshake initialization.. Dropping packet.\n");
		}
	}
	return valid;
}

bool server_valid_ack(int size, struct stcp_pkt *pkt) {
	bool valid = false;
	if(pkt != NULL) {
		if(size < 0 && errno != EINTR) {
			/* Error on recv system call */
			error("recvfrom: %s\n", strerror(errno));
			//TODO: What do here? exit failure? ignore?
			} else if(size == 0) {
			debug("Ignoring message: too small to be STCP packet.\n");
		} else if(pkt->hdr.flags & STCP_ACK) {
			valid = true;
		} else {
			warn("Invalid ACK packet.. Dropping packet.\n");
		}
	}
	return valid;
}

int server_transmit_payload1(int socket, int seq, int ack, struct stcp_pkt *pkt,
							Process *process, int flags, void *data, int datalen,
							struct sockaddr_in client) {
	int bytes = 0;
	// Set up packet data
	build_pkt(pkt, seq, ack, process->interface_win_size, flags, data, datalen);
	#ifdef DEBUG
		debug("Sending pkt: ");
		print_hdr(&pkt->hdr);
	#endif
	// Send the packet and see what happens
	bytes = sendto_pkt(socket, pkt, 0, (struct sockaddr*)&client, sizeof(client));
	debug("Sent %d bytes to the client\n", bytes);
	return bytes;
}

int server_transmit_payload2(int socket, int seq, int ack, struct stcp_pkt *pkt,
							Process *process, int flags, void *data, int datalen) {
	int bytes = 0;
	// Set up packet data
	build_pkt(pkt, seq, ack, process->interface_win_size, flags, data, datalen);
	#ifdef DEBUG
		debug("Sending pkt: ");
		print_hdr(&pkt->hdr);
	#endif
	// Send the packet and see what happens
	bytes = send_pkt(socket, pkt, 0);
	debug("Sent %d bytes to the client\n", bytes);
	return bytes;
}
