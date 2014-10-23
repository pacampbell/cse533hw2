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

int stcp_connect(struct stcp_sock *sock, char *file) {
	int len;
	uint16_t newport;
	struct stcp_pkt pkt;
	/* init first SYN */
	pkt.hdr.syn = htonl(0);
	pkt.hdr.win = htons(sock->rwin.size);
	pkt.hdr.flags = htons(STCP_SYN);
	/* Copy file to pkt data */
	pkt.dlen = strlen(file);
	if(pkt.dlen > STCP_MAX_DATA) {
		fprintf(stderr, "File name is too long! %s\n", file);
		return -1;
	}
	memcpy(pkt.data, file, pkt.dlen);
	/* send first SYN containing the filename in the data field */
	len = send(sock->sockfd, &pkt, sizeof(struct stcp_hdr) + pkt.dlen, 0);
	if(len < 0) {
		perror("stcp_connect: send");
		return -1;
	}
	if(len == 0) {
		fprintf(stderr, "Error: send failed to write any data\n");
		return -1;
	}
	/* wait for response from server */
	len = recv(sock->sockfd, &pkt, STCP_MAX_SEG, 0);
	if(len < 0) {
		perror("stcp_connect: recv");
		return -1;
	}
	if(len == 0) {
		fprintf(stderr, "Error: recv failed to read any data\n");
		return -1;
	}
	/* parse the new port from the server */
	newport = atoi(pkt.data);
	printf("New port received: %hu\n", newport);
	/* connect to new port */

	/* Send ACK */
	return -1;
}
