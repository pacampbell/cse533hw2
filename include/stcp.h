/* Shitty TCP */
#ifndef STCP_H
#define STCP_H
/* stdlib headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
/* project headers */
#include "debug.h"
#include "utility.h"

/* constants */
#define STCP_MAX_SEG 512
#define STCP_HDR_SIZE 12
#define STCP_MAX_DATA 500

/* STCP structs */
/* Header */
struct stcp_hdr {
	uint32_t seq;		/* Sequence Number */
	uint32_t ack;		/* Acknowledgment Number */
	uint16_t win;		/* Window size (in datagram units) */
	uint16_t flags;		/* Flags */
#define STCP_FIN  0x01
#define STCP_SYN  0x02
#define STCP_ACK  0x04
};

/* STCP Packet */
struct stcp_pkt {
	struct stcp_hdr hdr;		/* Segment header	*/
	char data[STCP_MAX_DATA];	/* Segment data		*/
	int dlen;				/* Length of data */
};

/* STCP recv circular buffer entry */
struct stcp_seg {
	/* STCP Packet */
	struct stcp_pkt pkt;
	/* STCP Segment meta-data used to manage circular buffer */
	uint8_t valid;				/* Entry is valid this is set to 1
									on recv and 0 on read */

	/* For Receiving ??? */

	/* For Sending ??? */
	/* time sent and num retried */
};

/* STCP recv sliding window */
struct stcp_rwin {
	struct stcp_seg *cbuf;	/* (contiguous) circular window buffer 	*/
	uint16_t size;			/* length of receive window (#stcp_seg allocated)*/
	uint16_t adv;		    /* Current window size to advertise  */
	uint32_t next_seq;		/* Sequence Number we expect to write next */
	uint16_t write_head;	/* Index of the free entry to write
								the next SYN segment 				*/
	uint16_t read_head;		/* Index of the first unread entry		*/
};

/* This will be the 'new socket' used to send and recv segments */
struct stcp_sock {
	/* Real DG socket */
	int sockfd;
	/* TODO: MUTEX for concurrent sender/receiver access */
	/* I don't know what I'm doing */
	uint32_t recv_seq;
	/* For Receiving */
	uint16_t rwin_ad;	/* receive window size to advertise (in DG units) 	*/
	struct stcp_rwin rwin;

	/* For Sending */
	uint16_t swin;
};


/* convert packet from/to host order */
void hton_hdr(struct stcp_hdr *hdr);
void ntoh_hdr(struct stcp_hdr *hdr);
void print_hdr(struct stcp_hdr *hdr);
/* Builds a pkt in host order */
void build_pkt(struct stcp_pkt *pkt, uint32_t seq, uint32_t ack, uint16_t win,
		uint16_t flags, char *data, int dlen);

/**
* Test if a packet is a valid SYN ACK in response to a SYN. SYN and ACK
* flags should be set, ack # should be sent_seq + 1, and data field
* should contain a port.
*
* @param pkt The received pkt in host order
* @param sent_seq The starting sequence number we sent in our SYN
* @return 1 if valid, 0 if invalid
*/
int _valid_SYNACK(struct stcp_pkt *pkt, uint32_t sent_seq);

/*
 * Initialize a stcp_sock from the given socket. sockfd must haven been
 * created with createClientSocket or appropriately binded/connected to
 * a peer.
 *
 * @param sockfd A valid sockfd that has been created by createClientSocket
 * @param rwin   The maximum number of segments in the receiving window
 * @param sock   The stcp_sock to initialize
 * @return 0 on success or -1 on error.
 */
int stcp_socket(int sockfd, uint16_t rwin, struct stcp_sock *sock);

/*
 * Free all memory associated with this stcp socket and close the
 * underlying socket.
 *
 * @param sock  A stcp_sock initialized by stcp_socket
 * @return 0 on successful connection or -1 on error.
 */
int stcp_close(struct stcp_sock *sock);

/*
 * Initialize a stcp connection to the server using the 3 way handshake
 *
 * @param sock  A stcp_sock initialized by stcp_socket
 * @param file  The name of the file to download
 * @return 0 on successful connection or -1 on error.
 */
int stcp_connect(struct stcp_sock *sock, struct sockaddr_in *serv_addr, char *file);

int stcp_client_recv(struct stcp_sock *sock);

/**
 * Wrappers for send functions.
 *
 * @param pkt Must be in host order with the pkt->dlen set
 * @return Total # bytes sent, -1 on error check errno
 */
int send_pkt(int sockfd, struct stcp_pkt *pkt, int flags);
int sendto_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *dest_addr, socklen_t addrlen);
/**
* Wrappers for recv functions. Differs in the return value (see below)
*
* @param pkt Will be returned in host order
* @return -1: on system call error, check errno
*          0: if the packet is too small
*         >0: if packet is valid
*/
int recv_pkt(int sockfd, struct stcp_pkt *pkt, int flags);
int recvfrom_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen);

#endif
