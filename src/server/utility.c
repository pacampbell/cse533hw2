#include "utility.h"

bool parseConfig(char *path, Config *config) {
	bool success = false;
	if(path != NULL && config != NULL) {
		FILE *config_file = fopen(path, "r");
		if(config_file != NULL) {
			/* Atempt to read the file line by line */
			char line[BUFFER_SIZE];
			/* Read in the port value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->port = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* Read in the mtu size */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->mtu = (unsigned int) atoi(line); 
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
