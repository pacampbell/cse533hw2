#include "server.h"

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
		destroy_interfaces(&interfaces);
	} else {
		/* The config parsing failed */
		debug("Failed to parse: %s\n", path);
	}
	return EXIT_SUCCESS;
}

void run(Interface *interfaces, Config *config) {
	fd_set rset;
	Interface *node;
	Process *processes = NULL, *process = NULL;
	int largest_fd = 0;
	bool running = true;

	// Packet stuff that should probably be in child.
	int recv_length = 0;
	struct stcp_pkt pkt;
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
				// START HERE: check if process already exists, fork child, Check for same subnet, create new out of band socket, etc.
				debug("Detected connection on interface: <%s> %s\n", node->name, node->ip_address);
				recv_length = recvfrom_pkt(node->sockfd, &pkt, 0, (struct sockaddr *)&connection_addr, &connection_len);
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
						pid = spawnchild(interfaces, new_process);
						if(pid == -1 || pid == 0) {
							// Either fork failed or we are in the child process
							// Set running to false and break out of this loop
							info("Child process has finished; pid = %d\n", pid);
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
				// Once child is spawned check to see if same subnet
				// create appropiate socket with correct permissions
				// Send back to client

				printf("received %d bytes\n", recv_length);
				print_hdr(&pkt.hdr);

				pkt.hdr.ack = pkt.hdr.seq + 1; 
				pkt.hdr.seq = 0;
				pkt.hdr.win = config->win_size;
				pkt.hdr.flags = STCP_ACK | STCP_SYN;
				pkt.dlen = 0;

				printf("Sending packet header: ");
				print_hdr(&pkt.hdr);

				recv_length = sendto_pkt(node->sockfd, &pkt, 0, (struct sockaddr *)&connection_addr, connection_len);
				if(recv_length < 0) {
					perror("");
					warn("Failed to send pkt\n");
				}
			}
			node = node->next;
		}
	}
	// Free up any process information that is left over
	destroy_processes(&processes);
}

int spawnchild(Interface *interfaces, Process *process) {
	int pid = fork();
	switch(pid) {
		case -1:
			/* Failed */
			error("Failed to fork child env.\n");
			break;
		case 0:
			/* In child */
			info("Server Child - started!\n");
			// TODO: Close connection to other interfaces
			childprocess(process);
			break;
		default:
			/* In parent */
			process->pid = pid;
			info("Main Server - Child PID: %d\n", pid);
			break;
	}
	return pid;
}

void childprocess(Process *process) {
	int sock = 0;
	bool samesub = false;
	struct sockaddr_in client_addr, server_addr;
	// Zero out memory
	memset((char *)&client_addr, 0, sizeof(client_addr));
	memset((char *)&server_addr, 0, sizeof(server_addr));
	// Set client fields
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(process->port);
	inet_aton(process->ip_address, &client_addr.sin_addr);
	// Set server fields
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(0);
	inet_aton(process->interface_ip_address, &server_addr.sin_addr);

	// Attach socket depending on if local or not
	samesub = isSameSubnet(process->interface_ip_address, 
		process->ip_address, process->interface_network_mask);
	sock = createClientSocket(&client_addr, &server_addr, samesub);

	// Send SYN | ACK for new socket
	if(sock > 0) {
		warn("SYN | ACK not implemented.\n");
	} else {
		error("Failed to create a socket.\n");
	}
}
