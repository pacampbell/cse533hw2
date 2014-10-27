#include "interfaces.h"

static void _destroy_interfaces(Interface *node) {
	if(node != NULL) {
		_destroy_interfaces(node->next);
		free(node);
	}
}

void destroy_interfaces(Interface **list) {
	if(*list != NULL) {
		_destroy_interfaces(*list);
		/* Set the head to NULL */
		*list = NULL;
	} else {
		warn("Tried to destroy a NULL interface list.\n");
	}
}

bool remove_node(Interface **list, Interface *node) {
	bool success = false;
	Interface *cn;
	// Make sure the list isn't NULL
	if(list == NULL || *list == NULL) {
		error("Attempted to remove an interface from a NULL list.\n");
		goto finished;
	}
	// Make sure the node isn't NULL
	if(node == NULL) {
		error("Attempted to remove a NULL interface from the list.\n");
		goto finished;
	}
	// If we got here then lets try to remove the node.
	cn = *list;
	while(cn != NULL) {
		if(strcmp(cn->name, node->name) == 0 && strcmp(cn->ip_address, node->ip_address) == 0) {
			success = true;
			debug("Removed: <%s> %s from the list of interfaces\n", node->name, node->ip_address);
			if(node->prev == NULL) {
				// We found the head
				*list = node->next;
				if(*list != NULL) {
					(*list)->prev = NULL;
				}
			} else {
				node->prev->next = node->next;
				node->next->prev = node->prev;
			}
			free(node);
			break;
		}
		// Get the next node in the list
		cn = cn->next;
	}
	// If the node was unable to be removed.
	if(!success) {
		// Node wasn't removed so we can still use it.
		warn("The interface: <%s> %s doesn't exist in the list.\n", node->name, node->ip_address);
	}
finished:
	return success;
}

Interface* discoverInterfaces(Config *config, bool shouldBind) {
	struct ifi_info	*ifi, *ifihead;
	struct sockaddr	*sa;
	bool insert = false;
	int server_fd = 0;
	// Create memory for the list.
	Interface *list = NULL;
	// Use crazy code to loop through the interfaces
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		// Create node
		Interface *node = malloc(sizeof(Interface));
		insert = false;
		// Null out pointers
		node->next = NULL;
		node->prev = NULL;
		// Copy the name of the interface
		strcpy(node->name, ifi->ifi_name); 
		// Copy the IPAddress
		if((sa = ifi->ifi_addr) != NULL) {
			strcpy(node->ip_address, Sock_ntop_host(sa, sizeof(*sa)));
		}
		// Copy the network mask
		if((sa = ifi->ifi_ntmaddr) != NULL) {
			strcpy(node->network_mask, Sock_ntop_host(sa, sizeof(*sa)));
		}
		// Print out info
		info("<%s> [%s%s%s%s%s\b] IP: %s Mask: %s\n", 
			node->name,
			ifi->ifi_flags & IFF_UP ? "UP " : "",
			ifi->ifi_flags & IFF_BROADCAST ? "BCAST " : "",
			ifi->ifi_flags & IFF_MULTICAST ? "MCAST " : "",
			ifi->ifi_flags & IFF_LOOPBACK ? "LOOP " : "",
			ifi->ifi_flags & IFF_POINTOPOINT ? "P2P " : "",
			node->ip_address,
			node->network_mask);

		if(shouldBind) {
			/* Config was successfully parsed; attempt to bind sockets */
			server_fd = createServer(node->ip_address, config->port);
			if(server_fd != SERVER_SOCKET_BIND_FAIL) {
				node->sockfd = server_fd;
				insert = true;
			} else {
				/* Unable to bind socket to port */
				fprintf(stderr, "Failed to bind socket: %s:%u\n", node->ip_address, config->port);
			}
		} else {
			insert = true;
		}

		// If all was successful insert into the list
		if(insert) {
			if(list == NULL) {
				list = node;	
			} else {
				// Just push it down the list
				// IE: It goes in reverse discovery order
				node->next = list;
				list->prev = node;
				list = node;
			}
		}
	}
	free_ifi_info_plus(ifihead);
	return list;
}

unsigned int size(Interface *interfaces) {
	unsigned int size = 0;
	if(interfaces != NULL) {
		Interface *node = interfaces;
		while(node != NULL) {
			size++;
			node = node->next;
		}
	} else {
		warn("Trying to find size of a non-existent interface list.\n");
	}
	return size;
}

bool isSameSubnet(const char *server_ip, const char *client_ip, const char *network_mask) {
	bool same = false;
	if(server_ip != NULL && client_ip != NULL && network_mask != NULL) {
		struct sockaddr_in server_sockaddr, client_sockaddr, mask_sockaddr;
		unsigned int server_subnet = 0, client_subnet = 0;
		// Convert the string to sockaddr_in
		inet_aton(server_ip, &server_sockaddr.sin_addr);
		inet_aton(client_ip, &client_sockaddr.sin_addr);
		inet_aton(network_mask, &mask_sockaddr.sin_addr);
		// Mask the addresses 
		server_subnet = server_sockaddr.sin_addr.s_addr & mask_sockaddr.sin_addr.s_addr;
		client_subnet = client_sockaddr.sin_addr.s_addr & mask_sockaddr.sin_addr.s_addr;
		// Determine if they are the same
		same = server_subnet == client_subnet;
	} else {
		error("NULL value passed in.\n server_ip = %s\nclient_ip = %s\nnetwork_mask = %s\n",
			server_ip == NULL ? "NULL" : server_ip,
			client_ip == NULL ? "NULL" : client_ip,
			network_mask == NULL ? "NULL" : network_mask);
	}
	return same; 
}

bool isSameIP(struct in_addr serv_addr, const char *client_ip) {
	bool same = false;
	if(client_ip != NULL) {
		struct in_addr client_addr;
		// Convert the string to sockaddr_in
		inet_aton(client_ip, &client_addr);
		// Determine if they are the same
		same = client_addr.s_addr == serv_addr.s_addr;
	} else {
		error("NULL value passed in. client_ip = %s\n",
			client_ip == NULL ? "NULL" : client_ip);
	}
	return same;
}
