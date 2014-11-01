#include "client.h"

int main(int argc, char *argv[]) {
	int fd, rv, err = 0;
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
	// Initialize pthread attributes
	if((rv = pthread_attr_init(&pattr)) != 0) {
		error("pthread_attr_init: %s\n", strerror(rv));
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
	if((rv = pthread_create(&ptid, &pattr, runConsumer, &args)) != 0) {
		error("pthread_create: %s\n", strerror(rv));
		goto stcp_failure;
	}
	/* destroy pthread attributes */
	if((rv = pthread_attr_destroy(&pattr)) != 0) {
		error("pthread_attr_destroy: %s\n", strerror(rv));
		goto stcp_failure;
	}
	/* Start Producing  */
	if(runProducer(&stcp) < 0) {
		error("Producer: failed to buffer entire file '%s' from %s:%u\n",
			config.filename,inet_ntoa(serv_addr.sin_addr),serv_addr.sin_port);
		/* Cancel the Consumer thread */
		if((rv = pthread_cancel(ptid)) != 0){
			error("pthread_cancel: %s\n", strerror(rv));
		}
		err = 1;
	} else {
		success("Producer: Buffered entire file '%s' from %s:%u\n",
			config.filename,inet_ntoa(serv_addr.sin_addr),serv_addr.sin_port);
	}
	info("Waiting on consumer thread...\n");
	/* wait for the consumer to finish reading all the data */
	if((rv = pthread_join(ptid, (void **)&exit_status)) < 0) {
		error("pthread_join: %s\n", strerror(rv));
		goto stcp_failure;
	}
	if(exit_status == NULL) {
		/* Consumer thread failed */
		error("Consumer: failed to read file from window.\n");
		goto stcp_failure;
	} else {
		success("Consumer: read %d byte file '%s' from window.\n",
				*exit_status, config.filename);
	}
	/* free thread's return status */
	free(exit_status);
	/* close the STCP socket */
	if(stcp_close(&stcp) < 0) {
		error("stcp_close: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(err)
		return EXIT_FAILURE;
	else
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
	/* Download speed statistics */
	int bbytes, total_bytes = 0;
	struct timeval tv_start;
	struct timeval tv_end;
	struct timeval tv_diff;
	double total_sec;
	/* select stuff */
	struct timeval tv;
	long timeout = 75; /* lets use a 75 second timeout */
	fd_set rset;
	/* Get a starting timestamp */
	if(gettimeofday(&tv_start, NULL) != 0) {
		error("Producer failed on gettimeofday: %s\n", strerror(errno));
		return -1;
	}
	/* Set the timeout */
	while(1) {
		int maxfd;
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		FD_ZERO(&rset);
		FD_SET(stcp->sockfd, &rset);
		maxfd = stcp->sockfd + 1;
		if (select(maxfd, &rset, NULL, NULL, &tv) < 0) {
			error("Producer failed on select: %s\n", strerror(errno));
			return -1;
		}
		if (FD_ISSET(stcp->sockfd, &rset)) {
			done = stcp_client_recv(stcp, &bbytes);
			total_bytes += bbytes;
			if(done < 0) {
				/* some kind of error */
				return -1;
			} else if (done) {
				/* We got the FIN from the server and sent a FIN ACK */
				break;
			}
		} else {
			/* 75 second timeout reached on select and socket was not readable.
			 * If this happens we assume the server crashed or the network is down.
			 * TODO: Ask Badr about this case. He said we should only have one
			 * timeout on the client side, but it seems silly to hang forever.
			 * And the max RTO on the server is 3 seconds with a max retransmit
			 * attempt of 12 so we could lower this to anything >36 seconds.
			 */
			error("Producer timed out after %ld seconds without receiving data\n", timeout);
			return -1;
		}
	}
	/* Get an ending timestamp */
	if(gettimeofday(&tv_end, NULL) != 0) {
		error("Producer failed on gettimeofday: %s\n", strerror(errno));
		return -1;
	}
	timeval_diff(&tv_start, &tv_end, &tv_diff);
	total_sec = (double)tv_diff.tv_sec + (double)tv_diff.tv_usec/1000000.0;
	success("Producer buffered %d byte file after %f sec\n", total_bytes, total_sec);
	success("Average download speed %f kB/s\n", ((double)total_bytes/1000.0)/total_sec);
	return 0;
}

void *runConsumer(void *arg) {
	struct consumer_args *args = arg;
    struct stcp_sock *stcp = args->stcp;
	int err, oldtype, oldstate;
	int *total_bytes;
	char read_buf[STCP_MAX_DATA * stcp->win.size];
	int nread;

	/* allocate space for our exit status */
	if((total_bytes = malloc(sizeof(int))) == NULL) {
		error("Consumer: malloc failed\n");
		pthread_exit(NULL);
	}
	*total_bytes = 0;

	/* Allow this thread to be canceled at anytime */
	if((err = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) != 0) {
		error("pthread_setcanceltype: %s\n", strerror(err));
		pthread_exit(NULL);
	}
	/* set the seed for our uniform uniformly distributed RNG */
	srand48(args->seed);
	while(1) {
		int rv;
		unsigned int ms;
		/* Sample from uniformly distributed RNG with mean from config */
		ms = args->mean * (-1 * log(drand48()));
		debug("Consumer: sleeping for %u ms\n", ms);
		if(usleep(ms * 1000) < 0) {
			error("Consumer usleep: %s\n", strerror(errno));
		}
		/* Wake up and read from buffer. We do not want to be canceled here. */
		if((err = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate)) != 0) {
			error("pthread_setcanceltype: %s\n", strerror(err));
			pthread_exit(NULL);
		}
		/* Read data that the producer has buffered in-order */
		rv = stcp_client_read(stcp, read_buf, sizeof(read_buf), &nread);
		if(rv < 0) {
			/* some kind of error */
			pthread_exit(NULL);
		} else if(rv == 0) {
			/* Consumer read EOF */
			*total_bytes += nread;
			break;
		}
		/* Increment the total # of bytes read */
		*total_bytes += nread;
		/* Re-enable cancelability */
		if((err = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate)) != 0) {
			error("pthread_setcanceltype: %s\n", strerror(err));
			pthread_exit(NULL);
		}
	}
	pthread_exit(total_bytes);
}

void timeval_diff(struct timeval *start, struct timeval *end, struct timeval *diff) {
	if(end->tv_usec > start->tv_usec) {
		/* subtract normally */
		diff->tv_sec = end->tv_sec - start->tv_sec;
		diff->tv_usec = end->tv_usec - start->tv_usec;
	} else {
		/* Carry from seconds to subtract */
		diff->tv_sec = end->tv_sec - start->tv_sec - 1;
		diff->tv_usec = 1000000L - (start->tv_usec - end->tv_usec);
	}
}
