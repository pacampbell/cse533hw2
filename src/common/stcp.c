#include "stcp.h"

int loss_thresh = 0;

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

int valid_pkt(struct stcp_pkt *pkt) {
	int valid = 1;

	if(pkt == NULL) {
		valid = 0;
	} else if(pkt->hdr.flags != (pkt->hdr.flags & (STCP_FIN|STCP_SYN|STCP_ACK))) {
		debug("Packet invalid: unknown flags: %hx\n", pkt->hdr.flags);
		valid = 0;
	} else if(pkt->dlen > STCP_MAX_DATA) {
		debug("Packet invalid: dlen too large: %d\n", pkt->dlen);
		valid = 0;
	}
	return valid;
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

int stcp_socket(int sockfd, uint16_t win_size, struct stcp_sock *stcp) {
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
	if(stcp == NULL) {
		error("stcp_socket passed NULL stcp_sock\n");
		return -1;
	}

	/* zero out the struct */
	memset(stcp, 0, sizeof(struct stcp_sock));

	stcp->sockfd = sockfd;

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
	if((err = pthread_mutex_init(&stcp->mutex, &attr)) != 0) {
		error("pthread_mutex_init: %s\n", strerror(err));
		return -1;
	}
	if ((err = pthread_mutexattr_destroy(&attr)) != 0) {
		error("pthread_mutexattr_destroy: %s\n", strerror(err));
		pthread_mutex_destroy(&stcp->mutex);
		return -1;
	}
	/* Init sliding window */
	if(win_init(&stcp->win, win_size, 0) < 0) {
		error("Failed to initialize sliding window.\n");
		pthread_mutex_destroy(&stcp->mutex);
		return -1;
	}
	return 0;
}

int stcp_close(struct stcp_sock *stcp){
	int err;
	/* Destroy the Producer/Consumer mutex */
	if((err = pthread_mutex_destroy(&stcp->mutex)) != 0) {
		error("pthread_mutex_destroy: %s\n", strerror(err));
		return -1;
	}
	/* cleanup window */
	win_destroy(&stcp->win);
	/* close socket */
	if(stcp->sockfd >= 0){
		return close(stcp->sockfd);
	}
	return 0;
}

int stcp_connect(struct stcp_sock *stcp, struct sockaddr_in *serv_addr, char *file) {
	int len, sendSYN;
	uint16_t newport;
	uint32_t start_seq = 0;
	uint32_t reply_seq; /* this is set to the seq # in the SYN ACK reply */
	struct stcp_pkt sent_pkt, reply_pkt, ack_pkt;
	/* 1s timeout for connect + Select stuff */
	int retries = 0, max_retries = 5, timeout = 1;
	struct timeval tv = {1L, 0L};
	int nfds;
	fd_set rset;

	/* init first SYN */
	sendSYN = 1;
	build_pkt(&sent_pkt, start_seq, 0, WIN_ADV(stcp->win), STCP_SYN, file, strlen(file));
	/* attempt to connect backed by timeout */
	while (1) {
		if(sendSYN) {
			info("Sending connection request with %u second timeout\n", (uint32_t)tv.tv_sec);
			/* send first SYN containing the filename in the data field */
			printf("SYN pkt:");
			print_hdr(&sent_pkt.hdr);
			len = send_pkt(stcp->sockfd, &sent_pkt, 0);
			if(len < 0) {
				error("Sending SYN packet: %s\n", strerror(errno));
				return -1;
			} else if(len == 0) {
				error("Sending SYN packet: failed to write any data\n");
				return -1;
			}
		}
		FD_ZERO(&rset);
		FD_SET(stcp->sockfd, &rset);
		nfds = stcp->sockfd + 1;
		if(select(nfds, &rset, NULL, NULL, &tv) < 0) {
			error("Select: %s", strerror(errno));
			return -1;
		}
		if(FD_ISSET(stcp->sockfd, &rset)) {
			/* Read response from server */
			len = recv_pkt(stcp->sockfd, &reply_pkt, 0);
			if(len < 0) {
				error("Recv from %s:%hu error: %s\n",
						inet_ntoa(serv_addr->sin_addr), ntohs(serv_addr->sin_port),
						strerror(errno));
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
				reply_seq = reply_pkt.hdr.seq;
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
	/* update our Receive window Seq */
	stcp->win.next_seq = reply_seq + 1;
	/* Reconnect to the new port */
	serv_addr->sin_port = htons(newport);
	if(udpConnect(stcp->sockfd, serv_addr) < 0) {
		return -1;
	}
	/* init ACK packet */
	build_pkt(&ack_pkt, 0, stcp->win.next_seq, WIN_ADV(stcp->win), STCP_ACK, NULL, 0);
	debug("Sending ACK to server ");
	print_hdr(&ack_pkt.hdr);
	/* Send ACK packet to server */
	len = send_pkt(stcp->sockfd, &ack_pkt, 0);
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
		error("Server disconnected during data transfer!\n");
		done = -1;
	} else if(len == 0) {
		/* The recv_pkt was invalid */
		done = 0;
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
				info("Producer: buffered FIN, sending FIN ACK.\n");
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
			error("Failed to send ACK handshake, send: %s", strerror(errno));
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
		/* Print out everything we copied into buf */
		for(i = 0; i < *nread; ++i) {
			putchar(buf[i]);
		}
		putchar('\n');
		if(eof) {
			info("Consumer: printed %d bytes of inorder data and read EOF. Ending...\n", *nread);
		} else {
			info("Consumer: printed %d bytes of inorder data.\n", *nread);
		}
	} else if(eof) {
		/* We read the last packet and no other data */
		info("Consumer: read EOF. Ending...\n");
	} else {
		debug("Consumer: No data to read from buffer.\n");
	}
	/* Release mutex */
	if((err = pthread_mutex_unlock(&stcp->mutex)) != 0) {
		error("pthread_mutex_unlock: %s\n", strerror(err));
		return -1;
	}
	/* return 0 if read EOF else 1 */
	return eof? 0:1;
}


void client_set_loss(unsigned int seed, double loss) {
	/* set the loss threshhold value for Sending AND receiving */
	warn("Setting send/recv drop rate to %f%%\n", loss);
	loss_thresh = loss * RAND_MAX;
	/* set seed for RNG */
	srand(seed);
}

int sendto_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *dest_addr, socklen_t addrlen) {
	int rv;
	/* DROP */
	if(rand() < loss_thresh) {
		warn("Artificially dropped on sendto ");
		print_hdr(&pkt->hdr);
		return sizeof(struct stcp_hdr) + pkt->dlen;
	}
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
	/* DROP */
	if(rand() < loss_thresh) {
		warn("Artificially dropped on send ");
		print_hdr(&pkt->hdr);
		return sizeof(struct stcp_hdr) + pkt->dlen;
	}
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
	/* DROP */
	if(rand() < loss_thresh) {
		warn("Artificially dropped on recvfrom ");
		print_hdr(&pkt->hdr);
		return 0;
	}
	/* set data length */
	pkt->dlen = rv - sizeof(struct stcp_hdr);
	if(rv > 0) {
		if(rv < sizeof(struct stcp_hdr) || rv > STCP_MAX_SEG) {
			/* Received datagram is too small or too large to be STCP */
			rv = 0;
		} else {
			/* might be valid */
			rv = valid_pkt(pkt);
		}
	}
	return rv;
}

int recv_pkt(int sockfd, struct stcp_pkt *pkt, int flags) {
	int rv;
	rv = recv(sockfd, pkt, STCP_MAX_SEG, flags);
	/* convert stcp_hdr into host order */
	ntoh_hdr(&pkt->hdr);
	/* DROP */
	if(rand() < loss_thresh) {
		warn("Artificially dropped on recv ");
		print_hdr(&pkt->hdr);
		return 0;
	}
	/* set data length */
	pkt->dlen = rv - sizeof(struct stcp_hdr);
	if(rv > 0) {
		if(rv < sizeof(struct stcp_hdr) || rv > STCP_MAX_SEG) {
			/* Received datagram is too small or too large to be STCP */
			rv = 0;
		} else {
			/* might be valid */
			rv = valid_pkt(pkt);
		}
	}
	return rv;
}

/*
 * Start Circle Buffer Functions
 */
int win_init(Window *win, int win_size, uint32_t initial_seq) {
	/* sliding window size */
	win->size = win_size;
	win->next_seq = initial_seq;
	win->next_ack = initial_seq;
	win->in_flight = 0;
	/* Slow Start values */
	win->cwnd = 1;
	win->ssthresh = 65535; /* Max value initially */
	/* Allocate space for the receiving window */
	win->buf = calloc(win_size, sizeof(Elem));
	if(win->buf == NULL) {
		error("calloc: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

void win_destroy(Window *win) {
	if(win->buf != NULL) {
		free(win->buf);
	}
}

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
			elem->pkt.hdr.seq, win->next_seq);
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

Elem *win_get(Window *win, int fwdoff) {
	Elem *elem = NULL;
	if(fwdoff > win->size) {
		debug("Window fwdoff is too large.\n");
	} else if (fwdoff < 0) {
		debug("Window fwdoff cannot be negative.\n");
	} else {
		elem = &win->buf[(win->end + fwdoff) % win->size];
	}
	return elem;
}

/**
 * Functions only for sender side
 */

int win_send_limit(Window *win) {
	/* minimum of cwnd, receiver advertised win, sender available window */
	int avail = win_available(win);
	if(win->cwnd < avail) {
		return (win->cwnd < win->rwin_adv)? win->cwnd : win->rwin_adv;
	} else {
		return (avail < win->rwin_adv)? avail : win->rwin_adv;
	}
}

/* TODO: what if called twice at EOF? */
int win_buffer_elem(Window *win, int fd) {
	int rlen;
	char buffer[STCP_MAX_DATA];
	Elem newelem;
	/* read data from file */
	if((rlen = read(fd, buffer, sizeof(buffer))) > 0) {
		/* build the elem to go in the window */
		build_pkt(&newelem.pkt, win->next_seq, 0, 0, 0, buffer, rlen);
		debug("Read %d bytes. Built data Element.\n", rlen);
	} else if(rlen == 0) {
		/* Read the end of the file, build the FIN packet */
		build_pkt(&newelem.pkt, win->next_seq, 0, 0, STCP_FIN, NULL, 0);
		debug("Read EOF. Built FIN Element.\n");
	} else {
		/* Error */
		error("Fatal error when reading from file: %s\n", strerror(errno));
		return -1;
	}
	/* now that we built the Elem, add it to the window */
	if(win_add(win, &newelem) == NULL) {
		error("Fatal error: Tried to buffer data but window was full\n");
		return -1;
	}
	return rlen;
}

Elem *win_get_index(Window *win, int startoff) {
	Elem *elem = NULL;
	if(startoff > win->size) {
		warn("Window startoff %d is too large.\n", startoff);
	} else if(startoff < 0) {
		warn("Window startoff %d cannot be negative.\n", startoff);
	} else if(startoff >= win->count) {
		warn("Window startoff %d cannot be greater than window count %d.\n", startoff, win->count);
	} else {
		elem = &win->buf[(win->start + startoff) % win->size];
	}
	return elem;
}

int win_valid_ACK(Window *win, struct stcp_pkt *pkt) {
	int valid = 0;

	if(pkt != NULL) {
		if((pkt->hdr.flags & STCP_ACK)
			&& (pkt->hdr.ack >= win->next_ack)
			&& (pkt->hdr.ack <= (win->next_ack + win->count))) {
			valid = 1;
		}
	}
	return valid;
}

int win_remove_ack(Window *win, uint32_t ack) {
	int count = 0;
	if(win_empty(win)) {
		warn("Window: tried to ack but window was empty\n");
	} else {
		Elem *elem = win_oldest(win);
		while(elem != NULL && (elem->pkt.hdr.seq < ack)) {
			win_remove(win);
			++count;
			elem = win_oldest(win);
		}
	}
	return count;
}
