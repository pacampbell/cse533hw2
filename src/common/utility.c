#include "utility.h"

bool parseServerConfig(char *path, Config *config) {
	bool success = false;
	if(path != NULL && config != NULL) {
		FILE *config_file = fopen(path, "r");
		if(config_file != NULL) {
			/* Atempt to read the file line by line */
			char line[BUFFER_SIZE];
			/* Read in the port value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->port = (unsigned short) atoi(line);
			} else {
				goto close;
			}
			/* Read in the window size */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->win_size = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* If we got this far must be a success */
			success = true;
close:
			/* Close the file handle */
			fclose(config_file);
		}
	}
	return success;
}

bool parseClientConfig(char *path, Config *config) {
	bool success = false;
	if(path != NULL && config != NULL) {
		FILE *config_file = fopen(path, "r");
		if(config_file != NULL) {
			/* Atempt to read the file line by line */
			char line[BUFFER_SIZE];
			/* Read in the server IP address value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				/* convert IP to a struct in_addr */
				if(inet_aton(line, &config->serv_addr) == 0) {
					goto close;
				}
			} else {
				goto close;
			}
			/* Read in the server port value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->port = (unsigned short) atoi(line);
			} else {
				goto close;
			}
			/* Read in the filename to be transferred */
			if(fgets(config->filename, BUFFER_SIZE, config_file) != NULL) {
				/* remove the new line */
				int len = strlen(config->filename);
				if(config->filename[len-1] == '\n')
					config->filename[len-1] = '\0';
			} else {
				goto close;
			}
			/* Read in the window size */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->win_size = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* Read in the random seed value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->seed = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* Read in the probability loss value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->loss = atof(line);
			} else {
				goto close;
			}
			/* Read in the mean of the exp. dist. value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->mean = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* If we got this far must be a success */
			success = true;
close:
			/* Close the file handle */
			fclose(config_file);
		}
	}
	return success;
}

int createServer(unsigned int port) {
	int fd = SERVER_SOCKET_BIND_FAIL;
	/* This server address */
	struct sockaddr_in addr;
	// Attempt to create the socket
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		goto finished;
	}
	// Zero out the struct
	memset((char *)&addr, 0, sizeof(addr));
	// Set fields
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	// Attempt to bind socket
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		goto finished;
	}
finished:
	return fd;
}

int createClientSocket(struct sockaddr_in *serv_addr,
				struct sockaddr_in *client_addr, bool local) {
	int sockfd;
	int optval = 1;
	socklen_t len = sizeof(struct sockaddr_in);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		perror("createClientSocket: socket");
		return -1;
	}
	/* set the socket SO_DONTROUTE option if we're local */
	if(local) {
		if(setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &optval,
						sizeof(optval)) < 0) {
			perror("createClientSocket: setsockopt");
			close(sockfd);
			return -1;
		}
	}
	/* bind socket to the client's address */
	if(bind(sockfd, (struct sockaddr*)client_addr,
			sizeof(struct sockaddr_in)) < 0) {
		perror("createClientSocket: bind");
		close(sockfd);
		return -1;
	}
	/* Get the port we are bound to */
	if(getsockname(sockfd, (struct sockaddr*)client_addr, &len) < 0) {
		perror("createClientSocket: getsockname");
		close(sockfd);
		return -1;
	}
	/* print the IP and port we binded to */
	printf("UDP binded: socket\t IP %s port %hu\n",
			inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	/* connect the UDP socket to the server address */
    if(udpConnect(sockfd, serv_addr) < 0) {
        return -1;
    }
	/* return the successfully created/binded/connected socket */
	return sockfd;
}

int udpConnect(int sockfd, struct sockaddr_in *peer) {
    socklen_t len = sizeof(struct sockaddr_in);

    /* connect the UDP socket to the peer address */
    if(connect(sockfd, (struct sockaddr*)peer,
            sizeof(struct sockaddr_in)) < 0) {
        perror("udpConnect: connect");
        return -1;
    }
    /* Get the peername we connected to */
    len = sizeof(struct sockaddr_in);
    if(getpeername(sockfd, (struct sockaddr*)peer, &len) < 0) {
        perror("udpConnect: getpeername");
        return -1;
    }
    /* print the IP and port we connected to */
    printf("UDP connected: peer\t IP %s port %hu\n",
            inet_ntoa(peer->sin_addr), ntohs(peer->sin_port));
    return 0;
}

unsigned int convertIp(char *ipaddress) {
	unsigned int address = 0;
	if(ipaddress != NULL) {
		unsigned int a = 0, b = 0, c = 0, d = 0;
		// Use scanf to split the string up
		sscanf(ipaddress, "%u.%u.%u.%u", &a, &b, &c, &d);
		// Shift the bits into place
		address = (address | a) << 8;
		address = (address | b) << 8;
		address = (address | c) << 8;
		address = (address | d);
	}
	return address;
}
