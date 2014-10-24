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

void discoverInterfaces() {
	struct ifi_info	*ifi, *ifihead;
	struct sockaddr	*sa;
	u_char *ptr;
	int i = 0;

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		printf("%s: ", ifi->ifi_name);
		if (ifi->ifi_index != 0)
			printf("(%d) ", ifi->ifi_index);
		printf("<");
		if (ifi->ifi_flags & IFF_UP)			printf("UP ");
		if (ifi->ifi_flags & IFF_BROADCAST)		printf("BCAST ");
		if (ifi->ifi_flags & IFF_MULTICAST)		printf("MCAST ");
		if (ifi->ifi_flags & IFF_LOOPBACK)		printf("LOOP ");
		if (ifi->ifi_flags & IFF_POINTOPOINT)	printf("P2P ");
		printf(">\n");
		if ( (i = ifi->ifi_hlen) > 0) {
			ptr = ifi->ifi_haddr;
			do {
				printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
			} while (--i > 0);
			printf("\n");
		}
		if (ifi->ifi_mtu != 0)
			printf("  MTU: %d\n", ifi->ifi_mtu);

		if ( (sa = ifi->ifi_addr) != NULL)
			printf("  IP addr: %s\n",
						Sock_ntop_host(sa, sizeof(*sa)));
		if ( (sa = ifi->ifi_ntmaddr) != NULL)
			printf("  network mask: %s\n",
						Sock_ntop_host(sa, sizeof(*sa)));
		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
						Sock_ntop_host(sa, sizeof(*sa)));
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
						Sock_ntop_host(sa, sizeof(*sa)));
	}
	free_ifi_info_plus(ifihead);
}
