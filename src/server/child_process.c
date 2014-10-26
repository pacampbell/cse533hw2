#include "child_process.h"

Process* add_process(Process **processes, Process *process) {
	Process *new_p = NULL;
	if(processes != NULL) {
		new_p = process;
		if(*processes == NULL) {
			// Set the head of the list to this new process
			*processes = new_p;
			// Null out next and prev just incase.
			new_p->next = NULL;
			new_p->prev = NULL;
		} else {
			// Just add the new process to the head of the list.
			new_p->prev = NULL;
			// Set the new nodes next to the current head
			new_p->next = *processes;
			// Set the current heads prev to the new node
			(*processes)->prev = new_p;
			// Set the head to the new node
			*processes = new_p;
		}
	}
	return new_p;
}

bool remove_process(Process **processes, Process *process) {
	error("Fuction not implemented.\n");
	return false;
}

Process* get_process(Process *processes, const char *ipaddress, unsigned int port) {
	Process *found = NULL;
	if(processes != NULL) {
		Process *node = processes;
		while(node != NULL) {
			if(strcmp(node->ip_address, ipaddress) == 0 && node->port == port) {
				found = node;
				break;
			}
			node = node->next;
		}
	} else {
		info("No processes are currently being tracked.\n");
	}
	return found;
}

static void _destroy_processes(Process *processes) {
	error("Fuction not implemented.\n");
}

void destroy_processes(Process **processes) {
	error("Fuction not implemented.\n");
	_destroy_processes(*processes);
}
