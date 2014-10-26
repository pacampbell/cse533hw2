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
	bool removed = false;
	Process *node = NULL;
	// Make sure we have a valid process list
	if(processes == NULL || *processes ==  NULL) {
		error("Attempted to remove a process from a NULL list.\n");
		goto finished;
	}
	// Make sure we have a valid process to search for
	if(process == NULL) {
		error("Attempted to remove a non-existant process from the process list.\n");
	}
	// We made it this far, lets search for the process and remove it.
	node = *processes;
	while(node != NULL) {
		if(node->pid == process->pid) {
			removed = true;
			// Check to see if this is the head node.
			if(node == *processes) {
				*processes = node->next;
				if(*processes != NULL) {
					(*processes)->prev = NULL;
				}
			} else {
				// We are not at the head
				node->prev->next = node->next;
				node->next->prev = node->prev;
			}
			free(node);
			break;
		}
		// Not a match keep going
		node = node->next;
	}
	if(!removed) {
		warn("The process <%d> does not exist in the list. Unable to remove it.\n", process->pid);
	}
finished:
	return removed;
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

Process* get_process_by_pid(Process *processes, int pid) {
	Process *found = NULL;
	if(processes != NULL) {
		Process *node = processes;
		while(node != NULL) {
			if(node->pid == pid) {
				found = node;
				break;
			}
		}
	}
	return found;
}

static void _destroy_processes(Process *processes) {
	warn("Fuction '_destroy_processes' not implemented.\n");
}

void destroy_processes(Process **processes) {
	warn("Fuction 'destroy_processes' not implemented.\n");
	_destroy_processes(*processes);
}
