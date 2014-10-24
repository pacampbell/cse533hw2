#include "stcp.h"

int stcp_socket(int sockfd, uint16_t rwin, struct stcp_sock *sock) {
	if(sockfd < 0) {
		debug("stcp_socket passed invalid sockfd\n");
		return -1;
	}
	if(rwin == 0) {
		debug("stcp_socket passed invalid rwin\n");
		return -1;
	}
	if(sock == NULL) {
		debug("stcp_socket passed invalid stcp_sock\n");
		return -1;
	}

	/* zero out the struct */
	memset(sock, 0, sizeof(struct stcp_sock));

	/* set the initial values for receiving */
	sock->sockfd = sockfd;
	/* Recv window size */
	sock->rwin.size = rwin;
	/* Allocate space for the recv window */
	sock->rwin.cbuf = calloc(rwin, sizeof(struct stcp_seg));
	if(sock->rwin.cbuf == NULL) {
		perror("stcp_socket: calloc");
		return -1;
	}
	return 0;
}

int stcp_close(struct stcp_sock *sock){
	/* free receive buffer */
	if(sock->rwin.cbuf != NULL) {
		free(sock->rwin.cbuf);
	}
	/* free send buffer */
	/* close socket */
	if(sock->sockfd >= 0){
		return close(sock->sockfd);
	}
	return 0;
}

int stcp_connect(struct stcp_sock *sock, struct sockaddr_in *serv_addr, char *file) {
	int len;
	uint16_t newport;
	struct stcp_pkt sent_pkt, recv_pkt, ack_pkt;
	/* 1s timeout for connect + Select stuff */
	int retries = 0, max_retries = 3, timeout = 1;
	struct timeval tv = {1L, 0L};
	int nfds;
	fd_set rset;

	/* init first SYN */
    sent_pkt.hdr.syn = htonl(0);
    sent_pkt.hdr.win = htons(sock->rwin.size);
    sent_pkt.hdr.flags = htons(STCP_SYN);
	/* Copy file to pkt data */
    sent_pkt.dlen = strlen(file);
	if(sent_pkt.dlen > STCP_MAX_DATA) {
		fprintf(stderr, "File name is too long! %s\n", file);
		return -1;
	}
	memcpy(sent_pkt.data, file, sent_pkt.dlen);
	/* attempt to connect backed by timeout */
	while (1) {
        printf("Sending connection request with %u second timeout\n", (uint32_t)tv.tv_sec);
		/* send first SYN containing the filename in the data field */
		len = send(sock->sockfd, &sent_pkt, sizeof(struct stcp_hdr) + sent_pkt.dlen, 0);
		if(len < 0) {
			perror("stcp_connect: send");
			return -1;
		} else if(len == 0) {
			fprintf(stderr, "stcp_connect: send failed to write any data\n");
			return -1;
		}
		FD_ZERO(&rset);
		FD_SET(sock->sockfd, &rset);
		nfds = sock->sockfd + 1;
		if(select(nfds, &rset, NULL, NULL, &tv) < 0) {
			perror("stcp_connect: select");
			return -1;
		}
		if(FD_ISSET(sock->sockfd, &rset)) {
			/* Read response from server */
			len = recv(sock->sockfd, &recv_pkt, STCP_MAX_SEG, 0);
			if(len < 0) {
				perror("stcp_connect: recv");
				return -1;
			} else if(len == 0) {
				fprintf(stderr, "stcp_connect: recv failed to read any data\n");
				return -1;
			}
			/* parse the new port from the server */
            ntoh_hdr(&recv_pkt.hdr);
            printf("Recv'd packet: ");
            print_hdr(&recv_pkt.hdr);
            if(recv_pkt.hdr.flags & STCP_ACK && recv_pkt.hdr.flags & STCP_SYN) {
                if(recv_pkt.hdr.ack == sent_pkt.hdr.syn + 1) {
                    newport = atoi(recv_pkt.data);
                    printf("New port received: %hu\n", newport);
                    /* break out out the loop */
                    break;
                }
            }
            printf("Malformed message SYN+ACK from server\n");
		}
		/* timeout reached! */
		retries++;
		/* double the timeout period */
		timeout <<= 1;
		/* Reset the timeout */
		tv.tv_sec = timeout;
		tv.tv_usec = 0L;
        printf("Connect timeout! ");
        if(retries > max_retries){
            printf("Aborting connection attempt.\n");
            return -1;
        }
	}
	/* connect to new port */
    serv_addr->sin_port = htons(newport);
    if(udpConnect(sock->sockfd, serv_addr) < 0) {
        return -1;
    }
	/* Send ACK */
    ack_pkt.hdr.syn = 0;
    ack_pkt.hdr.ack = recv_pkt.hdr.syn + 1;
    /* initial receiving window size */
    ack_pkt.hdr.win = sock->rwin.size;
    ack_pkt.hdr.flags = STCP_ACK;
    printf("Sending ACK to server ");
    print_hdr(&ack_pkt.hdr);
    /* convert to network order */
    hton_hdr(&ack_pkt.hdr);
    len = send(sock->sockfd, &ack_pkt.hdr, sizeof(struct stcp_hdr), 0);
    if(len < 0) {
        perror("stcp_connect: send");
        return -1;
    } else if(len == 0) {
        fprintf(stderr, "stcp_connect: send failed to write any data\n");
        return -1;
    }
	return 0;
}


int stcp_recv_send(struct stcp_sock *sock) {
    return -1;
}

void hton_hdr(struct stcp_hdr *hdr) {
    hdr->syn = htonl(hdr->syn);
    hdr->ack = htonl(hdr->ack);
    hdr->win = htons(hdr->win);
    hdr->flags = htons(hdr->flags);
}

void ntoh_hdr(struct stcp_hdr *hdr) {
    hdr->syn = ntohl(hdr->syn);
    hdr->ack = ntohl(hdr->ack);
    hdr->win = ntohs(hdr->win);
    hdr->flags = ntohs(hdr->flags);
}

void print_hdr(struct stcp_hdr *hdr) {
    printf("stcp_hdr{syn:%u, ack:%u, win:%hu, flags:%hu}\n",
            hdr->syn, hdr->ack, hdr->win, hdr->flags);
}
