/* include rtt1 */
#include	"unprtt.h"

int		rtt_d_flag = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) (((ptr)->rtt_srtt >> 3) + (ptr)->rtt_rttvar)

static int rtt_minmax(int rto) {
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void rtt_init(struct rtt_info *ptr) {
	ptr->rtt_base   = 0;
	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 3000000; /* .75s * 4 = 3s ==> 3000ms */
	/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
	ptr->rtt_rto    = rtt_minmax(RTT_RTOCALC(ptr));
	ptr->rtt_nrexmt = 0;
}

uint64_t rtt_getusec(void) {
	struct timeval	tv;

	if(gettimeofday(&tv, NULL) != 0){
		error("RTT update failed on gettimeofday: %s\n", strerror(errno));
		return 0;
	}
	return ((uint64_t)tv.tv_sec * 1000000L) + (uint64_t)tv.tv_usec;
}

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */
int rtt_ts(struct rtt_info *ptr) {
	return (int)(rtt_getusec() -  ptr->rtt_base);
}

void rtt_newpack(struct rtt_info *ptr) {
	ptr->rtt_nrexmt = 0;
}

/* returns msec RTO */
int rtt_start(struct rtt_info *ptr) {
	return(ptr->rtt_rto);
	/* return value can be used as: alarm(rtt_start(&foo)) */
}

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */
void rtt_stop(struct rtt_info *ptr) {
	/* Retransmission ambiguity problem: solved */
	if(ptr->rtt_nrexmt > 0) {
		return;
	}
	/* Update most recent measured RTT in usec */
	ptr->rtt_rtt = rtt_ts(ptr);

	/*
	 * Jacobson‐Karels  Algorithm
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 */
	ptr->rtt_rtt -= ptr->rtt_srtt >> 3;
	ptr->rtt_srtt += ptr->rtt_rtt;
	if(ptr->rtt_rtt < 0) {
		ptr->rtt_rtt = -ptr->rtt_rtt;
	}
	ptr->rtt_rtt -= ptr->rtt_rttvar >> 2;
	ptr->rtt_rttvar += ptr->rtt_rtt;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
}

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 */
int rtt_timeout(struct rtt_info *ptr) {
	/* double rto with min max clamps */
	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto << 1);

	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return(-1);			/* time to give up for this packet */
	return(0);
}

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void rtt_debug(struct rtt_info *ptr) {
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %d, srtt = %d, rttvar = %d, rto = %d\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
}
