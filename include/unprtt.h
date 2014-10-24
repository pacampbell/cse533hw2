#ifndef	__unp_rtt_h
#define	__unp_rtt_h
/* stdlib headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
/* project headers */
#include "debug.h"

struct rtt_info {
	int			rtt_rtt;	/* most recent measured RTT, msec */
	int			rtt_srtt;	/* smoothed RTT estimator, msec */
	int			rtt_rttvar;	/* smoothed mean deviation, msec */
	int			rtt_rto;	/* current RTO to use, msec */
	int			rtt_nrexmt;	/* #times retransmitted: 0, 1, 2, ... */
	uint32_t	rtt_base;	/* #sec since 1/1/1970 at start */
};

#define	RTT_RXTMIN 1000		/* min retransmit timeout value, msec */
#define	RTT_RXTMAX 3000		/* max retransmit timeout value, msec */
#define	RTT_MAXNREXMT 12	/* max #times to retransmit */

/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init(struct rtt_info *);
void	 rtt_newpack(struct rtt_info *);
int		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *, uint32_t);
int		 rtt_timeout(struct rtt_info *);
uint32_t rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set nonzero for addl info */

#endif	/* __unp_rtt_h */
