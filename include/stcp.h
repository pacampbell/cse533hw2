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

/* Shitty TCP structs */
struct stcphdr {
	uint32_t syn;		/* Sequence Number */
	uint32_t ack;		/* Acknowledgment Number */
	uint16_t win;		/* Window size (in datagram units) */
	uint16_t flags;		/* Flags */
#define STCP_FIN  0x01
#define STCP_SYN  0x02
#define STCP_RST  0x04
#define STCP_ACK  0x08
};

#endif
