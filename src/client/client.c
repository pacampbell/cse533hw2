#include "client.h"

int main(int argc, char *argv[]) {
	debug("Client dummy program: %s\n", argv[0]);

	int fd;
	char *path = "client.in";
	Config config;

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
		/* Config was successfully parsed; attempt to connect to server */
		if((fd = handshake(&config)) >= 0) {
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

int handshake(Config *config) {
	int fd;
	struct sockaddr_in servaddr;    /* server address */
	char *my_message = "this is a test message";

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(config->port);

	/* copy the servers' address into the sockaddr_in structure */
	memcpy((void *)&servaddr.sin_addr, (void *)&config->serv_addr, sizeof(struct in_addr));

	/* Socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket");
		return 0;
	}

	/* send a message to the server */
	if (sendto(fd, my_message, strlen(my_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("sendto failed");
		return 0;
	}

	return -1;
}

int run(int conn_fd, Config *config) {
	return 0;
}