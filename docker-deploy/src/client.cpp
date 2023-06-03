using namespace std;
#include "server.hpp"
int main(int argc, char** argv) {
    Client client;
    int client_fd = client.init(argv[1], argv[2]);
    const char* msg = "hello from client";
    int num = strlen(msg);
    send(client_fd, &num, sizeof(int), 0);
    send(client_fd, msg, num, 0);
    return 0;
}