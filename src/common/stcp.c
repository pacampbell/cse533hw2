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
	printf(KWHT "seq:%u, ack:%u, win:%hu, flags: %s%s%s\b\n" KNRM, hdr->seq, hdr->ack,
			hdr->win,
			(hdr->flags & STCP_FIN)? "FIN " : "",
			(hdr->flags & STCP_SYN)? "SYN " : "",
			(hdr->flags & STCP_ACK)? "ACK " : "");
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

int stcp_socket(int sockfd, uint16_t win_size, struct stcp_sock *sock) {
	int err;
	pthread_mutexattr_t attr;

	if(sockfd < 0) {
		error("stcp_socket invalid sockfd: %d\n", sockfd);
		return -1;
	}
	if(win_size == 0) {
		error("stcp_socket invalid win_size: %hu\n", win_size);
		return -1;
	}
	if(sock == NULL) {
		error("stcp_socket passed NULL stcp_sock\n");
		return -1;
	}

	/* zero out the struct */
	memset(sock, 0, sizeof(struct stcp_sock));

	sock->sockfd = sockfd;

	/* Use error checking mutex :) */
	if ((err = pthread_mutexattr_init(&attr)) != 0) {
		error("pthread_mutexattr_init: %s\n", strerror(err));
		return -1;
	}
	if ((err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) != 0) {
		error("pthread_mutexattr_settype: %s\n", strerror(err));
		return -1;
	}
	/* Initialize the Producer/Consumer mutex */
	if((err = pthread_mutex_init(&sock->mutex, &attr)) != 0) {
		error("pthread_mutex_init: %s\n", strerror(err));
		return -1;
	}
	if ((err = pthread_mutexattr_destroy(&attr)) != 0) {
		error("pthread_mutexattr_destroy: %s\n", strerror(err));
		pthread_mutex_destroy(&sock->mutex);
		return -1;
	}
	/* sliding window size */
	sock->win.size = win_size;
	/* Allocate space for the receiving window */
	sock->win.buf = calloc(win_size, sizeof(Elem));
	if(sock->win.buf == NULL) {
		error("calloc: %s\n", strerror(errno));
		pthread_mutex_destroy(&sock->mutex);
		return -1;
	}
	return 0;
}

int stcp_close(struct stcp_sock *sock){
	int err;
	/* Destroy the Producer/Consumer mutex */
	if((err = pthread_mutex_destroy(&sock->mutex)) != 0) {
		error("pthread_mutex_destroy: %s\n", strerror(err));
		return -1;
	}
	/* free sliding window */
	if(sock->win.buf != NULL) {
		free(sock->win.buf);
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
	build_pkt(&sent_pkt, start_seq, 0, WIN_ADV(sock->win), STCP_SYN, file, strlen(file));
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
				/* update the initial seq */
				sock->win.next_seq = reply_pkt.hdr.seq + 1;
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
	build_pkt(&ack_pkt, 0, sock->win.next_seq, WIN_ADV(sock->win), STCP_ACK, NULL, 0);
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
int stcp_client_recv(struct stcp_sock *stcp) {
	int len, err, done;
	struct stcp_pkt data_pkt, ack_pkt;
	uint16_t flags;

	/* default return 0 (we are not done) */
	done = 0;
	/* Aquire mutex */
	if((err = pthread_mutex_lock(&stcp->mutex)) != 0) {
		error("pthread_mutex_lock: %s\n", strerror(err));
		return -1;
	}

	/* Attempt to receive packet */
	len = recv_pkt(stcp->sockfd, &data_pkt, 0);
	if(len < 0) {
		error("Server disconnected.\n");
		done = -1;
	} else if(len == 0) {
		fprintf(stderr, "stcp_connect: recv_pkt failed to read any data\n");
		done = -1;
	} else {
		int seq_index;
		/* Valid data packet */
		info("data_pkt: ");
		print_hdr(&data_pkt.hdr);

		/* Buffer and shit */
		seq_index = data_pkt.hdr.seq - stcp->win.next_seq;
		if(seq_index >=0 && seq_index < WIN_ADV(stcp->win)) {
			/* It can fit in our buffer */
			Elem *elem = &stcp->win.buf[(stcp->win.end + seq_index) % stcp->win.size];
			if(elem->valid) {
				debug("Window already contains this seq\n");
			} else {
				/* append into buffer */
				debug("Appending pkt into window\n");
				memcpy(&elem->pkt, &data_pkt, sizeof(data_pkt));
				elem->valid = 1;
				if(seq_index == 0){
					debug("Advancing index of last elem in window\n");
					stcp->win.end = (stcp->win.end + 1) % stcp->win.size;
					stcp->win.count += 1;
					stcp->win.next_seq += 1;
				}
			}
			/* init ACK packet */
			flags = STCP_ACK;
			if(data_pkt.hdr.flags & STCP_FIN) {
				info("Producer received FIN, sending FIN ACK.\n");
				flags |= STCP_FIN;
				/* set done to 1 to indicate we sent the FIN ACK */
				done = 1;
			}
		}

		/* TODO update stcp->next_seq */
		build_pkt(&ack_pkt, 0, stcp->win.next_seq, WIN_ADV(stcp->win), flags, NULL, 0);
		info("Sending ACK: ");
		print_hdr(&ack_pkt.hdr);
		/* Send ACK packet to server */
		len = send_pkt(stcp->sockfd, &ack_pkt, 0);
		if(len < 0) {
			perror("stcp_connect: send_pkt");
			done = -1;
		} else if(len == 0) {
			error("send_pkt failed to write any data\n");
			done = -1;
		}
	}

	/* Release mutex */
	if((err = pthread_mutex_unlock(&stcp->mutex)) != 0) {
		error("pthread_mutex_unlock: %s\n", strerror(err));
		return -1;
	}
	return done;
}

/**
 * This is called when the client wakes up.
 */
int stcp_client_read(struct stcp_sock *stcp) {
	int err, done = 0, printed = 0;
	Elem *elem = NULL;
	/* Aquire mutex */
	if((err = pthread_mutex_lock(&stcp->mutex)) != 0) {
		error("pthread_mutex_lock: %s\n", strerror(err));
		return -1;
	}
	/* while buff is readable */
	while(stcp->win.count > 0) {
		elem = &stcp->win.buf[stcp->win.start];
		if(elem->valid) {
			int i;
			if(!printed) {
				info("Consumer reading buffered data:\n");
				printed = 1;
			}
			/* dump data to stdout */
			for(i = 0; i < elem->pkt.dlen; ++i){
				putchar(elem->pkt.data[i]);
			}
			/* Advance the read head */
			elem->valid = 0;
			stcp->win.count -= 1;
			stcp->win.start = (stcp->win.start + 1) % stcp->win.size;
			/* check if the pkt was a FIN */
			if(elem->pkt.hdr.flags & STCP_FIN) {
				printf("\n");
				info("Consumer read end of file data. Ending...\n");
				done = 1;
				break;
			} else {
				done = 0;
			}

		} else {
			error("Count > 0 but circle buff elem is not valid!\n");
			done = -1;
			break;
		}
	}
	if(printed && !done) {
		printf("\n");
		info("Consumer printed current inorder data.\n");
		printed = 1;
	}
	/* Release mutex */
	if((err = pthread_mutex_unlock(&stcp->mutex)) != 0) {
		error("pthread_mutex_unlock: %s\n", strerror(err));
		return -1;
	}
	return done;
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
