#ifndef INTERFACES_H
#define INTERFACES_H
#define INTERFACES_BUFFER 256
#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "debug.h"
#include "unpifiplus.h"

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

/**
 * Removes a node from the interface list.
 * @param list List of unicast interfaces for this system.
 * @param node Node to remove from the list of interfaces.
 * @return Returns true if the node was successfully removed, 
 * else false.
 */
bool remove_node(Interface **list, Interface *node);

/**
 * Discovers unicast interfaces on the system.
 * @param config Contains configuration info from file.
 * @return Returns all the unicast interfaces on the system.
 */
Interface*  discoverInterfaces(Config *config);

#endif
