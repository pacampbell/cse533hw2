#include "utility.h"

bool parseServerConfig(char *path, Config *config) {
	int fd, rlen;
	char file[2*BUFFER_SIZE];
	if((fd = open(path, O_RDONLY)) < 0) {
		error("Error opening config file: %s\n", strerror(errno));
		return false;
	}
	/* read everything from the file */
	if((rlen = read(fd, file, sizeof(file))) > 0) {
		int win_size, port;
		if(sscanf(file, "%d\n%d", &port, &win_size) != 2) {
			error("Config file: failed to parse both lines. Please verify format\n");
			goto failed;
		}
		/* Validate port and window size */
		if(port <= 0 || port > 0xFFFF) {
			error("Config file: server port must be between [1, 65536]!\n");
			goto failed;
		}
		if(win_size <= 0 || win_size > 0xFFFF) {
			error("Config file: window size must be between [1, 65536]!\n");
			goto failed;
		}
		config->win_size = (unsigned int)win_size;
		config->port = (unsigned short)port;
	} else if(rlen < 0) {
		error("Error reading config file: %s\n", strerror(errno));
		goto failed;
	} else {
		error("Config file was empty!\n");
		goto failed;
	}
	if(close(fd) < 0) {
		error("Error closing config file: %s\n", strerror(errno));
		return false;
	}
	return true;
failed:
	close(fd);
	return false;
}

bool parseClientConfig(char *path, Config *config) {
	int fd, rlen;
	char file[7 * 256];
	if((fd = open(path, O_RDONLY)) < 0) {
		error("Error opening config file: %s\n", strerror(errno));
		return false;
	}
	/* read everything from the file */
	if((rlen = read(fd, file, sizeof(file))) > 0) {
		char ipbuf[256];
		int win_size, port, mean;
		if(sscanf(file, "%255s\n%d\n%255s\n%d\n%d\n%lf\n%d",
				ipbuf, &port, config->filename, &win_size,
				&config->seed, &config->loss, &mean) != 7) {
			error("Config file: failed to parse all 7 lines. Please verify format\n");
			goto failed;
		}
		/* convert IP to a struct in_addr */
		if(inet_aton(ipbuf, &config->serv_addr) == 0) {
			error("Config file: failed to parse IP '%s'\n", ipbuf);
			goto failed;
		}
		/* Successfully parsed all 7 options, now validate */
		if(port <= 0 || port > 0xFFFF) {
			error("Config file: server port must be between [1, 65536]!\n");
			goto failed;
		}
		if(win_size <= 0 || win_size > 0xFFFF) {
			error("Config file: window size must be between [1, 65536]!\n");
			goto failed;
		}
		if(mean <= 0) {
			error("Config file: mean (for consumer sleep) must be positive!\n");
			goto failed;
		}
		if(config->loss < 0.0 || config->loss > 1.0) {
			error("Config file: loss rate must be between [0.0, 1.0]!\n");
			goto failed;
		}
		config->win_size = (unsigned int)win_size;
		config->port = (unsigned short)port;
		config->mean = (unsigned int)mean;
	} else if(rlen < 0) {
		error("Error reading config file: %s\n", strerror(errno));
		goto failed;
	} else {
		error("Config file was empty!\n");
		goto failed;
	}
	if(close(fd) < 0) {
		error("Error closing config file: %s\n", strerror(errno));
		return false;
	}
	return true;
failed:
	close(fd);
	return false;
}

int createServer(char *address, unsigned int port) {
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
	inet_aton(address, &addr.sin_addr);
	addr.sin_port = htons(port);
	// Attempt to bind socket
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		error("Failed to bind: %s:%d\n", address, port);
		fd = SERVER_SOCKET_BIND_FAIL;
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
		error("Failed to create socket: %s\n", strerror(errno));
		return -1;
	}
	/* set the socket SO_DONTROUTE option if we're local */
	if(local) {
		info("Setting the option SO_DONTROUTE connection socket\n");
		if(setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &optval,
						sizeof(optval)) < 0) {
			error("Failed to setsockopt: %s\n", strerror(errno));
			close(sockfd);
			return -1;
		}
	}
	/* bind socket to the client's address */
	if(bind(sockfd, (struct sockaddr*)client_addr,
			sizeof(struct sockaddr_in)) < 0) {
		error("Failed to bind: %s\n", strerror(errno));
		close(sockfd);
		return -1;
	}
	/* Get the port we are bound to */
	if(getsockname(sockfd, (struct sockaddr*)client_addr, &len) < 0) {
		error("Failed to getsockname: %s\n", strerror(errno));
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
		error("Failed to connect: %s\n", strerror(errno));
		return -1;
	}
	/* Get the peername we connected to */
	len = sizeof(struct sockaddr_in);
	if(getpeername(sockfd, (struct sockaddr*)peer, &len) < 0) {
		error("Failed to getpeername: %s\n", strerror(errno));
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
		address = ntohl(address);
	}
	return address;
}
