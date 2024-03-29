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
#define STCP_FAST_RETRANSMIT 3

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
	uint64_t sent_ts;		/* Microsec UNIX ts of the sending time */
} Elem;

/* STCP sliding window */
typedef struct {
	Elem *buf;			/* (contiguous) buffer space for size Elems */
	uint16_t size;		/* length of receive window (#Elem allocated) */
	uint16_t count;		/* # of elements in buffer */
	uint16_t end;		/* Index of the next free elem		*/
	uint16_t start;		/* Index of the oldest elem		*/
	/* stuff specifically for receiving window */
	uint32_t next_seq;	/* For RWIN: Sequence # we expect to recv next */
						/* For SWIN: Sequence # we expect to buffer next */
	/* stuff specifically for sending window */
	uint32_t next_ack;	/* Acknowledgment Number we expect to recv next */
	/* For Sending */
	int in_flight;
	uint16_t rwin_adv;	/* Last seen window size from a receiver ACK */
	uint16_t cwnd;		/* Congestion window value */
	uint16_t ssthresh;	/* Slow Start Threshold */
	int dup_ack;		/* Duplicate ACK count for fast retransmit */
	// TODO: other shit for sending
} Window;

/* This will be the 'new socket' used to send and recv segments */
struct stcp_sock {
	int sockfd;			/* The connected UDP socket */
	Window win;
	/* For Receiving */
	pthread_mutex_t mutex;
};


/* convert packet from/to host order */
void hton_hdr(struct stcp_hdr *hdr);
void ntoh_hdr(struct stcp_hdr *hdr);
void print_hdr(struct stcp_hdr *hdr);
/* Builds a pkt in host order */
void build_pkt(struct stcp_pkt *pkt, uint32_t seq, uint32_t ack, uint16_t win,
		uint16_t flags, void *data, int dlen);

/**
 * Test if a packet is a valid STCP packet.
 *
 * @param pkt The received pkt in HOST order
 * @return 1 if valid, 0 if invalid
 */
int valid_pkt(struct stcp_pkt *pkt);

/**
 * Test if a packet is a valid SYN ACK in response to a SYN. SYN and ACK
 * flags should be set, ack # should be sent_seq + 1, and data field
 * should contain a port.
 *
 * @param pkt The received pkt in host order
 * @param sent_seq The starting sequence number we sent in our SYN
 * @return 1 if valid, 0 if invalid
 */
int valid_synack(struct stcp_pkt *pkt, uint32_t sent_seq);

/*
 * Initialize a stcp_sock from the given socket. sockfd must haven been
 * created with createClientSocket or appropriately binded/connected to
 * a peer.
 *
 * @param sockfd 	A valid sockfd that has been created by createClientSocket
 * @param win_size  The maximum number of segments in the sliding window
 * @param sock   	The stcp_sock to initialize
 * @return 			0 on success or -1 on error.
 */
int stcp_socket(int sockfd, uint16_t win_size, struct stcp_sock *stcp);

/*
 * Free all memory associated with this stcp socket and close the
 * underlying socket.
 *
 * @param sock  A stcp_sock initialized by stcp_socket
 * @return 0 on successful connection or -1 on error.
 */
int stcp_close(struct stcp_sock *stcp);

/*
 * Initialize a stcp connection to the server using the 3 way handshake
 *
 * @param sock  A stcp_sock initialized by stcp_socket
 * @param file  The name of the file to download
 * @return 0 on successful connection or -1 on error.
 */
int stcp_connect(struct stcp_sock *stcp, struct sockaddr_in *serv_addr, char *file);


/*
 * Recv a pkt from the server, buffer it, and send an ACK.
 *
 * @param sock A stcp_sock initialized by the client for reading.
 * @param bbytes Will store the number of bytes of data add to the window
 * @return  -1 on error
 *			 0 if the FIN was received
 *			>0 if we are waiting for more data
 */
int stcp_client_recv(struct stcp_sock *stcp, int *bbytes);

/*
 * Read from the Window buffer.
 *
 * @param sock A stcp_sock initialized by the client for reading.
 * @param buf  The buffer to copy data into.
 * @param len  length of the provided buffer. Must be a multiple of STCP_MAX_DATA
 * @return -1 on error
 * 			0 on EOF
 *			1 on success
 */
int stcp_client_read(struct stcp_sock *stcp, char *buf, int buflen, int *nread);

/*
 * Set the rate at which packets are artificially dropped on send and recv
 *
 * @param seed  To seed the RNG
 * @param loss  The artificial drop rate
 */
void client_set_loss(unsigned int seed, double loss);

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
 *          0: if the packet is too small or is invalid
 *          1: if packet is valid
 */
int recv_pkt(int sockfd, struct stcp_pkt *pkt, int flags);
int recvfrom_pkt(int sockfd, struct stcp_pkt *pkt, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen);


/*
 * Start Circle Buffer Functions/Macros
 */

#define WIN_ADV(win) ((win).size - (win).count)

void win_print(Window *win);

/**
 * Allocate and initialize a sliding window
 *
 * @param win 			The Window
 * @param win_size 		Max # of Elem in the window
 * @param initial_seq 	Sender: The initial seq to send
 						Receiver: the initial seq to recv
 * @return Window Elem count
 */
int win_init(Window *win, int win_size, uint32_t initial_seq);

/**
 * Free any memory associated with the Window win.
 */
void win_destroy(Window *win);

/**
 * Number of elements inside the window.
 * @param win The Window
 * @return Window Elem count
 */
int win_count(Window *win);

/**
 * Number of free elements in the window.
 * @param win The Window
 * @return Window free element count
 */
int win_available(Window *win);

/**
 * If the window is currently full
 * @param win The Window
 * @return 1 if full, 0 otherwise
 */
int win_full(Window *win);

/**
 * If the window is currently empty
 * @param win The Window
 * @return 1 if empty, 0 otherwise
 */
int win_empty(Window *win);

/**
 * Appends the Elem pointed to by elem to the Window win. A reference to the
 * added Elem inside the Window is returned. Or NULL if the window was full.
 * @param win  The Window
 * @param elem The Elem to append to the window
 * @return pointer to the added elem, or NULL if the window was full.
 */
Elem *win_add(Window *win, Elem *elem);

/**
 * Returns the oldest elem inside the Window win. Or NULL if the window was empty.
 * @param win  The Window
 * @return pointer to the oldest elem, or NULL if the window was empty.
 */
Elem *win_oldest(Window *win);

/**
 * Returns the reference to the current end of the window.
 * The end is the next Elem after win->end
 * @param win  The Window
 * @return pointer to the end elem.
 */
Elem *win_end(Window *win);

/**
 * Removes the oldest Elem inside the Window win.
 * @param win  The Window
 */
void win_remove(Window *win);

/**
 * Removes all the elements from the Window win.
 * @param win  The Window to clear
 */
void win_clear(Window *win);

/**
 * Functions only for receiver side
 */

/**
 * Adds the Elem pointed to by elem into the Window potentially buffering
 * the element out of order. This function returns true if the last element
 * in the window after inserting elem is the FIN packet.
 *
 * @param win    The Window
 * @param elem   The Elem to append to the window
 * @param bbytes Will store the number of bytes of data add to the window
 * @return True if the FIN is in the window, else false
 */
int win_add_oor(Window *win, Elem *elem, int *bbytes);

/**
 * Returns the elem at offset fwdoff. Or NULL if the index is too large.
 * @param win  The Window
 * @param endoff The forward offset from end of the Element to return.
 * @return pointer to the oldest elem, or NULL if the index is too large.
 */
Elem *win_get(Window *win, int endoff);

/**
 * Functions only for sender side
 */

/**
* Returns minimum of cwnd, receiver advertised win, sender available window.
* @param win  The sending window
* @return Number of Elements we can put in the buffer
*/
int win_send_limit(Window *win);

/**
 * Reads one element from the file and adds it to the window.
 *
 * @param win  The sending window
 * @param fd   The file descriptor open for reading
 * @return  -1: on fatal error.
 *           0: If the EOF is reached
 *          >0: If the EOF was not reached and we Added an elem
 */
int win_buffer_elem(Window *win, int fd);

/**
 * Returns the elem at offset startoff from the start of the window. Or NULL
 * if the offset is too large.
 * @param win  The Window
 * @param startoff The forward offset from start of the Element to return.
 * @return pointer to the oldest elem, or NULL if the index is too large.
 */
Elem *win_get_index(Window *win, int startoff);

int win_valid_seq(Window *win, uint32_t seq);

/*
 * Returns true if the packet pointed to by pkt is a valid ACK for the current
 * state of sending window win. Duplicate ACK packets are valid and should
 * return True.
 *
 * @param win  The sending window
 * @param pkt  The pkt in question
 * @return True if valid, False if invalid
 */
int win_valid_ack(Window *win, struct stcp_pkt *pkt);

/*
 * Returns true if the packet pointed to by pkt is a a duplicate ACk.
 * And updates the duplicate ACK count in the Window
 *
 * @param win  The sending window
 * @param pkt  The pkt that was validated by win_valid_ack
 * @return True if duplicate, False if not
 */
int win_dup_ack(Window *win,  struct stcp_pkt *pkt);

/*
 * Removes all Element in the window whose seq# is < ack.
 *
 * @param win  The sending window
 * @param ack_pkt  The pkt which is a valid ACK packet
 * @return The count of Elements removed
 */
int win_remove_ack(Window *win,  struct stcp_pkt *ack_pkt);

#endif
