#include "interfaces.h"

static void _destroy_interfaces(Interface *node) {
	if(node != NULL) {
		_destroy_interfaces(node->next);
	}
	free(node);
}

void destroy_interfaces(Interface **list) {
	_destroy_interfaces(*list);
	if(*list != NULL) {
		*list = NULL;
	}
}