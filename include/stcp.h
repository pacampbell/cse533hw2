/* Shitty TCP */
#ifndef STCP_H
#define STCP_H
/* stdlib headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
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
	int dlen;					/* Length of data */
};

/* STCP circular buffer entry */
typedef struct {
	/* Stuff common to both send/recv window elems */
	uint8_t valid;			/* Entry is valid this is set to 1
								on recv and 0 on read */
	struct stcp_pkt pkt;	/* Packet to buffer */
	/* stuff only for sending window element */
	/* TODO: add rtt_info and other stuff */
} Elem;

/* STCP recv sliding window */
typedef struct {
	Elem *buf;			/* (contiguous) buffer space for size Elems */
	uint16_t size;		/* length of receive window (#Elem allocated) */
	uint16_t count;		/* # of elements in buffer */
	uint16_t end;		/* Index of the next free elem		*/
	uint16_t start;		/* Index of the oldest elem		*/
	/* stuff specifically for receiving window */
} Cbuf;

/* This will be the 'new socket' used to send and recv segments */
struct stcp_sock {
	int sockfd;			/* The connected UDP socket */

	/* For Receiving */
	pthread_mutex_t mutex;
	uint32_t next_seq;	/* Sequence Number we expect to recv next */
	Cbuf recv_win;

	/* For Sending */
	uint32_t next_ack;	/* Acknowledgement Number we expect to recv next */
	Cbuf send_win;
	uint16_t cwin;		/* Congestion window value */
	uint16_t ssthresh;	/* Slow Start Threshhold */
	// TODO: other shit for sending
};


/* convert packet from/to host order */
void hton_hdr(struct stcp_hdr *hdr);
void ntoh_hdr(struct stcp_hdr *hdr);
void print_hdr(struct stcp_hdr *hdr);
/* Builds a pkt in host order */
void build_pkt(struct stcp_pkt *pkt, uint32_t seq, uint32_t ack, uint16_t win,
		uint16_t flags, void *data, int dlen);

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
 * @param swin   The maximum number of segments in the sending window
 * @param sock   The stcp_sock to initialize
 * @return 0 on success or -1 on error.
 */
int stcp_socket(int sockfd, uint16_t rwin, uint16_t swin, struct stcp_sock *sock);

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


/*
 * Recv a pkt from the server, buffer it, and send an ACK.
 *
 * @param sock A stcp_sock initialized by the client for reading.
 */
int stcp_client_recv(struct stcp_sock *sock);

/*
 * Read from the buffer and print to stdout.
 *
 * @param sock A stcp_sock initialized by the client for reading.
 */
int stcp_client_read(struct stcp_sock *sock);

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


/*
 * Start Circle Buffer Functions/Macros
 */

#define RWIN_ADV(rwin) ((rwin).size - (rwin).count)

#endif
