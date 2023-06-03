#include "proxy.hpp"

int main(int argc, char **argv) {

    // Fork the parent process
    pid_t pid = fork();
    
    // If the fork failed, exit with an error
    if (pid < 0) {
        std::cerr << "Failed to fork process" << std::endl;
        return 1;
    }
    
    // If the fork succeeded, exit the parent process
    if (pid > 0) {
        return 0;
    }
    
    // Set up the daemon environment
    umask(0);
    setsid();
    chdir("/");
    
    // Redirect standard input, output, and error to /dev/null
    int null_fd = open("/dev/null", O_RDWR);
    dup2(null_fd, STDIN_FILENO);
    dup2(null_fd, STDOUT_FILENO);
    dup2(null_fd, STDERR_FILENO);
    close(null_fd);
    
    // Perform any other daemon-specific initialization here
    
    // Enter the daemon loop
    while (true) {
      run();
    }
    
    
    return 0;
}
