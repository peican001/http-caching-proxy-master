#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

using namespace std;

class Server {
   public:
    int init(const char *port, char *new_port) {
        int status;
        int socket_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        const char *hostname = NULL;
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        host_info.ai_flags = AI_PASSIVE;
        if (strcmp(port, "") == 0) {
            port = NULL;
            status = getaddrinfo(hostname, "0", &host_info, &host_info_list);
        } else {
            status = getaddrinfo(hostname, port, &host_info, &host_info_list);
        }
        if (status != 0) {
            cerr << "Error: cannot get address info for host" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        socket_fd = socket(host_info_list->ai_family,
                           host_info_list->ai_socktype,
                           host_info_list->ai_protocol);
        if (socket_fd == -1) {
            cerr << "Error: cannot create socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        int yes = 1;
        status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
            cerr << "Error: cannot bind socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        status = listen(socket_fd, 100);
        if (status == -1) {
            cerr << "Error: cannot listen on socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        // get port number
        struct sockaddr_storage socket_addr;
        socklen_t socket_addr_len = sizeof(socket_addr);
        status = getsockname(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
        if (status == -1) {
            cerr << "Error: cannot get socket name" << endl;
            return -1;
        }
        char service[NI_MAXSERV];
        status = getnameinfo((struct sockaddr *)&socket_addr, socket_addr_len, NULL, 0, service, NI_MAXSERV, NI_NUMERICSERV);
        if (status == 0) {
            cout << "Listening on port " << service << endl;
        } else {
            cerr << "Error: cannot get service name" << endl;
        }
        cout << "Waiting for connection on port " << service << endl;

        strcpy(new_port, service);

        freeaddrinfo(host_info_list);

        return socket_fd;
    }
};
class Client {
    public:
    int id;
    int client_fd;
    //struct sockaddr_storage client_addr;
    std::string ip;

    public:
    void setFd(int my_client_fd) { client_fd = my_client_fd; }
    int getFd() { return client_fd; }
    void setIP(std::string myip) { ip = myip; }
    std::string getIP() { return ip; }
    void setID(int myid) { id = myid; }
    int getID() { return id; }
    
   public:
    int init(const char *hostname, const char *port) {
        int status;
        int socket_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        status = getaddrinfo(hostname, port, &host_info, &host_info_list);
        if (status != 0) {
            cerr << "Error: cannot get address info for host (clinet side)" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        socket_fd = socket(host_info_list->ai_family,
                           host_info_list->ai_socktype,
                           host_info_list->ai_protocol);
        if (socket_fd == -1) {
            cerr << "Error: cannot create socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if

        cout << "Connecting to " << hostname << " on port " << port << "..." << endl;

        status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
            cerr << "Error: cannot connect to socket" << endl;
            cerr << "  (" << hostname << "," << port << ")" << endl;
            return -1;
        }  // if
        std::cout << "Connect to server successfully\n";
        freeaddrinfo(host_info_list);

        return socket_fd;
    }
};