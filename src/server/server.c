#include "server.h"

static Process *processes = NULL;
static void *orig_alarm = NULL;
static volatile sig_atomic_t resend = false; 
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
			/* Start the servers main loop */
			run(interfaces, &config);
		} else {
			warn("No interfaces were bound to. Aborting program.\n");
		}
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
		select(largest_fd + 1, &rset, NULL, NULL, NULL);
		// Check to see if any are set
		node = interfaces;
		while(node != NULL) {
			if(FD_ISSET(node->sockfd, &rset)) {
				struct stcp_pkt pkt;
				debug("Detected connection on interface: <%s> %s\n", node->name, node->ip_address);
				valid_pkt = recvfrom_pkt(node->sockfd, &pkt, 0, (struct sockaddr *)&connection_addr, &connection_len);
				if(server_valid_syn(valid_pkt, &pkt)) {
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
								info("Child process has finished; pid = %d\n", (int)getpid());
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
				}
			}
			node = node->next;
		}
	}
}

int spawnchild(Interface *interfaces, Process *process, struct stcp_pkt *pkt) {
	// TODO: Add signal handler for sigchild, to remove process from process list
	int pid;
	Interface *interface = NULL;
	signal(SIGCHLD, sigchld_handler);
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
			break;
		default:
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
				// Send SYN | ACK for new socket
				len = server_transmit_payload(process->interface_fd, 0, 
					pkt->hdr.seq + 1, pkt, process, STCP_SYN | STCP_ACK, 
					&server_addr.sin_port, sizeof(server_addr.sin_port), 
					client_addr);
				// Log on the serer the error
				if(len < 0) {
					error("Failed to send packet to %s:%d\n", process->ip_address, process->port);
				}
				set_timeout();
				// resend = false;
				setjmp(env);
				/* Wait for client's ACK */
				len = recv_pkt(sock, &ack, 0);
				if(resend) {
					resend = false;
					handshake_attempts++;
				}
				clear_timeout();

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
				len = server_transmit_payload(sock, pkt->hdr.seq + 1, 0, pkt,
					process, 0, buffer, read, client_addr);
				// If the read length and the sent length are not the same
				// Something probably went wrong
				if(read != (len - sizeof(pkt->hdr))) {
					error("Read len = %d, Sent len = %d (they should match)\n",
						read, (len - (int)sizeof(pkt->hdr)));
					break;
				}
			}
			// Send the fin packet
			len = server_transmit_payload(sock, pkt->hdr.seq + 1, 0, pkt, 
				process, STCP_FIN, buffer, read, client_addr);
			// TODO: Receive the FIN_ACK from cleint
			sleep(1);
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

static void sigchld_handler(int signum) {
    int pid;
    Process *process = NULL;
    while ((pid = waitpid(-1, NULL, WNOHANG)) != -1) {
        process = get_process_by_pid(processes, pid);
        if(process != NULL) {
        	if(remove_process(&processes, process)) {
        		info("Successfuly removed the process %d\n", (int)pid);
        	}
        } else {
        	warn("Unable to find process with pid: %d\n", (int)pid);
        }
    }
}

static void set_timeout(int nsec) {
	orig_alarm = signal(SIGCHLD, sigalrm_timeout);
	if(alarm(nsec) != 0) {
		warn("Alarm was already set with nsec = %d\n", nsec);
	}
}

static void clear_timeout() {
	alarm(0);
	signal(SIGCHLD, orig_alarm);
}

static void sigalrm_timeout(int signum) {
	resend = true;
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

int server_transmit_payload(int socket, int seq, int ack, struct stcp_pkt *pkt,
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
