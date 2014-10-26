#include "stcp.h"

void build_pkt(struct stcp_pkt *pkt, uint32_t seq, uint32_t ack, uint16_t win,
				uint16_t flags, void *data, int dlen) {
	pkt->hdr.seq = seq;
	pkt->hdr.ack = ack;
	pkt->hdr.win = win;
	pkt->hdr.flags = flags;
	/* make sure dlen and data is valid */
	dlen = (dlen > STCP_MAX_DATA)? STCP_MAX_DATA : dlen;
	if(dlen > 0 && data != NULL) {
		pkt->dlen = dlen;
		memcpy(pkt->data, data, dlen);
	} else {
		pkt->dlen = 0;
	}
}

void hton_hdr(struct stcp_hdr *hdr) {
	hdr->seq = htonl(hdr->seq);
	hdr->ack = htonl(hdr->ack);
	hdr->win = htons(hdr->win);
	hdr->flags = htons(hdr->flags);
}

void ntoh_hdr(struct stcp_hdr *hdr) {
	hdr->seq = ntohl(hdr->seq);
	hdr->ack = ntohl(hdr->ack);
	hdr->win = ntohs(hdr->win);
	hdr->flags = ntohs(hdr->flags);
}

void print_hdr(struct stcp_hdr *hdr) {
	printf(" seq:%u, ack:%u, win:%hu, flags:%s %s %s\n", hdr->seq, hdr->ack,
			hdr->win,
			(hdr->flags & STCP_FIN)? "FIN" : "",
			(hdr->flags & STCP_SYN)? "SYN" : "",
			(hdr->flags & STCP_ACK)? "ACK" : "");
}

int _valid_SYNACK(struct stcp_pkt *pkt, uint32_t sent_seq) {
	if(!(pkt->hdr.flags & (STCP_SYN | STCP_ACK))) {
		error("Packet flags: SYN and ACK not set!\n");
		return 0;
	}
	if(pkt->hdr.ack != sent_seq + 1){
		error("Packet ack: %u, expected ack: %u!\n", pkt->hdr.ack, sent_seq+1);
		return 0;
	}
	if(pkt->dlen <= 0){
		error("Packet data: port not present!\n");
		return 0;
	} else if (pkt->dlen != 2){
		error("Packet data: port not valid, expected 2 bytes, got %d!\n",
				pkt->dlen);
		return 0;
	}
	return 1;
}

int stcp_socket(int sockfd, uint16_t rwin, uint16_t swin,
				struct stcp_sock *sock) {
	int err;
	if(sockfd < 0) {
		error("stcp_socket invalid sockfd: %d\n", sockfd);
		return -1;
	}
	if(rwin == 0 && swin == 0) {
		error("stcp_socket rwin and swin cannot both be 0.\n");
		return -1;
	}
	if(sock == NULL) {
		error("stcp_socket passed NULL stcp_sock\n");
		return -1;
	}

	/* zero out the struct */
	memset(sock, 0, sizeof(struct stcp_sock));

	sock->sockfd = sockfd;
	/* Initialize the Producer/Consumer mutex */
	if((err = pthread_mutex_init(&sock->mutex, NULL)) != 0) {
		error("stcp_socket: pthread_mutex_init: %s", strerror(err));
		return -1;
	}
	/* receiving window size */
	sock->recv_win.size = rwin;
	/* sending window size */
	sock->send_win.size = swin;
	/* Allocate space for the receiving window */
	if(rwin != 0) {
		sock->recv_win.buf = calloc(rwin, sizeof(Elem));
		if(sock->recv_win.buf == NULL) {
			error("stcp_socket: calloc: %s", strerror(errno));
			return -1;
		}
	} else {
		sock->recv_win.buf = NULL;
	}
	/* Allocate space for the sending window */
	if(swin != 0) {
		sock->send_win.buf = calloc(swin, sizeof(Elem));
		if(sock->send_win.buf == NULL) {
			error("stcp_socket: calloc: %s", strerror(errno));
			return -1;
		}
	} else {
		sock->send_win.buf = NULL;
	}
	return 0;
}

int stcp_close(struct stcp_sock *sock){
	int err;
	/* Destroy the Producer/Consumer mutex */
	if((err = pthread_mutex_destroy(&sock->mutex)) != 0) {
		error("stcp_socket: pthread_mutex_destroy: %s", strerror(err));
		return -1;
	}
	/* free receive buffer */
	if(sock->recv_win.buf != NULL) {
		free(sock->recv_win.buf);
	}
	/* free send buffer */
	if(sock->send_win.buf != NULL) {
		free(sock->send_win.buf);
	}
	/* close socket */
	if(sock->sockfd >= 0){
		return close(sock->sockfd);
	}
	return 0;
}

int stcp_connect(struct stcp_sock *sock, struct sockaddr_in *serv_addr, char *file) {
	int len, sendSYN;
	uint16_t newport;
	uint32_t start_seq = 0;
	struct stcp_pkt sent_pkt, reply_pkt, ack_pkt;
	/* 1s timeout for connect + Select stuff */
	int retries = 0, max_retries = 5, timeout = 1;
	struct timeval tv = {1L, 0L};
	int nfds;
	fd_set rset;

	/* init first SYN */
	sendSYN = 1;
	build_pkt(&sent_pkt, start_seq, 0, RWIN_ADV(sock->recv_win), STCP_SYN, file, strlen(file));
	/* attempt to connect backed by timeout */
	while (1) {
		if(sendSYN) {
			printf("Sending connection request with %u second timeout\n", (uint32_t)tv.tv_sec);
			/* send first SYN containing the filename in the data field */
			printf("SYN pkt:");
			print_hdr(&sent_pkt.hdr);
			len = send_pkt(sock->sockfd, &sent_pkt, 0);
			if(len < 0) {
				perror("stcp_connect: send_pkt");
				return -1;
			} else if(len == 0) {
				fprintf(stderr, "stcp_connect: send_pkt failed to write any data\n");
				return -1;
			}
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
			len = recv_pkt(sock->sockfd, &reply_pkt, 0);
			if(len < 0) {
				perror("stcp_connect: recv_pkt");
				return -1;
			} else if(len == 0) {
				fprintf(stderr, "stcp_connect: recv_pkt failed to read any data\n");
				return -1;
			}
			/* parse the new port from the server */
			debug("Recv'd packet: ");
			print_hdr(&reply_pkt.hdr);
			/* determine if it was a valid connection reply */
			if(_valid_SYNACK(&reply_pkt, start_seq)) {
				uint16_t *p = (uint16_t *)reply_pkt.data;
				/* port should be the only 2 bytes of data in host order */
				newport = ntohs(*p);
				info("New port received: %hu\n", newport);
				/* break out out the loop */
				break;
			} else {
				/* Dont't resend the SYN just wait for a valid response */
				sendSYN = 0;
			}
		} else {
			/* timeout reached! */
			retries++;
			/* double the timeout period */
			timeout <<= 1;
			/* Reset the timeout */
			tv.tv_sec = timeout;
			tv.tv_usec = 0L;
			printf("Connect timeout! ");
			if (retries > max_retries) {
				printf("Aborting connection attempt.\n");
				return -1;
			}
			/* resend the SYN because we timed out */
			sendSYN = 1;
		}
	}
	/* Reconnect to the new port */
	serv_addr->sin_port = htons(newport);
	if(udpConnect(sock->sockfd, serv_addr) < 0) {
		return -1;
	}
	/* init ACK packet */
	build_pkt(&ack_pkt, 0, reply_pkt.hdr.seq + 1, RWIN_ADV(sock->recv_win), STCP_ACK, NULL, 0);
	printf("Sending ACK to server ");
	print_hdr(&ack_pkt.hdr);
	/* Send ACK packet to server */
	len = send_pkt(sock->sockfd, &ack_pkt, 0);
	if(len < 0) {
		perror("stcp_connect: send_pkt");
		return -1;
	} else if(len == 0) {
		fprintf(stderr, "stcp_connect: send_pkt failed to write any data\n");
		return -1;
	}
	return 0;
}

/**
 * This should only be called when the underlying socket is readable.
 * Attempt to recv an STCP packet P and send an appropriate ACK.
 * 1. If seq < expected, discard P and send duplicate ACK
 * 2. If seq == expected, buffer P and send cumulative ACK
 * 3. If seq > expected, buffer(if space is avail) P and send duplicate ACK
 */
int stcp_client_recv(struct stcp_sock *sock) {
	int len;
	struct stcp_pkt data_pkt, ack_pkt;

	/* Attempt to receive packet */
	len = recv_pkt(sock->sockfd, &data_pkt, 0);
	if(len < 0) {
		perror("stcp_client_recv: recv_pkt");
		return -1;
	} else if(len == 0) {
		fprintf(stderr, "stcp_connect: recv_pkt failed to read any data\n");
		return -1;
	}
	info("data_pkt: ");
	print_hdr(&data_pkt.hdr);
	/* Buffer and shit */

	/* init ACK packet */
	build_pkt(&ack_pkt, 0,sock->next_seq, RWIN_ADV(sock->recv_win), STCP_ACK, NULL, 0);
	info("Sending ACK: ");
	print_hdr(&ack_pkt.hdr);
	/* Send ACK packet to server */
	len = send_pkt(sock->sockfd, &ack_pkt, 0);
	if(len < 0) {
		perror("stcp_connect: send_pkt");
		return -1;
	} else if(len == 0) {
		fprintf(stderr, "stcp_connect: send_pkt failed to write any data\n");
		return -1;
	}
	return 0;
}

/**
 * This is called when the client wakes up.
 */
int stcp_client_read(struct stcp_sock *sock) {
	warn("stcp_client_read not yet implemented!\n");
	return -1;
}

int sendto_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *dest_addr, socklen_t addrlen) {
	int rv;
	/* convert stcp_hdr into network order */
	hton_hdr(&pkt->hdr);
	rv = sendto(sockfd, pkt, sizeof(struct stcp_hdr) + pkt->dlen, flags,
				dest_addr, addrlen);
	/* convert stcp_hdr back into host order */
	ntoh_hdr(&pkt->hdr);
	return rv;
}

int send_pkt(int sockfd, struct stcp_pkt *pkt, int flags) {
	int rv;
	/* convert stcp_hdr into network order */
	hton_hdr(&pkt->hdr);
	rv = send(sockfd, pkt, sizeof(struct stcp_hdr) + pkt->dlen, flags);
	/* convert stcp_hdr back into host order */
	ntoh_hdr(&pkt->hdr);
	return rv;
}

int recvfrom_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen) {
	int rv;
	rv = recvfrom(sockfd, pkt, STCP_MAX_SEG, flags, src_addr, addrlen);
	/* convert stcp_hdr into host order */
	ntoh_hdr(&pkt->hdr);
	/* set data length */
	pkt->dlen = rv - sizeof(struct stcp_hdr);
	if(rv > 0 && rv < sizeof(struct stcp_hdr)) {
		rv = 0;
	}
	return rv;
}

int recv_pkt(int sockfd, struct stcp_pkt *pkt, int flags) {
	int rv;
	rv = recv(sockfd, pkt, STCP_MAX_SEG, flags);
	/* convert stcp_hdr into host order */
	ntoh_hdr(&pkt->hdr);
	/* set data length */
	pkt->dlen = rv - sizeof(struct stcp_hdr);
	if(rv > 0 && rv < sizeof(struct stcp_hdr)) {
		rv = 0;
	}
	return rv;
}
