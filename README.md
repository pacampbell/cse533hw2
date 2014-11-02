# CSE533 Assignment 2

## Students 
1. Shane Harvey 
2. Paul Campbell 108481554 

##  get_ifi_info Modifications

1. Closed an unclosed but unused file descriptor reported by valgrind.
2. Created an interface struct to hold the information about the system interfaces. It is located in interfaces.h . The struct holds the fields as described in the assignment, and also references to prev and next so we can use them as a linked list. Created a helper file interfaces.c to manage the interface structures as a linked list. 

## unprrt.h Modifications
1. Changed specified constants (RTT_RXTMIN..)
2. Changed to integer arithmetic rather than floating point.
3. Changed base time to millisec instead of seconds
3. Changed rtt_init to initialize rttvar to 3000 ms ((0.75s * 1000) << 4)
4. Each element in our send buffer keeps its own send timestamp which we use for calculating timeouts using RTT.

## ARQ mechanism 
TODO

## Closing logic
TODO
