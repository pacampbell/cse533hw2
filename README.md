# CSE533 Assignment 2

## Description

Offical assignment description is located [here](http://www3.cs.stonybrook.edu/~cse533/asgn2/asgn2.html).

## Pre-reqs

1. gcc
2. make

## Usage
> TODO: Show server and client usage

## Build Instructions
1. Navigate to project root directory and type `make` or `make debug`
2. The binarys will be placed in the `BINDIR`

## Run Instructions
1. Navigate to the root directory of the project and type `./bin/server` or `./bin/client`
or
1. Type `make run-server` or `make run-client`


##unprrt.h Modifications
1. Changed Specifid constants (RTT_RXTMIN..)
2. Changed to integer arithmetic rather than floating point.
3. Changed base time to millisec instead of seconds
3. Changed rtt_init to initialize rttvar to 3000 ms ((0.75s * 1000) << 4)