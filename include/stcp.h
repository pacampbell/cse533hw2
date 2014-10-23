/* Shitty TCP */
#ifndef STCP_H
#define STCP_H
/* stdlib headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
/* project headers */
#include "debug.h"

/* constants */
#define STCP_MAX_DG 512
#define STCP_HDR_SIZE 12
#define STCP_MAX_DATA 500

/* STCP structs */
/* Header */
struct stcp_hdr {
	uint32_t syn;		/* Sequence Number */
	uint32_t ack;		/* Acknowledgment Number */
	uint16_t win;		/* Window size (in datagram units) */
	uint16_t flags;		/* Flags */
#define STCP_FIN  0x01
#define STCP_SYN  0x02
#define STCP_RST  0x04
#define STCP_ACK  0x08
};

/* STCP recv circular buffer entry */
struct stcp_seg {
	/* STCP Segment */
	struct stcp_hdr hdr;		/* Segment header	*/
	char data[STCP_MAX_DATA];	/* Segment data		*/

	/* STCP Segment meta-data used to manage circular buffer */
	uint32_t dlen;				/* Length of data */
	uint8_t free;				/* Entry is free (can be overwritten) */

	/* For Receiving ??? */

	/* For Sending ??? */
	/* time sent and num retried */
};

/* STCP recv sliding window */
struct stcp_rwin {
	struct stcp_seg *cbuf;	/* (contiguous) circular window buffer 	*/
	uint16_t size;			/* length of receive window (#stcp_seg allocated)*/
	uint32_t next_syn;		/* Sequence Number we expect to write next */
	uint16_t write_head;	/* Index of the free entry to write
								the next SYN segment 				*/
	uint16_t read_head;		/* Index of the first unread entry		*/
};

/* This will be the 'new socket' used to send and recv segments */
struct stcp_sock {
	/* Real DG socket */
	int sockfd;
	/* I don't know what I'm doing */

	/* For Receiving */
	uint16_t rwin_ad;	/* receive window size to advertise (in DG units) 	*/
	struct stcp_rwin rwin;

	/* For Sending */
	uint16_t swin;
};

/*
 * Initialize a stcp_sock from the given socket. sockfd must haven been
 * created with createClientSocket or appropriately binded/connected to
 * a peer.
 *
 * @param sockfd A valid sockfd that has been created by createClientSocket
 * @param rwin   The maximum number of segments in the receiving window
 * @return An stcp_sock initialized for reading end or NULL on error
 */
struct stcp_sock *stcp_socket(int sockfd, uint32_t rwin);

#endif
