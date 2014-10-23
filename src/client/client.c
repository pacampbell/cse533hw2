#include "client.h"

int main(int argc, char *argv[]) {
	debug("Client dummy program: %s\n", argv[0]);

	int fd;
	char *path = "client.in";
	Config config;
	struct sockaddr_in serv_addr, client_addr;
	bool local;

	/* Zero out the structs */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;

	debug("Begin parsing config file: %s\n", path);
	/* Attempt to parse the config */
	if(parseClientConfig(path, &config)) {
		debug("Server Address: %s\n", inet_ntoa(config.serv_addr));
		debug("Port: %u\n", config.port);
		debug("Filename: %s\n", config.filename);
		debug("Window Size: %u\n", config.win_size);
		debug("Seed: %u\n", config.seed);
		debug("Prob loss: %f%%\n", config.loss);
		debug("Mean: %u\n", config.mean);

		local = chooseIPs(&config, &serv_addr.sin_addr, &client_addr.sin_addr);
		/* finish initializing addresses */
		serv_addr.sin_port = htons(config.port);
		client_addr.sin_port = htons(0);

		fd = createSocket(&serv_addr, &client_addr, local);
		/* Config was successfully parsed; attempt to connect to server */
		if((fd = handshake(&config, fd)) >= 0) {
			/* Start the client producer/consumer threads */
			run(fd, &config);
		} else {
			/* Unable to connect to server  */
			fprintf(stderr, "handshake failed with server @ %s port %u\n",
				inet_ntoa(config.serv_addr), config.port);
		}
	} else {
		/* The config parsing failed */
		debug("Failed to parse: %s\n", path);
	}

	return EXIT_SUCCESS;
}

bool chooseIPs(Config *config, struct in_addr *server_ip,
				struct in_addr *client_ip) {
	bool local = true;
	/* TODO: call get_ifi_info_plus */

	/* TODO: check if we match the server's address */

	/* TODO: Else use the longest match */
	/* For now just set server_ip to the config */
	server_ip->s_addr = config->serv_addr.s_addr;
	/* TODO: Else choose first IP from ifi_info */
	/* for now just set client_ip to loopback address */
	client_ip->s_addr = htonl(INADDR_LOOPBACK);

	printf("Server address is local. IPserver: %s, IPclient %s\n",
		inet_ntoa(*server_ip),inet_ntoa(*client_ip));
	return local;
}

int createSocket(struct sockaddr_in *serv_addr, struct sockaddr_in *client_addr,
				 bool local) {
	int sockfd;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		debug("Failed to create socket\n");
		return sockfd;
	}
	/* bind socket to the client's address */
	if(bind(sockfd, (struct sockaddr*)client_addr,
			sizeof(struct sockaddr_in)) < 0) {
		debug("Failed to bind to client address\n");
		perror("createSocket: bind");
		close(sockfd);
		return -1;
	}
	/* connect the DG socket to the server address */
	if(connect(sockfd, (struct sockaddr*)serv_addr,
				sizeof(struct sockaddr_in)) < 0) {
		debug("Failed to connect to server address\n");
		perror("createSocket: connect");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

int handshake(Config *config, int sockfd) {
	char *my_message = "this is a test message";

	/* send a message to the server */
	if (send(sockfd, my_message, strlen(my_message), 0) < 0) {
		perror("sendto failed");
		return 0;
	}

	return -1;
}

int run(int conn_fd, Config *config) {
	return 0;
}
