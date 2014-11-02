#ifndef	__unp_rtt_h
#define	__unp_rtt_h
/* stdlib headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
/* project headers */
#include "debug.h"

struct rtt_info {
	int			rtt_rtt;	/* most recent measured RTT, usec */
	int			rtt_srtt;	/* smoothed RTT estimator, usec */
	int			rtt_rttvar;	/* smoothed mean deviation, usec */
	int			rtt_rto;	/* current RTO to use, usec */
	int			rtt_nrexmt;	/* #times retransmitted: 0, 1, 2, ... */
	uint64_t	rtt_base;	/* #sec since 1/1/1970 at start */
};

#define	RTT_RXTMIN 1000000	/* min retransmit timeout value, usec (1 second)  */
#define	RTT_RXTMAX 3000000	/* max retransmit timeout value, usec (3 seconds) */
#define	RTT_MAXNREXMT 12	/* max #times to retransmit */

/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init(struct rtt_info *);
uint64_t rtt_getusec(void);
void	 rtt_newpack(struct rtt_info *);
int		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *);
int		 rtt_timeout(struct rtt_info *);
int      rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set nonzero for addl info */

#endif	/* __unp_rtt_h */
