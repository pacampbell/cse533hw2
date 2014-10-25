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
    /* Start the consumer thread */
    /* Both threads use the same stcp structure */
    args.stcp = &stcp;
    args.seed = config.seed;
    args.mean = config.mean;
    pthread_create(&ptid, &pattr, runConsumer, &args);
    /* Start Producing  */
    if(runProducer(&stcp) < 0) {
        fprintf(stderr, "runProducer failed!");
        goto stcp_failure;
    }
    /* wait for the consumer to finish reading all the data */
    /* NULL so dont recv exit status */
    if((rv = pthread_join(ptid, NULL)) < 0) {
        errno = rv;
        perror("main: pthread_join");
        goto stcp_failure;
    }
    printf("Consumer thread joined");
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

int runProducer(struct stcp_sock *stcp) {
    /* select stuff */
    struct timeval tv;
    int nfds;
    fd_set rset;

    /* Set the timeout */
    while(1) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&rset);
        FD_SET(stcp->sockfd, &rset);
        nfds = stcp->sockfd + 1;
        if (select(nfds, &rset, NULL, NULL, &tv) < 0) {
            perror("stcp_connect: select");
            return -1;
        }
        if (FD_ISSET(stcp->sockfd, &rset)) {
            printf("Must call recv_send_ack.\n");
            /* if is FIN, send ACK and break */
        } else {
            /* timeout reached on select */
            printf("Producer select timeout.\n");
        }
    }
    return 0;
}


void *runConsumer(void *arg) {
    struct consumer_args *args = arg;
  //  struct stcp_sock *stcp = args->stcp;
    unsigned int ms;
    /* set the seed for our uniform uniformly distributed RNG */
    srand48(args->seed);
    while(1) {
        ms = sampleExpDist(args->mean);
        debug("Consumer: sleeping for %u ms\n", ms);
        if(usleep(ms * 1000) < 0) {
            perror("runConsumer: usleep");
        }
        /* Wake up and read from buffer */
    }
	return 0;
}


unsigned int sampleExpDist(unsigned int mean) {
    unsigned int usecs;
    usecs = mean * (-1 * log(drand48()));
    return usecs;
}