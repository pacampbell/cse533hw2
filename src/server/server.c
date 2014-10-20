#include "server.h"

int main(int argc, char *argv[]) {
	char *path = "server.in";
	Config config;
	debug("Begin parsing %s\n", path);

	if(parseConfig(path, &config)) {
		debug("Port: %u\n", config.port);
		debug("MTU: %u\n", config.mtu);
	} else {
		debug("Failed to parse: %s\n", path);
	}

	return EXIT_SUCCESS;
}
