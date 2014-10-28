#include "client.h"

int main(int argc, char *argv[]) {
	int fd, rv;
	char *path = "client.in";
	Config config;
	struct sockaddr_in serv_addr, client_addr;
	struct stcp_sock stcp;
	bool local;
	/* pthread variables */
	struct consumer_args args;
	pthread_t ptid;
	pthread_attr_t pattr;
	int *exit_status;
	// Init pthread attributes
	if((rv = pthread_attr_init(&pattr))) {
		errno = rv;
		perror("main: pthread_attr_init");
		exit(EXIT_FAILURE);
	}

	/* Zero out the structs */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;

	debug("Begin parsing config file: %s\n", path);
	/* Attempt to parse the config */
	if(!parseClientConfig(path, &config)) {
		/* The config parsing failed */
		error("Failed to parse config file: %s\n", path);
		exit(EXIT_FAILURE);
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
	/* set the packet drop rate of send and recv */
	client_set_loss(config.seed, config.loss);
	/* Try to establish a connection to the server */
	if(stcp_connect(&stcp, &serv_addr, config.filename) < 0) {
		/* Unable to connect to server  */
		fprintf(stderr, "Handshake failed with server @ %s port %hu\n",
			inet_ntoa(config.serv_addr), config.port);
		goto stcp_failure;
	}
	info("Connection established. Starting producer and consumer threads.\n");
	/* Start the consumer thread */
	/* Both threads use the same stcp structure */
	args.stcp = &stcp;
	args.seed = config.seed;
	args.mean = config.mean;
	if((rv = pthread_create(&ptid, &pattr, runConsumer, &args) != 0)) {
		error("pthread_create: %s\n", strerror(rv));
		goto stcp_failure;
	}
	/* Start Producing  */
	if(runProducer(&stcp) < 0) {
		error("Producer failed! Cancelling Consumer thread.\n");
		/* Cancel the Consumer thread */
		if((rv = pthread_cancel(ptid) != 0))
			error("pthread_cancel: %s\n", strerror(rv));
		/* wait for the consumer to cancel */
		if((rv = pthread_join(ptid, NULL)) < 0)
			error("pthread_join: %s\n", strerror(rv));
		goto stcp_failure;
	}
	/* wait for the consumer to finish reading all the data */
	if((rv = pthread_join(ptid, (void **)&exit_status)) < 0) {
		errno = rv;
		perror("main: pthread_join");
		goto stcp_failure;
	}
	debug("Consumer thread joined: thread exit status: %d\n", *exit_status);
	if(*exit_status < 0) {
		/* Consumer thread failed */
		error("Consumer thread failed!\n");
	}
	/* free thread's return status */
	free(exit_status);
	/* close the STCP socket */
	if(stcp_close(&stcp) < 0) {
		perror("main: stcp_close");
		exit(EXIT_FAILURE);
	}
	/* destroy pthread attributes */
	if(pthread_attr_destroy(&pattr)) {
		perror("main: pthread_attr_destroy");
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
	stcp_failure:
	/* close the STCP socket */
	stcp_close(&stcp);
	return EXIT_FAILURE;
}

bool chooseIPs(Config *config, struct in_addr *server_ip,
				struct in_addr *client_ip) {
	bool local = false;
	Interface *interfaces = NULL;
	Interface *temp = NULL;
	/* Get a list of our interfaces */
	interfaces = discoverInterfaces(NULL, DONT_BIND_INTERFACE);
	if(!size(interfaces)) {
		error("No interfaces were found. Aborting program.\n");
		destroy_interfaces(&interfaces);
		exit(EXIT_FAILURE);
	}
	/* Check if we match the server's address exactly */
	for(temp = interfaces; temp != NULL; temp = temp->next) {
		if(isSameIP(config->serv_addr, temp->ip_address)) {
			local = true;
			/* Set both IP's to 127.0.0.1 */
			info("IPclient and IPserver share the same IP: %s\n", temp->ip_address);
			info("Setting both IP's to be 127.0.0.1\n");
			client_ip->s_addr = htonl(INADDR_LOOPBACK);
			server_ip->s_addr = htonl(INADDR_LOOPBACK);
			break;
		}
	}
	if(!local) {
		unsigned long longest = 0; /* longest matching network mask in network order */
		Interface *long_if = NULL;
		/* Set the IPserver to the config file */
		server_ip->s_addr = config->serv_addr.s_addr;
		info("IPserver set to config file value: %s\n", inet_ntoa(*server_ip));
		/* Chose longest prefix */
		for(temp = interfaces; temp != NULL; temp = temp->next) {
			struct in_addr temp_ip, mask_ip;
			unsigned long ip_add, net_mask;
			inet_aton(temp->ip_address, &temp_ip);
			inet_aton(temp->network_mask, &mask_ip);
			ip_add = temp_ip.s_addr;
			net_mask = mask_ip.s_addr;

			if((ip_add & net_mask) == (server_ip->s_addr & net_mask)) {
				/* Server IP and this interface IP are on the same subnet */
				debug("IPclient and IPserver share subnet: %s/%d\n", temp->ip_address, bitsSet(net_mask));
				local = true;
				if(ntohl(net_mask) > ntohl(longest)) {
					longest = net_mask;
					long_if = temp;
					client_ip->s_addr = ip_add;
				}
			}
		}
		if(!local) {
			/* If NONE of the interfaces were local to the server choose any address */
			client_ip->s_addr = htonl(INADDR_ANY);
			info("No interfaces are local to IPserver, setting IPclient to arbitrary IP\n");
		} else {
			info("Chose longest prefix match with subnet: %s/%d. IPclient: %s\n",
				 long_if->ip_address, bitsSet(longest), long_if->ip_address);
		}
	}
	info("Server address %s local!\n", local? "is":"is NOT");

	/* Clean up memory */
	destroy_interfaces(&interfaces);
	return local;
}

int bitsSet(unsigned long mask) {
	unsigned long bit;
	int count = 0;
	for(bit = 1; bit != 0; bit <<= 1) {
		if(mask & bit){
			++count;
		}
	}
	return count;
}

int runProducer(struct stcp_sock *stcp) {
	int done;
	/* select stuff */
	struct timeval tv;
	long timeout = 30; /* lets use a 30 second timeout */
	int nfds;
	fd_set rset;

	/* Set the timeout */
	while(1) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		FD_ZERO(&rset);
		FD_SET(stcp->sockfd, &rset);
		nfds = stcp->sockfd + 1;
		if (select(nfds, &rset, NULL, NULL, &tv) < 0) {
			perror("stcp_connect: select");
			return -1;
		}
		if (FD_ISSET(stcp->sockfd, &rset)) {
			done = stcp_client_recv(stcp);
			if(done < 0) {
				/* some kind of error */
				return -1;
			} else if (done) {
				/* We got the FIN from the server and sent a FIN ACK */
				break;
			}
		} else {
			/* 30 second timeout reached on select and socket was not readable.
			 * If this happens we assume the server crashed or the network is down.
			 * TODO: Ask Badr about this case. He said we should only have one
			 *       timeout on the client side, but it seems silly to hang forever.
			 */
			error("Producer timed out after %ld seconds without receiving data\n", timeout);
			return -1;
		}
	}
	return 0;
}

void *runConsumer(void *arg) {
	struct consumer_args *args = arg;
    struct stcp_sock *stcp = args->stcp;
	unsigned int ms;
	int err, oldtype, oldstate, rv;
	int *retval;
	char read_buf[STCP_MAX_DATA * stcp->win.size];
	int nread;

	/* allocate space for our exit status */
	if((retval = malloc(sizeof(int))) == NULL) {
		error("Consumer: malloc failed\n");
		pthread_exit(NULL);
	}
	*retval = 0;

	/* Allow this thread to be cancelled at anytime */
	if((err = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0) {
		error("pthread_setcanceltype: %s\n", strerror(err));
		*retval = -1;
		pthread_exit(retval);
	}
	/* set the seed for our uniform uniformly distributed RNG */
	srand48(args->seed);
	while(1) {
		ms = sampleExpDist(args->mean);
		debug("Consumer: sleeping for %u ms\n", ms);
		if(usleep(ms * 1000) < 0) {
			perror("runConsumer: usleep");
		}
		/* Wake up and read from buffer. We do not want to be canceled here. */
		if((err = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate)) != 0) {
			error("pthread_setcanceltype: %s\n", strerror(err));
			*retval = -1;
			pthread_exit(retval);
		}
		/* Read data that the producer has buffered inorder */
		rv = stcp_client_read(stcp, read_buf, sizeof(read_buf), &nread);
		/* Now we can dump data to stdout */
		if(rv < 0) {
			/* some kind of error */
			*retval = -1;
			pthread_exit(retval);
		} else if(rv == 0) {
			/* EOF */
			break;
		}
		/* Reenable cancelability */
		if((err = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate)) != 0) {
			error("pthread_setcanceltype: %s\n", strerror(err));
			*retval = -1;
			pthread_exit(retval);
		}
	}
	pthread_exit(retval);
}


unsigned int sampleExpDist(unsigned int mean) {
	unsigned int usecs;
	usecs = mean * (-1 * log(drand48()));
	return usecs;
}
