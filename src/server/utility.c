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
				config->port = (unsigned int) atoi(line);
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
