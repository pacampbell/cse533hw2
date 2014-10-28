#include "utility.h"

bool parseServerConfig(char *path, Config *config) {
	bool success = false;
	int fd, rlen;
	char file[2*BUFFER_SIZE];
	if((fd = open(path, O_RDONLY)) < 0) {
		error("Error opening config file: %s\n", strerror(errno));
		return false;
	}
	/* read everything from the file */
	if((rlen = read(fd, file, sizeof(file))) > 0) {
		if(sscanf(file, "%hu\n%u", &config->port, &config->win_size) != 2) {
			error("Config file: failed to parse both lines. Please verify format\n");
			success = false;
		} else {
			/* Success!!!! */
			success = true;
		}
	} else if(rlen < 0) {
		error("Error reading config file: %s\n", strerror(errno));
		success = false;
	} else {
		error("Config file was empty!\n");
		success = false;
	}
	if(close(fd) < 0) {
		error("Error closing config file: %s\n", strerror(errno));
		return false;
	}
	return success;
}

bool parseClientConfig(char *path, Config *config) {
	bool success = false;
	int fd, rlen;
	char file[7 * 256];
	if((fd = open(path, O_RDONLY)) < 0) {
		error("Error opening config file: %s\n", strerror(errno));
		return false;
	}
	/* read everything from the file */
	if((rlen = read(fd, file, sizeof(file))) > 0) {
		char ipbuf[256];
		if(sscanf(file, "%255s\n%hu\n%255s\n%u\n%d\n%lf\n%u",
				ipbuf, &config->port, config->filename, &config->win_size,
				&config->seed, &config->loss, &config->mean) != 7) {
			error("Config file: failed to parse all 7 lines. Please verify format\n");
			success = false;
		} else {
			/* convert IP to a struct in_addr */
			if(inet_aton(ipbuf, &config->serv_addr) == 0) {
				error("Config file: failed to parse IP '%s'\n", ipbuf);
				success = false;
			} else {
				/* Success!!!! */
				success = true;
			}
		}
	} else if(rlen < 0) {
		error("Error reading config file: %s\n", strerror(errno));
		success = false;
	} else {
		error("Config file was empty!\n");
		success = false;
	}
	if(close(fd) < 0) {
		error("Error closing config file: %s\n", strerror(errno));
		return false;
	}
	return success;
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
	if(set_nonblocking(fd) < 0) {
		error("Failed to set socket to non-blocking: %s:%d\n", address, port);
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
		address = ntohl(address);
	}
	return address;
}

int set_nonblocking(int sockfd) {
	int fileflags;

	if((fileflags = fcntl(sockfd, F_GETFL, 0)) == -1) {
		perror("fcntl F_GETFL");
		return -1;
	}
	if (fcntl(sockfd, F_SETFL, fileflags | O_NONBLOCK) == -1)  {
		perror("fcntl F_SETFL, O_NONBLOCK");
		return -1;
	}
	return 0;
}

int set_blocking(int sockfd) {
	int fileflags;

	if((fileflags = fcntl(sockfd, F_GETFL, 0)) == -1) {
		perror("fcntl F_GETFL");
		return -1;
	}
	if (fcntl(sockfd, F_SETFL, fileflags & ~O_NONBLOCK) == -1)  {
		perror("fcntl F_SETFL, O_NONBLOCK");
		return -1;
	}
	return 0;
}
