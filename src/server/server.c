#include "server.h"

int main(int argc, char *argv[]) {
	char *path = "server.in";
	Config config;
	int server_fd = 0;
	debug("Begin parsing %s\n", path);
	/* Attempt to parse the config */
	if(parseServerConfig(path, &config)) {
		Interface *interfaces = NULL, *node = NULL;
		debug("Port: %u\n", config.port);
		debug("Window Size: %u\n", config.win_size);
		// Get a list interfaces
		interfaces = discoverInterfaces(&config);
		// Get the head
		node = interfaces;
		/* Config was successfully parsed; attempt to bind sockets */
		while(node != NULL) {
			server_fd = createServer(node->ip_address, config.port);
			if(server_fd != SERVER_SOCKET_BIND_FAIL) {
				node->sockfd = server_fd;
			} else {
				/* Unable to bind socket to port */
				/* TODO: Remove interface from interfaces? */
				fprintf(stderr, "Failed to bind socket: %s:%u\n", node->ip_address, config.port);
			}
			// Get next node
			node = node->next;
		}
		/* Start the servers main loop */
		run(interfaces, &config);

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
	int largest_fd = 0;
	bool running = true;
	// int recv_length = 0;
	// unsigned char buffer[SERVER_BUFFER_SIZE];
	// struct sockaddr_in connection_addr;
	// socklen_t connection_len = sizeof(connection_addr);
    struct stcp_pkt pkt;
    pkt.hdr.seq = htonl(0);
    pkt.hdr.ack = htonl(1);
    pkt.hdr.win = htons(config->win_size);
    pkt.hdr.flags = htons(STCP_SYN | STCP_ACK);
    strcpy(pkt.data, "5656");
    pkt.dlen = 4;

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
				// START HERE: Check for same subnet, fork child, create new out of band socket, etc.
			}
			node = node->next;
		}

		/*
		recv_length = recvfrom(server_fd, buffer, SERVER_BUFFER_SIZE, 0, (struct sockaddr *)&connection_addr, &connection_len);
		printf("received %d bytes\n", recv_length);
		if (recv_length > 0) {
				buffer[recv_length] = '\0';
				printf("received message: '%s'\n", buffer);
				
				if(sendto(server_fd, &pkt, sizeof(pkt) + pkt.dlen, 0,
						(struct sockaddr *)&connection_addr, connection_len) < 0) {
					perror("run: sendto");
					break;
				}
		}
		*/
	}
}

Interface* discoverInterfaces(Config *config) {
	struct ifi_info	*ifi, *ifihead;
	struct sockaddr	*sa;
	unsigned int a = 0, b = 0;
	// Create memory for the list.
	Interface *list = NULL;
	// Use crazy code to loop through the interfaces
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		// Create node
		Interface *node = malloc(sizeof(Interface));
		// Figure out where to place the node
		if(list == NULL) {
			list = node;	
		} else {
			// Just push it down the list
			// IE: It goes in reverse discovery order
			node->next = list;
			list->prev = node;
			list = node;
		}
		// Copy the name of the interface
		strcpy(node->name, ifi->ifi_name); 
		// Copy the IPAddress
		if((sa = ifi->ifi_addr) != NULL) {
			strcpy(node->ip_address, Sock_ntop_host(sa, sizeof(*sa)));
			a = convertIp(node->ip_address);
		}
		// Copy the network mask
		if((sa = ifi->ifi_ntmaddr) != NULL) {
			strcpy(node->network_mask, Sock_ntop_host(sa, sizeof(*sa)));
			b = convertIp(node->network_mask);
		}
		// Figure out the subnet mask
		if(a && b) {
			struct in_addr ip_addr;
			unsigned int subnet = a & b;
			ip_addr.s_addr = subnet;
			strcpy(node->subnet_address, inet_ntoa(ip_addr));
		}
		// Print out info
		#ifdef DEBUG
			printf("<%s> [%s%s%s%s%s\b] IP: %s Mask: %s Subnet: %s\n", 
				node->name,
				ifi->ifi_flags & IFF_UP ? "UP " : "",
				ifi->ifi_flags & IFF_BROADCAST ? "BCAST " : "",
				ifi->ifi_flags & IFF_MULTICAST ? "MCAST " : "",
				ifi->ifi_flags & IFF_LOOPBACK ? "LOOP " : "",
				ifi->ifi_flags & IFF_POINTOPOINT ? "P2P " : "",
				node->ip_address, 
				node->network_mask, 
				node->subnet_address);
		#endif
	}
	free_ifi_info_plus(ifihead);
	return list;
}
