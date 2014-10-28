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
			info("Sending connection request with %u second timeout\n", (uint32_t)tv.tv_sec);
			/* send first SYN containing the filename in the data field */
			printf("SYN pkt:");
			print_hdr(&sent_pkt.hdr);
			len = send_pkt(sock->sockfd, &sent_pkt, 0);
			if(len < 0) {
				error("Sending SYN packet: %s\n", strerror(errno));
				return -1;
			} else if(len == 0) {
				error("Sending SYN packet: failed to write any data\n");
				return -1;
			}
		}
		FD_ZERO(&rset);
		FD_SET(sock->sockfd, &rset);
		nfds = sock->sockfd + 1;
		if(select(nfds, &rset, NULL, NULL, &tv) < 0) {
			error("Select: %s", strerror(errno));
			return -1;
		}
		if(FD_ISSET(sock->sockfd, &rset)) {
			/* Read response from server */
			len = recv_pkt(sock->sockfd, &reply_pkt, 0);
			if(len < 0) {
				error("Receiving SYN+ACK packet: %s\n", strerror(errno));
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
				info("Received SYN+ACK with new port: %hu\n", newport);
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
	debug("Sending ACK to server ");
	print_hdr(&ack_pkt.hdr);
	/* Send ACK packet to server */
	len = send_pkt(sock->sockfd, &ack_pkt, 0);
	if(len < 0) {
		error("Sending ACK packet: %s\n", strerror(errno));
		return -1;
	} else if(len == 0) {
		error("Sending ACK packet: failed to write any data\n");
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
	struct stcp_pkt ack_pkt;
	uint16_t flags;
	Elem elem;

	/* default return 0 (we are not done) */
	done = 0;
	/* Aquire mutex */
	if((err = pthread_mutex_lock(&stcp->mutex)) != 0) {
		error("pthread_mutex_lock: %s\n", strerror(err));
		return -1;
	}

	/* Attempt to receive packet */
	len = recv_pkt(stcp->sockfd, &elem.pkt, 0);
	if(len < 0) {
		error("Server disconnected.\n");
		done = -1;
	} else if(len == 0) {
		fprintf(stderr, "stcp_connect: recv_pkt failed to read any data\n");
		done = -1;
	} else {
		Elem *added;

		/* TODO: Validate data packet? */
		info("Received packet: ");
		print_hdr(&elem.pkt.hdr);

		/* Buffer and shit */
		added = win_add_oor(&stcp->win, &elem);
		flags = STCP_ACK;
		if(added != NULL) {
			/* init ACK packet */
			if(added->pkt.hdr.flags & STCP_FIN) {
				info("Producer buffered FIN, sending FIN ACK.\n");
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
int stcp_client_read(struct stcp_sock *stcp, char *buf, int buflen, int *nread) {
	int err, eof, i;
	Window *win = &stcp->win;
	if(buf == NULL) {
		error("Invalid argument buf cannot be NULL\n");
		errno = EINVAL;
		return -1;
	}
	if(nread == NULL) {
		error("Invalid argument nread cannot be NULL\n");
		errno = EINVAL;
		return -1;
	}
	if(buflen < 0 || ((buflen % STCP_MAX_DATA) != 0)) {
		error("Invalid argument buflen: %d\n", buflen);
		errno = EINVAL;
		return -1;
	}
	/* Intialize the number of byte copied into buf */
	*nread = 0;
	eof = 0;
	/* Aquire mutex */
	if((err = pthread_mutex_lock(&stcp->mutex)) != 0) {
		error("pthread_mutex_lock: %s\n", strerror(err));
		return -1;
	}
	/* Try to read data from the window */
	while(!win_empty(win)) {
		Elem *elem = win_oldest(win);
		int dlen = elem->pkt.dlen;
		if(*nread + dlen <= buflen) {
			/* Copy data to buf */
			memcpy(buf+*nread, elem->pkt.data, dlen);
			*nread += dlen;
			/* check if the pkt was a FIN */
			if(elem->pkt.hdr.flags & STCP_FIN) {
				eof = 1;
			}
			/* remove the elem we just read */
			win_remove(win);
		}
	}
	if(*nread) {
		info("Consumer: printing %d bytes of inorder data:\n", *nread);
	} else {
		debug("Consumer: No data to read from buffer.\n");
	}
	for(i = 0; i < *nread; ++i) {
		/* Print out everything we copied into buf */
		putchar(buf[i]);
	}
	if (eof) {
		/* We read the last packet */
		putchar('\n');
		info("Consumer read EOF. Ending...\n");
	} else {
		if(*nread) {
			putchar('\n');
			info("Consumer printed %d bytes of inorder data.\n", *nread);
		}
	}
	/* Release mutex */
	if((err = pthread_mutex_unlock(&stcp->mutex)) != 0) {
		error("pthread_mutex_unlock: %s\n", strerror(err));
		return -1;
	}
	/* return 0 if read EOF else 1 */
	return eof? 0:1;
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

/*
 * Start Circle Buffer Functions
 */

int win_count(Window *win) {
	return win->count;
}

int win_available(Window *win) {
	/* Number of empty elems in the buffer */
	return (win->size - win->count);
}

int win_full(Window *win) {
	return (win->count == win->size);
}

int win_empty(Window *win) {
	return (win->count == 0);
}

Elem *win_add(Window *win, Elem *elem) {
	Elem *added = NULL;
	if(win->next_seq == elem->pkt.hdr.seq) {
		if(win_full(win)) {
			debug("Tried to add elem when the Window was full!\n");
		} else {
			/* Copy the elem into the ending index and mark it as valid */
			added = &win->buf[win->end];
			memcpy(added, elem, sizeof(Elem));
			elem->valid = 1;
			/* Advance the end index and increment our elem count */
			win->count += 1;
			win->end = (win->end + 1) % win->size;
			win->next_seq += 1;
		}
	} else {
		error("You tried to add the wrong seq: %u! Expected:%u!",
			elem->pkt.hdr.seq,win->next_seq);
	}
	return added;
}

void win_remove(Window *win) {
	/* remove the oldest elem in the window */
	if(win_empty(win)) {
		debug("Tried to remove elem when the Window was empty!\n");
	} else {
		/* Remove the elem at the start */
		win->buf[win->start].valid = 0;
		/* Advance the start index and decrement count */
		win->count -= 1;
		win->start = (win->start + 1) % win->size;
	}
}

Elem *win_oldest(Window *win) {
	Elem *oldest = NULL;
	if(win_empty(win)) {
		debug("Tried to get an elem when the Window was empty!\n");
	} else {
		oldest = &win->buf[win->start];
	}
	return oldest;
}

// Elem *win_newest(Window *win) {
// 	Elem *newest = NULL;
// 	if(win_empty(win)) {
// 		debug("Tried to get an elem when the Window was empty!\n");
// 	} else {
// 		newest = &win->buf[win->end];
// 	}
// 	return newest;
// }

Elem *win_end(Window *win) {
	return &win->buf[win->end];
}

void win_clear(Window *win) {
	/* remove everything from the window */
	while(!win_empty(win)) {
		win_remove(win);
	}
}

/**
 * Functions only for receiver side
 */

Elem *win_add_oor(Window *win, Elem *elem) {
	uint32_t seq = elem->pkt.hdr.seq;
	uint32_t fwdoff; /* fwd offset of elem seq */
	Elem *newelem = NULL;

	if(seq < win->next_seq) {
		fwdoff = win->next_seq - seq;
	} else {
		fwdoff = seq - win->next_seq;
	}

	/* see it we can fit this offset */
	if(fwdoff == 0) {
		Elem *temp;
		/* we can just use regular add */
		newelem = win_add(win, elem);
		/* TODO: advance past already buffered data */
		temp = win_end(win);
		while((temp->valid) && (temp->pkt.hdr.seq == win->next_seq)) {
			debug("Added missing seq: %u, moving window end to send cumulative ACK\n", win->next_seq - 1);
			/* Advance the end index and increment our elem count */
			win->count += 1;
			win->end = (win->end + 1) % win->size;
			win->next_seq += 1;
			temp = win_end(win);
		}

	} else if(fwdoff < win_available(win)) {
		Elem *newelem = win_get(win, fwdoff);
		if(newelem->valid) {
			debug("Recv window already buffered seq: %u\n", seq);
		} else {
			memcpy(newelem, elem, sizeof(Elem));
			newelem->valid = 1;
		}
	} else {
		/* no room in buffer */
		debug("Recv window full. Dropping packet\n");
	}

	return newelem;
}

Elem *win_get(Window *win, int index) {
	Elem *elem = NULL;
	if(index > win->size) {
		debug("Window index is too large.\n");
	} else if (index < 0) {
		debug("Window index cannot be negative.\n");
	} else {
		elem = &win->buf[index];
	}
	return elem;
}

/**
 * Functions only for sender side
 */

int win_send_limit(Window *win) {
	/* minimum of cwin, receiver advertised win, sender available window */
	int avail = win_available(win);
	if(win->cwin < avail) {
		return (win->cwin < win->rwin_adv)? win->cwin : win->rwin_adv;
	} else {
		return (avail < win->rwin_adv)? avail : win->rwin_adv;
	}
}
