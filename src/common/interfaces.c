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
		error("Attempted to remove a node from a NULL list.\n");
		goto finished;
	}
	// Make sure the node isn't NULL
	if(node == NULL) {
		error("Attempted to remove a NULL node from the list.\n");
		goto finished;
	}
	// If we got here then lets try to remove the node.
	cn = *list;
	while(cn != NULL) {
		if(strcmp(cn->name, node->name) == 0 && strcmp(cn->ip_address, node->ip_address)) {
			success = true;
			debug("Removed: <%s> %s from the list of interfaces", node->name, node->ip_address);
			if(node->prev == NULL) {
				// We found the head
				*list = node->next;
			} else {
				node->prev = node->next;
				node->next->prev = node->prev;
			}
			// Free the space since it should of been dynamiclly alocated.
			free(node);
			break;
		}
		// Get the next node in the list
		cn = cn->next;
	}
	// If the node was unable to be removed.
	if(!success) {
		// Node wasn't removed so we can still use it.
		warn("The node: <%s> %s doesn't exist in the list.\n", node->name, node->ip_address);
	}
finished:
	return success;
}

Interface* discoverInterfaces() {
	struct ifi_info	*ifi, *ifihead;
	struct sockaddr	*sa;
	unsigned int a = 0, b = 0;
	// Create memory for the list.
	Interface *list = NULL;
	// Use crazy code to loop through the interfaces
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		// Create node
		Interface *node = malloc(sizeof(Interface));
		// Null out pointers
		node->next = NULL;
		node->prev = NULL;
		// Figure out where to place the node
		if(list == NULL) {
			list = node;	
		} else {
			// Just push it down the list
			// IE: It goes in reverse discovery order
			node->next = list;
			list->prev = node;
			list = node;
		}
		// Copy the name of the interface
		strcpy(node->name, ifi->ifi_name); 
		// Copy the IPAddress
		if((sa = ifi->ifi_addr) != NULL) {
			strcpy(node->ip_address, Sock_ntop_host(sa, sizeof(*sa)));
			a = convertIp(node->ip_address);
		}
		// Copy the network mask
		if((sa = ifi->ifi_ntmaddr) != NULL) {
			strcpy(node->network_mask, Sock_ntop_host(sa, sizeof(*sa)));
			b = convertIp(node->network_mask);
		}
		// Figure out the subnet mask
		if(a && b) {
			struct in_addr ip_addr;
			unsigned int subnet = a & b;
			ip_addr.s_addr = subnet;
			strcpy(node->subnet_address, inet_ntoa(ip_addr));
		}
		// Print out info
		info("<%s> [%s%s%s%s%s\b] IP: %s Mask: %s Subnet: %s\n", 
			node->name,
			ifi->ifi_flags & IFF_UP ? "UP " : "",
			ifi->ifi_flags & IFF_BROADCAST ? "BCAST " : "",
			ifi->ifi_flags & IFF_MULTICAST ? "MCAST " : "",
			ifi->ifi_flags & IFF_LOOPBACK ? "LOOP " : "",
			ifi->ifi_flags & IFF_POINTOPOINT ? "P2P " : "",
			node->ip_address, 
			node->network_mask, 
			node->subnet_address);
	}
	free_ifi_info_plus(ifihead);
	return list;
}

bool isSameSubnet(const char *server_ip, const char *client_ip, const char *network_mask) {
	bool same = false;
	if(server_ip != NULL && client_ip != NULL && network_mask != NULL) {
		struct sockaddr_in server_sockaddr, client_sockaddr, mask_sockaddr;
		unsigned int server_mask = 0, client_mask = 0;
		// Convert the string to sockaddr_in
		inet_pton(AF_INET, server_ip, &server_sockaddr);
		inet_pton(AF_INET, client_ip, &client_sockaddr);
		inet_pton(AF_INET, network_mask, &mask_sockaddr);
		// Mask the addresses 
		server_mask = server_sockaddr.sin_addr.s_addr & mask_sockaddr.sin_addr.s_addr;
		client_mask = client_sockaddr.sin_addr.s_addr & mask_sockaddr.sin_addr.s_addr;
		// Determine if they are the same
		same = server_mask == client_mask;
	} else {
		error("NULL value passed in.\n server_ip = %s\nclient_ip = %s\nnetwork_mask = %s\n",
			server_ip == NULL ? "NULL" : server_ip,
			client_ip == NULL ? "NULL" : client_ip,
			network_mask == NULL ? "NULL" : network_mask);
	}
	return same; 
}
