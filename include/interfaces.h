#ifndef INTERFACES_H
#define INTERFACES_H
#define INTERFACES_BUFFER 256
#include <stdlib.h>

/**
 * Container used for holding information
 * about an interface.
 */
typedef struct _Interface {
	int sockfd;
	char name[INTERFACES_BUFFER];
	char ip_address[INTERFACES_BUFFER];
	char network_mask[INTERFACES_BUFFER];
	char subnet_address[INTERFACES_BUFFER];
	struct _Interface *next;
	struct _Interface *prev;
} Interface;

/**
 * Frees all the memory allocated in to contain
 * information about the interfaces.
 * @param list List of unicast interfaces for this system.
 */
void destroy_interfaces(Interface **list);

#endif
