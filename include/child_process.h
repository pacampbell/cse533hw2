#ifndef CHILD_PROCESS_H
#define CHILD_PROCESS_H
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "debug.h"
#define PROCESSES_BUFFER 256

/** 
 * Hold information about the child process
 * that was forked off.
 */
typedef struct _Process {
	/* Process information */
	unsigned int pid;
	/* Client connection info */
	unsigned int port;
	char ip_address[PROCESSES_BUFFER];
	/* Interface connection info */
	unsigned int interface_fd;
	unsigned int interface_port;
	unsigned int interface_win_size;
	char interface_ip_address[PROCESSES_BUFFER];
	char interface_network_mask[PROCESSES_BUFFER];
	/* List fields */
	struct _Process *next;
	struct _Process *prev;
} Process;

/**
 * Adds a process to the list.
 * @param processes List containing all processes.
 * @param process Process to add to the list.
 * @return Returns The newly added process if successful, else NULL.
 */
Process* add_process(Process **processes, Process *process);

/**
 * Removes a process from the list.
 * @param processes List containing all processes.
 * @param Returns true if the process was successfully removed, else false.
 */
bool remove_process(Process **processes, Process *process);

/**
 * Searchs for a process in the list.
 * @param processes List containing all processes.
 * @return Returns the process if found, else NULL.
 */
Process* get_process(Process *processes, const char *ipaddress, unsigned int port);

/**
 * Gets a process in the process list by pid, if it exists.
 * @param processes List containing all processes.
 * @param pid PID of the process to search for.
 * @return Returns the process if found, else NULL.
 */
Process* get_process_by_pid(Process *processes, int pid);

/**
 * Frees all memory from the process list.
 * @param processes List containing all processes.
 */
void destroy_processes(Process **processes);

#endif
