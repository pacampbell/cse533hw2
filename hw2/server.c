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
			/* Zero out memory */
			memset(&sigac_child, '\0', sizeof(sigac_child));
			memset(&sigac_alarm, '\0', sizeof(sigac_alarm));
			/* Set values */
			sigac_child.sa_sigaction = &sigchld_handler;
			sigac_child.sa_flags = SA_SIGINFO;
			sigac_alarm.sa_sigaction = &sigalrm_timeout;
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
			if(errno == EINTR) {
				/* interrupted by a SIGCHLD */
				continue;
			} else {
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
					if(errno != EINTR) {
						error("Error on recvfrom: %s\n", strerror(errno));
					}
					node = node->next;
					continue;
				}
				if(server_valid_syn(valid_pkt, &pkt)) {
					sigset_t sigchld_mask;

					sigemptyset(&sigchld_mask);
					sigaddset(&sigchld_mask, SIGCHLD);
					/* Block SIGCHLD until we add the child procces to the list */
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
					debug("Ignoring invalid SYN packet.\n");
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
			error("Failed to fork child process.\n");
			break;
		case 0:
			/* In child */
			process->pid = getpid();
			info("Server Child - PID: %d\n", process->pid);
			/* Close unneeded parent FD */
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
			info("Child process has finished; pid = %d\n", process->pid);
			/* Clean up memory */
			debug("Freeing interfaces list - %d.\n", process->pid);
			destroy_interfaces(&interfaces);
			/* Free up any process information that is left over */
			debug("Freeing processes list - %d.\n", process->pid);
			destroy_processes(&processes);
			// Quit the child
			exit(EXIT_SUCCESS);
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
	char file[STCP_MAX_DATA + 1];
	int fd;
	// Extract file name
	strncpy(file, pkt->data, pkt->dlen);
	file[pkt->dlen] = '\0';
	info("Searching for file: '%s'\n", file);
	// Attempt to open the file
	fd = open(file, O_RDONLY);
	if(fd >= 0) {
		int sock;
		bool samesub;
		struct stcp_pkt ack;
		struct sockaddr_in client_addr, server_addr;

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
			int timeout_duration = 1;
			int timeout_attempts = 0;
			fd_set handshake_set;
			struct timeval tv;
			int select_result;
			bool valid_ack = false;
			/* Construct the SYN ACK response */
			uint32_t init_seq = 0;
			uint32_t init_ack = pkt->hdr.seq + 1;
			uint32_t rwin_adv;
			struct stcp_pkt syn_ack;
			build_pkt(&syn_ack, init_seq, init_ack, 0, STCP_SYN|STCP_ACK,
						&server_addr.sin_port, sizeof(server_addr.sin_port));
			do {
				/* Wait for client's ACK */
				FD_ZERO(&handshake_set);
				FD_SET(sock, &handshake_set);

				/* Send SYN | ACK on the original socket */
				len = sendto_pkt(process->interface_fd, &syn_ack, 0,
						(struct sockaddr*)&client_addr, sizeof(client_addr));
				if(len < 0) {
					error("Failed to send SYN_ACK on out of band UDP socket.\n");
				}
				if(timeout_attempts > 0) {
					/* Send syn_ack on the new connected socket as well */
					len = send_pkt(sock, &syn_ack, 0);
					if(len < 0) {
						error("Failed to send SYN_ACK on connected UDP socket.\n");
					}
				}

				// Calculate timeout
				tv.tv_sec = timeout_duration;
				tv.tv_usec = 0;
				// Wait for FD to be set
				select_result = select(sock + 1, &handshake_set, NULL, NULL, &tv);
				if(select_result == 0) {
					timeout_attempts++;
					timeout_duration += timeout_duration;
					warn("Socket timed out, Attempt: %d - Waiting %d seconds\n", timeout_attempts, timeout_duration);
				} else if(select_result == -1) {
					error("Select failed with error: %s\n", strerror(errno));
				} else if (FD_ISSET(sock, &handshake_set)) {
					int valid_pkt;
					if((valid_pkt = recv_pkt(sock, &ack, 0)) < 0) {
						if(errno == ECONNREFUSED) {
							warn("Client has not connected to port %hu yet!\n", ntohs(server_addr.sin_port));
							timeout_attempts++;
							timeout_duration += timeout_duration;
						} else {
							error("Fatal error on recv: %s\n", strerror(errno));
							goto clean_up;
						}
					} else if(valid_pkt == 1) {
						if(ack.hdr.flags & STCP_ACK) {
							/* The advertised window size of the client */
							rwin_adv = ack.hdr.win;
							valid_ack = 1;
						}
					}
				}
				// Break out of the loop and quit attempting to serve this client
				if(timeout_attempts >= MAX_TIMEOUT_ATTEMPTS) {
					error("Failed to initiate SYN_ACK handshake.\n");
					goto clean_up;
				}
			} while(!valid_ack && timeout_attempts < MAX_TIMEOUT_ATTEMPTS);

			// Check to make sure we didn't have too many handshake attempts
			if(timeout_attempts >= MAX_TIMEOUT_ATTEMPTS) {
				error("Unable to complete three-way handshake on SYN_ACK step.\n");
				goto clean_up;
			}

			/* Close original interface socket */
			if(close(process->interface_fd) != 0) {
				warn("Failed to close the interface fd: %d\n", processes->interface_fd);
			}

			/* Connection established start sending file */
			info("Connected with client %s:%hu. Starting transfer.\n",
					inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
			/* TODO: Must mask SIGALRM during read and make sure it is in the Window */
			if(transfer_file(sock, fd, process->interface_win_size, init_seq + 1, rwin_adv)) {
				success("Child Server %u transfered '%s' to %s:%u\n",
					process->pid, file, process->ip_address, process->port);
			} else {
				error("Child Server %u failed to transfer '%s' to %s:%u\n",
					process->pid, file, process->ip_address, process->port);
			}
		} else {
			error("Failed to create a socket.\n");
		}
clean_up:
		/* Close open socket */
		if(sock >= 0) {
			close(sock);
		}
		/* Close the opened file */
		if(close(fd) < 0) {
			error("Error closing file '%s': %s\n", file, strerror(errno));
		}
	} else {
		error("Error opening file '%s': %s\n", file, strerror(errno));
	}
}

#define SLOW_START(c) do{ \
				(c).ssthresh = (c).cwnd / 2; \
				if((c).ssthresh == 0){(c).ssthresh = 1;}\
				(c).cwnd = 1;} while(0)

int transfer_file(int sock, int fd, unsigned int win_size, uint32_t init_seq,
					uint32_t rwin_adv) {
	int success = 0, i, ret;
	Window swin;
	Elem *elem;
	struct stcp_pkt ack;
	bool sending = true;
	bool eof = false;
	bool deadlock = false;
	struct rtt_info rtt;
	int cwnd_inc;
	/* Get ourselves a sliding window buffer */
	if(win_init(&swin, win_size, init_seq) < 0) {
		error("Failed to initialize sliding window.\n");
		return -1;
	}
	/* Init the rtt struct */
	rtt_init(&rtt);
	/* Set the initial Advertised window of the client */
	swin.rwin_adv = rwin_adv;
	/* Print out the initial sending window */
	display_window(&swin, &rtt);
	/* Commence file transfer */
	do {
		// Populate the send buffer
		while(!eof && win_count(&swin) < win_send_limit(&swin)) {
			// warn("Send limit: %d\n", win_send_limit(&swin));
			// TODO: Check case when cwnd < count
			if((ret = win_buffer_elem(&swin, fd)) < 0) {
				/* critical error */
				sending = false;
				success = false;
				goto clean_up;
			} else {
				eof = ret? false : true;
				debug("Buffered packet successfully.\n");
			}
		}
		/* Set the longjump marker here */
		if(sigsetjmp(env, 1) != 0)  {
			/* Handle timeout stuff here */
			// Handle nonsense with ssthresh
			SLOW_START(swin);
			warn("RTO reached. Updated cwnd: %hu ssthresh: %hu\n", swin.cwnd, swin.ssthresh);
			// Set inflight to zero (everything was lost)
			swin.in_flight = 0;
			swin.dup_ack = 0;
			if(rtt_timeout(&rtt) == -1) {
				/* We reach max retries time to quit! */
				error("Transfer failed reached max retransmission attempts!\n");
				success = false;
				goto clean_up;
			}
			if(swin.rwin_adv == 0) {
				/* Handle TCP DEADLOCK */
				deadlock = true;
				warn("Entering TCP deadlock avoidance.\n");
				/* if the window is empty and we deadlocked add another Elem! */
				if(win_empty(&swin)) {
					if((ret = win_buffer_elem(&swin, fd)) < 0) {
						sending = false;
						success = false;
						goto clean_up;
					} else {
						eof = ret? false : true;
						debug("Buffered deadlock avoidance packet.\n");
					}
				}
			}
		}
send_payload:
		/* send up to cwnd packets */
		for(i = swin.in_flight; ((swin.in_flight < swin.cwnd) && i < win_count(&swin)) || deadlock; i++) {
			elem = win_get_index(&swin, i);
			if((ret = send_pkt(sock, &elem->pkt, 0)) < 0) {
				error("Fatal error on send: %s\n", strerror(errno));
				goto clean_up;
			} else {
				/* Set the usec timestamp for this element */
				elem->sent_ts = rtt_getusec();
				if(i == 0) {
					/* Copy the sent timestamp for the most outstanding packet */
					rtt.rtt_base = elem->sent_ts;
				}
				swin.in_flight++;
				/* reset TCP deadlock so we don't send packets forever */
				deadlock = false;
			}
		}
		/* Print out the current state of the sending window */
		display_window(&swin, &rtt);
		set_timeout(&rtt);
		/* receive packet */
		do {
			if(swin.dup_ack == STCP_FAST_RETRANSMIT) {
				/* Do Fast Retransmit */
				clear_timeout();
				swin.dup_ack = 0;
				swin.in_flight = 0;
				if(swin.cwnd > 1){
					swin.cwnd = swin.cwnd/2;
				}
				swin.ssthresh = swin.cwnd;
				warn("FastRetransmit: Recv 3 Dup ACKs. Updated cwnd: %hu ssthresh: %hu\n", swin.cwnd, swin.ssthresh);
				goto send_payload;
			}
			/* Keep checking under the alarm condition until
			   we timeout or get a good ack. */
			ret = recv_pkt(sock, &ack, 0);
			/* what if we get SIGALRM here????? Race condition */
			if(ret < 0) {
				error("Fatal error on recv: %s\n", strerror(errno));
				sending = false;
				success = false;
				goto clean_up;
			}
			/* It doesn't matter if we get a SIGALRAM during this while
			 * loop because these functions don't update critical
			 * information in the sending window.
			 */
		} while(ret == 0 || !win_valid_ack(&swin, &ack) || win_dup_ack(&swin, &ack));
		clear_timeout();
		/* Update rtt only for the next expected ACK (not for duplicates) */
		//warn("swin.next_ack=%u, ack.hdr.ack=%u\n", swin.next_ack, ack.hdr.ack);
		if(swin.next_ack == ack.hdr.ack) {
			rtt_stop(&rtt);
			debug("Updating RTT\n");
		}
		/* reset number of retries to 0 */
		rtt_newpack(&rtt);

		/* Decrement inflight packet count since ack was valid */
		ret = win_remove_ack(&swin, &ack);
		swin.in_flight -= ret;
		if(swin.in_flight < 0)
			swin.in_flight = 0;
		/* Update cwnd */
		if(swin.cwnd < swin.ssthresh) {
			if(swin.cwnd + ret < swin.ssthresh) {
				swin.cwnd += ret;
			} else {
				swin.cwnd = swin.ssthresh;
				cwnd_inc = 0;
			}
			info("SlowStart: Received ACK for %d packets. Updated cwnd to %hu\n", ret, swin.cwnd);
		} else {
			/* Increment swin.cwnd by 1 for each cwnd packets we get acks for */
			cwnd_inc += ret;
			if(cwnd_inc >= swin.cwnd) {
				swin.cwnd += 1;
				cwnd_inc = 0;
				info("CongestionAvoidance: Received ACKs for cwnd packets. Updated cwnd to %hu\n", swin.cwnd);
			}
		}

		/* Check to make sure we didn't do something stupid */
		if(swin.in_flight < 0) {
			error("Negative window.in_flight value.\n");
			goto clean_up;
		}

		/* Check to see if we are done? */
		if(win_count(&swin) == 0 && eof) {
			debug("Exiting loop correctly.\n");
			sending = false;
			success = true;
		}
	} while(sending);
clean_up:
	clear_timeout();
	/* Print out the current state of the sending window after being done */
	display_window(&swin, &rtt);
	win_destroy(&swin);
	return success;
}

static void sigchld_handler(int signum, siginfo_t *siginfo, void *context) {
	int pid;

	while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
		Process *process = get_process_by_pid(processes, pid);
		if(process != NULL) {
			if(remove_process(&processes, process)) {
				info("Successfully removed the process %d\n", (int)pid);
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

static void set_timeout(struct rtt_info *rtt) {
	struct itimerval timer;
	long int sec,usec;
	sec = (long int)rtt->rtt_rto / 1000000L;
	usec = (long int)rtt->rtt_rto % 1000000L;
	timer.it_value.tv_sec = sec;
	timer.it_value.tv_usec = usec;
	timer.it_interval.tv_sec = sec;
	timer.it_interval.tv_usec = usec;
	info("RTO: %ldsecs + %ldusecs. SRTT: %dusecs. RTTVAR: %dusecs.\n", sec,
			usec, rtt->rtt_srtt, rtt->rtt_rttvar);
	setitimer(ITIMER_REAL, &timer, NULL);
}

static void clear_timeout() {
	struct itimerval timer;
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
	debug("Clear timeout\n");
}

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
	// Send the packet and see what happens
	bytes = sendto_pkt(socket, pkt, 0, (struct sockaddr*)&client, sizeof(client));
	if(bytes >= 0) {
		debug("Sent %d bytes to the client\n", bytes);
	}
	return bytes;
}

int server_transmit_payload2(int socket, int seq, int ack, struct stcp_pkt *pkt,
							Process *process, int flags, void *data, int datalen) {
	int bytes = 0;
	// Set up packet data
	build_pkt(pkt, seq, ack, process->interface_win_size, flags, data, datalen);
	// Send the packet and see what happens
	bytes = send_pkt(socket, pkt, 0);
	if(bytes >= 0) {
		debug("Sent %d bytes to the client\n", bytes);
	}
	return bytes;
}

#define WINDOW_BORDER() do{printf("+------------------+"); \
						   printf("------------------+"); \
						   printf("------------------+"); \
						   printf("------------------+\n");} while(0)

#define WINDOW_ROW(v1Label, v1Value, v2Label, v2Value, v3Label, v3Value, v4Label, v4Value) do { \
				   	 	 printf("| %-9s %-6u |", (v1Label), (v1Value)); \
				   	 	 printf(" %-9s %-6u |", (v2Label), (v2Value)); \
				   	 	 printf(" %-9s %-6u |", (v3Label), (v3Value)); \
				   	 	 printf(" %-9s %-6u |\n", (v4Label), (v4Value)); \
						} while(0)

#define HEADER(label) do{ \
						int size, i; \
						printf("+------------------+\n"); \
						printf("| %s", (label)); \
						size = strlen((label)); \
						for(i = 0; i < (20 - size - 4); i++) printf(" "); \
						printf(" |\n"); \
					} while(0);

#define DIVIDER() printf("+------------------+------------------+------------------+" \
						"------------------+\n")

#define PRINT_ELEM(val, f) do { \
							printf("| %-16" f " ", (val)); \
						} while(0)

#define PRINT_SINGLE_DIV() printf("+------------------")

void display_window(Window *window, struct rtt_info *rtt) {
	int i, j;
	char pid_str[19];
	// Convert pid to str
	sprintf(pid_str, "PID: %d", (int)getpid());
	/* Print the pid Header */
	HEADER(pid_str);
	/* Print the window header */
	WINDOW_BORDER();
	/* Print our CWND - SEND Limit - ssthresh Retries ? */
	WINDOW_ROW("CWND:", window->cwnd, "SNDLMT:", win_send_limit(window), "ssthresh:", window->ssthresh,
		"nrexmt:", rtt->rtt_nrexmt);
	/* Seperate the rows */
	DIVIDER();
	/* Window Size - Count - in flight - ? */
	WINDOW_ROW("WINSIZE:", window->size, "Count:", window->count, "INFLGHT:", window->in_flight,
		"RWINADV:", window->rwin_adv);
	/* Print the window footer */
	WINDOW_BORDER();
	/* Extra newline */
	printf("\n");
	/* Print buffer header */
	HEADER("Sending Window");

	/* Print the elem footer */
	if(window->size < 4 && window->size % 4 != 0) {
		for(j = 0; j < window->size % 4; j++) {
			PRINT_SINGLE_DIV();
		}
		printf("+\n");
	} else {
		DIVIDER();
	}
	
	/* Print Elements */
	for(i = 0; i < window->size; ++i) {
		Elem *elem =  &window->buf[i];
		// Print out the last pipe and newline 
		if(i % 4 == 0 && i != 0) {
			printf("|\n");
			/* Print divider */
			DIVIDER();
		}
		// Print out the element information
		if(elem->valid) {
			PRINT_ELEM(elem->pkt.hdr.seq, "u");
		} else {
			PRINT_ELEM("_", "s");
		}
	}
	/* Handle end of window */
	if(i % 4) {
		printf("|\n");
	} else {
		printf("|\n");
		/* Print divider */
		DIVIDER();
	}
	/* Print the elem footer */
	for(j = 0; j < window->size % 4; j++) {
		PRINT_SINGLE_DIV();
	}
	/* Print out the final + for the table output */
	if(i != 4 && i % 4 != 0) {
		printf("+\n");
	}
	/* Buffer with an extra newline */
	printf("\n");
}
