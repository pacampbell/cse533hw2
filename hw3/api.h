#ifndef API_H
#define API_H
#include <sys/types.h>

ssize_t msg_send(int sd, const char *canonicalIP, int port, void *msg, int flag);

ssize_t msg_recv(int sd, void *msg, const char *canonicalIP, int *port);

#endif
