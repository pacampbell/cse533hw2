#include "child_process.h"

Process* add_process(Process **processes, Process *process) {
	return NULL;
}

bool remove_process(Process **processes, Process *process) {
	return false;
}

Process* get_process(Process *processes, const char *ipaddress, unsigned int port) {
	Process *found = NULL;
	if(processes != NULL) {
		Process *node = processes;
		while(node != NULL) {
			if(strcmp(node->ip_address, ipaddress) && node->port == port) {
				error("TODO: FINISH THIS FUNCTION\n");
			}
			node = node->next;
		}
	}
	return found;
}

static void _destroy_processes(Process *processes) {

}

void destroy_processes(Process **processes) {
	_destroy_processes(*processes);
}
