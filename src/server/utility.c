#include "utility.h"

bool parseConfig(char *path, Config *config) {
	bool success = false;
	if(path != NULL && config != NULL) {
		FILE *config_file = fopen(path, "r");
		if(config_file != NULL) {
			// Atempt to read the file line by line
			char *line = NULL;
			size_t len = 0;
			size_t read;
			// Read in the port value
			if((read = getline(&line, &len, config_file)) != -1) {
				config->port = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			// Read in the mtu size
			if((read = getline(&line, &len, config_file)) != -1) {
				config->mtu = (unsigned int) atoi(line); 
			} else {
				goto close;
			}
			// If we got this far must be a success
			success = true;
close:
			// Close the file handle
			fclose(config_file);
		}
	}
	return success;
}
