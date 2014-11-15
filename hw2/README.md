# CSE533 Assignment 2

## Students 
1. Shane Harvey 108272239 
2. Paul Campbell 108481554 

##  get_ifi_info Modifications

1. Closed an unclosed but unused file descriptor reported by valgrind.
2. Created an interface struct to hold the information about the system interfaces. It is located in interfaces.h . The struct holds the fields as described in the assignment, and also references to prev and next so we can use them as a linked list. Created a helper file interfaces.c to manage the interface structures as a linked list. 

## unprrt.h Modifications
1. Changed specified constants (RTT_RXTMIN..)
2. Changed to integer arithmetic (with microsecond granularity) rather than floating point.
3. Changed base time to microseconds instead of seconds
3. Changed rtt_init to initialize rttvar to 2000000 usec so the first RTO is 2 seconds
4. Each element in our send buffer keeps its own send timestamp which we use for calculating timeouts using RTT. 

## ARQ mechanism 
1. Flow control by limiting sending to the receivers advertised window size.
2. Implemented Congestion control with SlowStart, Congestion Avoidence, and Fast Retransmit.
3. Fast retransmit on 3 duplicate ACKs.
4. TCP deadlock avoidance when the receivers advertised window is 0 (by sending probes after each RTO)

## Closing logic 

Server Side:  
After sending the last bytes in the file, the server sends a FIN packet to notify the receiver that the file  transfer is complete. This FIN is backed by the ARQ mechanism. The server then waits for a FIN+ACK response from the client.

Client Side:  
If the Producer thread receives the FIN packet it buffers it in the receiving window and sends a FIN+ACK back to the server. If the FIN is buffered out of order by the Producer then it sends a duplicate ACK for the missing SEQ.

FIN+ACK Gets Dropped:  
The FIN+ACK is not backed up by any timeouts so in the event that it gets lost, the child server will timeout and retransmit the FIN. The server will then attempt to recv on the socket and return an error that the client has disconnected. In this case the server reports that the file transmission has failed, but the client has successfully completed the transfer.
