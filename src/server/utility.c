#include "utility.h"

bool parseConfig(char *path, Config *config) {
	bool success = false;
	if(path != NULL && config != NULL) {
		FILE *config_file = fopen(path, "r");
		if(config_file != NULL) {
			/* Atempt to read the file line by line */
			char line[BUFFER_SIZE];
			/* Read in the port value */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->port = (unsigned int) atoi(line);
			} else {
				goto close;
			}
			/* Read in the mtu size */
			if(fgets(line, BUFFER_SIZE, config_file) != NULL) {
				config->mtu = (unsigned int) atoi(line); 
			} else {
				goto close;
			}
			/* If we got this far must be a success */
			success = true;
close:
			/* Close the file handle */
			fclose(config_file);
		}
	}
	return success;
}
