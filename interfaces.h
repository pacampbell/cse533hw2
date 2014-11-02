#ifndef INTERFACES_H
#define INTERFACES_H
#define INTERFACES_BUFFER 256
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "utility.h"
#include "debug.h"
#include "unpifiplus.h"

#define BIND_INTERFACE 1
#define DONT_BIND_INTERFACE 0

/**
 * Container used for holding information
 * about an interface.
 */
typedef struct _Interface {
	int sockfd;
	char name[INTERFACES_BUFFER];
	char ip_address[INTERFACES_BUFFER];
	char network_mask[INTERFACES_BUFFER];
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
 * @param config Configuration containing port number to use.
 * @param shouldBind Tells the function to bind to discovered interfaces.
 * @return Returns all the unicast interfaces on the system.
 */
Interface*  discoverInterfaces(Config *config, bool shouldBind);

/**
 * Checks to see if two ipaddresses are part of the same subnet.
 * @param server_ip Ipaddress of the server.
 * @param client_ip Ipaddress of the client.
 * @param network_mask Network mask to perform the operation with.
 * @return Returns true if the same subnet, else false.
 */
bool isSameSubnet(const char *server_ip, const char *client_ip, const char *network_mask);

/**
 * Checks to see if two ipaddresses are exactly the same.
 * @param serv_addr Ipaddress of the server.
 * @param client_ip Ipaddress of the client as string
 * @return Returns true if they match, else false.
 */
bool isSameIP(struct in_addr serv_addr, const char *client_ip);

/**
 * Calculates the size of the interface.
 * @param interfaces List containing all the network interfaces.
 * @return Return returns the size of the interfaces list. 
 */
unsigned int size(Interface *interfaces);

#endif
