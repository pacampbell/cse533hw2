#include "client.h"

int main(int argc, char *argv[]) {
	debug("Client dummy program: %s\n", argv[0]);

	struct hostent *hp;     /* host information */
	struct sockaddr_in servaddr;    /* server address */
	char *my_message = "this is a test message";
	int fd;

	memset((char*)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(8080);

	hp = gethostbyname("localhost");

	if (!hp) {
		fprintf(stderr, "could not obtain address of %s\n", "localhost");
		return 0;
	}

	/* put the host's address into the server address structure */
	memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);

	/* Socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket");
		return 0;
	}

	/* send a message to the server */
	if (sendto(fd, my_message, strlen(my_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("sendto failed");
		return 0;
	}

	return EXIT_SUCCESS;
}
