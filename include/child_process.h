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
	unsigned int pid;
	unsigned int port;
	char ip_address[PROCESSES_BUFFER];
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
 * Frees all memory from the process list.
 * @param processes List containing all processes.
 */
void destroy_processes(Process **processes);

#endif
