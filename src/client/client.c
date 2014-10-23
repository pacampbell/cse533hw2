#include "client.h"

int main(int argc, char *argv[]) {
	debug("Client dummy program: %s\n", argv[0]);

	int sockfd;
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
		debug("Port: %hu\n", config.port);
		debug("Filename: %s\n", config.filename);
		debug("Window Size: %u\n", config.win_size);
		debug("Seed: %u\n", config.seed);
		debug("Prob loss: %f%%\n", config.loss);
		debug("Mean: %u\n", config.mean);

		local = chooseIPs(&config, &serv_addr.sin_addr, &client_addr.sin_addr);
		/* finish initializing addresses */
		serv_addr.sin_port = htons(config.port);
		client_addr.sin_port = htons(0);
		/* socket create/bind/connect */
		sockfd = createClientSocket(&serv_addr, &client_addr, local);
		/* Config was successfully parsed; attempt to connect to server */
		if((sockfd = handshake(&config, sockfd)) >= 0) {
			/* Start the client producer/consumer threads */
			run(sockfd, &config);
		} else {
			/* Unable to connect to server  */
			fprintf(stderr, "handshake failed with server @ %s port %hu\n",
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

/**
 * The client sends a datagram to the server giving the filename for the transfer.
 * This send needs to be backed up by a timeout in case the datagram is lost.
 *
 */
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
