#include "client.h"

int main(int argc, char *argv[]) {
    int success = EXIT_SUCCESS;
    char temp_name[] = "hw3_tempXXXXXX";
    int sock_fd;
    int named_pipe;
    struct sockaddr_un local;
    // Attempt to open the temp file.
    if((named_pipe = mkstemp(temp_name)) < 0) {
        error("Failed to open the tempfile %s\n", temp_name);
        success = EXIT_FAILURE;
        goto EXIT;
    }
    // Attempt to get a socket
    if((sock_fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        error("Failed to get a socket\n");
        success = EXIT_FAILURE;
        goto REMOVE_TMP;
    }
    // Attempt to bind to the socket
    memset(&local, 0, sizeof(struct sockaddr_un));
    // Copy the path to the temp file to the local.sun_path
    local.sun_family = AF_LOCAL;
    strcpy(local.sun_path, temp_name);
    // Attempt to bind socket
    if(bind(sock_fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
        error("Failed to bind to the sock_fd: %d\n", sock_fd);
        success = EXIT_FAILURE;
        goto CLOSE_SOCK_FD;
    }
    // If we got this far we can attempt to enter the run loop
    success = run(sock_fd);

CLOSE_SOCK_FD:
    // Attempt to close the open socket
    if(sock_fd >= 0) {
        if(close(sock_fd) > 0) {
            debug("Successfully closed sock_fd: %d\n", sock_fd);
        } else {
            warn("Failed to close sock_fd: %d\n", sock_fd);
        }
    }
REMOVE_TMP:
    // Attempt to remove the temp file
    if(named_pipe >= 0) {
        if(unlink(temp_name) == 0) {
            debug("Sucessfully removed %s\n", temp_name);
        } else {
            warn("Failed to remove %s\n", temp_name);
        }
    }
EXIT:
    return success;
}

int run(int sock_fd) {
    
    return EXIT_SUCCESS;
}
