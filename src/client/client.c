#include "client.h"

int main(int argc, char *argv[]) {
	debug("Client dummy program: %s\n", argv[0]);

	int fd;
	char *path = "client.in";
	Config config;
	struct sockaddr_in serv_addr, client_addr;
	struct stcp_sock stcp;
	bool local;

	/* Zero out the structs */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;

	debug("Begin parsing config file: %s\n", path);
	/* Attempt to parse the config */
	if(!parseClientConfig(path, &config)) {
		/* The config parsing failed */
		debug("Failed to parse: %s\n", path);
	}
	debug("Server Address: %s\n", inet_ntoa(config.serv_addr));
	debug("Port: %hu\n", config.port);
	debug("Filename: %s\n", config.filename);
	debug("Window Size: %u\n", config.win_size);
	debug("Seed: %u\n", config.seed);
	debug("Prob loss: %f%%\n", config.loss);
	debug("Mean: %u\n", config.mean);

	/* Check interfaces and determine if we are local */
	local = chooseIPs(&config, &serv_addr.sin_addr, &client_addr.sin_addr);
	/* finish initializing addresses */
	serv_addr.sin_port = htons(config.port);
	client_addr.sin_port = htons(0);
	/* socket create/bind/connect */
	fd = createClientSocket(&serv_addr, &client_addr, local);

	/* initialize our STCP socket */
	if(stcp_socket(fd, config.win_size, &stcp) < 0) {
		fprintf(stderr, "stcp_socket failed\n");
		exit(EXIT_FAILURE);
	}
	/* Try to establish a connection to the server */
	if(stcp_connect(&stcp, &serv_addr, config.filename) < 0) {
		/* Unable to connect to server  */
		fprintf(stderr, "Handshake failed with server @ %s port %hu\n",
			inet_ntoa(config.serv_addr), config.port);
		goto stcp_failure;
	}
    printf("Connection established. Starting producer and consumer threads.");
	/* Start the producer and consumer threads  */
	/* Both threads use the same stcp structure */

	/* close the STCP socket */
	if(stcp_close(&stcp) < 0) {
		perror("main: stcp_close");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
stcp_failure:
	/* close the STCP socket */
	if(stcp_close(&stcp) < 0) {
		perror("main: stcp_close");
	}
	return EXIT_FAILURE;
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

int runProducer(struct stcp_sock *sock) {
    return 0;
}


int runConsumer(struct stcp_sock *sock, unsigned int seed, double loss, unsigned int mean) {
	return 0;
}
