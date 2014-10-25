#include "server.h"

int main(int argc, char *argv[]) {
	char *path = "server.in";
	Config config;
	int server_fd = 0;
	debug("Begin parsing %s\n", path);
	/* Attempt to parse the config */
	if(parseServerConfig(path, &config)) {
		debug("Port: %u\n", config.port);
		debug("Window Size: %u\n", config.win_size);
		/* Config was successfully parsed; attempt to start server */
		if((server_fd = createServer(config.port)) != -1) {
			/* Start the servers main loop */
			run(server_fd, &config);
		} else {
			/* Unable to bind socket to port */
			fprintf(stderr, "Failed to bind socket @ port %u\n", config.port);
		}
	} else {
		/* The config parsing failed */
		debug("Failed to parse: %s\n", path);
	}
	return EXIT_SUCCESS;
}

void run(int server_fd, Config *config) {
	bool running = true;
	int recv_length = 0;
	unsigned char buffer[SERVER_BUFFER_SIZE];
	struct sockaddr_in connection_addr;
	socklen_t connection_len = sizeof(connection_addr);
    struct stcp_pkt pkt;
    pkt.hdr.syn = htonl(0);
    pkt.hdr.ack = htonl(1);
    pkt.hdr.win = htons(config->win_size);
    pkt.hdr.flags = htons(STCP_SYN | STCP_ACK);
    strcpy(pkt.data, "5656");
    pkt.dlen = 4;

	debug("Server waiting on port %u\n", config->port);
	discoverInterfaces();
	while(running) {
		recv_length = recvfrom(server_fd, buffer, SERVER_BUFFER_SIZE, 0, (struct sockaddr *)&connection_addr, &connection_len);
		printf("received %d bytes\n", recv_length);
		if (recv_length > 0) {
				buffer[recv_length] = '\0';
				printf("received message: '%s'\n", buffer);
				/* Send to client */
				if(sendto(server_fd, &pkt, sizeof(pkt) + pkt.dlen, 0,
						(struct sockaddr *)&connection_addr, connection_len) < 0) {
					perror("run: sendto");
					break;
				}
		}
	}
}

Interface* discoverInterfaces() {
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
		// Determine the type of socket
		printf("<");
		if (ifi->ifi_flags & IFF_UP)			printf("UP ");
		if (ifi->ifi_flags & IFF_BROADCAST)		printf("BCAST ");
		if (ifi->ifi_flags & IFF_MULTICAST)		printf("MCAST ");
		if (ifi->ifi_flags & IFF_LOOPBACK)		printf("LOOP ");
		if (ifi->ifi_flags & IFF_POINTOPOINT)	printf("P2P ");
		printf("\b>\n");
		// TODO: Determine if this is a unicast interface
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
			printf("<%s>\nIP: %s\nMask: %s\nSubnet: %s\n\n", node->name, 
				node->ip_address, node->network_mask, node->subnet_address);
		#endif
	}
	free_ifi_info_plus(ifihead);
	return list;
}
