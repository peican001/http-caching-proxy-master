#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "http.hpp"
#include "server.hpp"
using namespace std;
#define PORT "12345"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
std::ofstream logFile("/var/log/erss/proxy.log");
//std::ofstream logFile("./proxy.log");
std::unordered_map<std::string, Response> Cache;

void handleGet(int client_connection_fd, int to_server_fd, const char *msg, int length, int id, const char *hostname, std::string req_line);
void handlePOST(int client_connection_fd, int to_server_fd, const char *msg, int length, int id, const char *hostname, std::string req_line);
void handleConnect(int client_connection_fd, int to_server_fd, int id, const char *hostname);

void parsefield(Response& res, char * first_msg, int len){
    std::string msg(first_msg, len);
    size_t date_pos;
    if ((date_pos = msg.find("Date: ")) != std::string::npos) {
        size_t GMT_pos = msg.find(" GMT");
        std::string date_str = msg.substr(date_pos + 6, GMT_pos - date_pos - 6);
        res.response_time.init(date_str);
    }
    size_t max_age_pos;
    if ((max_age_pos = msg.find("max-age=")) != std::string::npos) {
        std::string max_age_str = msg.substr(max_age_pos + 8);
        res.max_age = atoi(max_age_str.c_str());
    }
    size_t expire_pos;
    if ((expire_pos = msg.find("Expires: ")) != std::string::npos) {
        size_t GMT_pos = msg.find(" GMT");
        res.exp_str = msg.substr(expire_pos + 9, GMT_pos - expire_pos - 9);
        res.expire_time.init(res.exp_str);
    }
    size_t nocatch_pos;
    if ((nocatch_pos = msg.find("no-cache")) != std::string::npos) {
        res.nocache_flag = true;
    }
    size_t etag_pos;
    if ((etag_pos = msg.find("ETag: ")) != std::string::npos) {
        size_t etag_end = msg.find("\r\n", etag_pos + 6);
        res.ETag = msg.substr(etag_pos + 6, etag_end - etag_pos - 6);
    }
    size_t lastmodified_pos;
    if ((lastmodified_pos = msg.find("Last-Modified: ")) != std::string::npos) {
        size_t lastmodified_end = msg.find("\r\n", lastmodified_pos + 15);
        res.LastModified =
            msg.substr(lastmodified_pos + 15, lastmodified_end - lastmodified_pos - 15);
    }
}



void printnote(char *first_msg, int len, int id) {
    // parse
    std::string msg(first_msg, len);
    size_t date_pos;

    std::string Etag;
    std::string Last_modified;
    int max_age;
    parsetime expire_time;
    std::string exp_str;
    parsetime response_time;
    bool nocache_flag;
    std::string ETag;
    std::string LastModified;
    if ((date_pos = msg.find("Date: ")) != std::string::npos) {
        size_t GMT_pos = msg.find(" GMT");
        std::string date_str = msg.substr(date_pos + 6, GMT_pos - date_pos - 6);
        response_time.init(date_str);
    }
    size_t max_age_pos;
    if ((max_age_pos = msg.find("max-age=")) != std::string::npos) {
        std::string max_age_str = msg.substr(max_age_pos + 8);
        max_age = atoi(max_age_str.c_str());
    }
    size_t expire_pos;
    if ((expire_pos = msg.find("Expires: ")) != std::string::npos) {
        size_t GMT_pos = msg.find(" GMT");
        exp_str = msg.substr(expire_pos + 9, GMT_pos - expire_pos - 9);
        expire_time.init(exp_str);
    }
    size_t nocatch_pos;
    if ((nocatch_pos = msg.find("no-cache")) != std::string::npos) {
        nocache_flag = true;
    }
    size_t etag_pos;
    if ((etag_pos = msg.find("ETag: ")) != std::string::npos) {
        size_t etag_end = msg.find("\r\n", etag_pos + 6);
        ETag = msg.substr(etag_pos + 6, etag_end - etag_pos - 6);
    }
    size_t lastmodified_pos;
    if ((lastmodified_pos = msg.find("Last-Modified: ")) != std::string::npos) {
        size_t lastmodified_end = msg.find("\r\n", lastmodified_pos + 15);
        LastModified =
            msg.substr(lastmodified_pos + 15, lastmodified_end - lastmodified_pos - 15);
    }

    // check
    if (max_age != -1) {
        pthread_mutex_lock(&mutex);
        logFile << id << ": NOTE Cache-Control: max-age=" << max_age << std::endl;
        pthread_mutex_unlock(&mutex);
    }
    if (exp_str != "") {
        pthread_mutex_lock(&mutex);
        logFile << id << ": NOTE Expires: " << exp_str << std::endl;
        pthread_mutex_unlock(&mutex);
    }
    if (nocache_flag == true) {
        pthread_mutex_lock(&mutex);
        logFile << id << ": NOTE Cache-Control: no-cache" << std::endl;
        pthread_mutex_unlock(&mutex);
    }
    if (ETag != "") {
        pthread_mutex_lock(&mutex);
        logFile << id << ": NOTE ETag: " << ETag << std::endl;
        pthread_mutex_unlock(&mutex);
    }
    if (LastModified != "") {
        pthread_mutex_lock(&mutex);
        logFile << id << ": NOTE Last-Modified: " << LastModified << std::endl;
        pthread_mutex_unlock(&mutex);
    }
}

void *handle(void *info) {

    Client *client_info = (Client *)info;
    int client_connection_fd = client_info->getFd();

    char msg[5000];
    // recv request from client
    int len = recv(client_connection_fd, msg, 5000, 0);

    if (len <= 0) {
        pthread_mutex_lock(&mutex);
        logFile << client_info->getID() << ": WARNING Invalid Request" << std::endl;
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    int id = client_info->getID();

    msg[5000] = '\0';
    int length = strlen(msg);
    Request request;
    request.parse(msg, length);
    if (request.method != "POST" && request.method != "GET" &&
        request.method != "CONNECT") {
        const char *req400 = "HTTP/1.1 400 Bad Request";
        pthread_mutex_lock(&mutex);
        logFile << id << ": Responding \"" << req400 << "\"" << std::endl;
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    Client client;
    // extract host name from request
    string hostname_str = request.gethost();
    hostname_str[hostname_str.size() - 1] = '\0';
    const char *hostname = hostname_str.c_str();
    //std::cout << "test: " << hostname << std::endl;

    
    std::string input = std::string(msg, len);
    /*
    if (input == "" || input == "\r" || input == "\n" || input == "\r\n") {
        return NULL;
    }
    */

    // get line
    size_t pos1 = input.find_first_of("\r\n");
    std::string parser_line = input.substr(0, pos1);

    // get time
    time_t curr_time = time(0);
    struct tm *nowTime = gmtime(&curr_time);
    const char *t = asctime(nowTime);
    std::string Time = std::string(t).append("\0");

    pthread_mutex_lock(&mutex);
    cout << "fk!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
    logFile << client_info->getID() << ": \"" << parser_line << "\" from "
            << client_info->getIP() << " @ " << Time;
    pthread_mutex_unlock(&mutex);
    std::cout << "received client request is:" << msg << "end" << std ::endl;

    // handling response from server
    int to_server_fd;
    char response[60000];
    if (strcmp(request.method.c_str(), "GET") == 0) {
        to_server_fd = client.init(hostname, "80");
        if (to_server_fd != -1) {
            std::cout << "connection to " << hostname << " succeed" << endl;
        }
        // int id = client_info->getID();
        bool valid = false;
        std::string changed_input = input;
        std::unordered_map<std::string, Response>::iterator it = Cache.begin();
        it = Cache.find(parser_line);
        if (it == Cache.end()) {
            pthread_mutex_lock(&mutex);
            logFile << id << ": not in cache" << std::endl;
            pthread_mutex_unlock(&mutex);
            pthread_mutex_lock(&mutex);
            logFile << id << ": "
                    << "Requesting \"" << parser_line << "\" from " << hostname << std::endl;
            pthread_mutex_unlock(&mutex);
            send(to_server_fd, msg, len, 0);
            handleGet(client_connection_fd, to_server_fd, msg, length, id, hostname, parser_line);
        } else {                            // find in cache
            if (it->second.nocache_flag) {  // has no-cache symbol
                bool flag = true;
                
                if (it->second.ETag != "" || it->second.LastModified != "") {
                    
                    if (it->second.ETag != "") {
                        std::string add_etag = "If-None-Match: " + it->second.ETag.append("\r\n");
                        changed_input = changed_input.insert(changed_input.length() - 2, add_etag);
                    }
                    if (it->second.LastModified != "") {
                        std::string add_modified = "If-Modified-Since: " + it->second.LastModified.append("\r\n");
                        changed_input = changed_input.insert(changed_input.length() - 2, add_modified);
                    }
                    std::string req_msg_str = changed_input;
                    int new_msg_len = changed_input.size() + 1;
                    char req_new_msg[new_msg_len];
                    int send_len = send(to_server_fd, req_new_msg, new_msg_len, 0);
                    char new_resp[65536] = {0};
                    int new_len = recv(to_server_fd, &new_resp, sizeof(new_resp), 0);
                    
                    std::string checknew(new_resp, new_len);
                    if (checknew.find("HTTP/1.1 200 OK") != std::string::npos) {
                        pthread_mutex_lock(&mutex);
                        logFile << id << ": in cache, requires validation" << std::endl;
                        pthread_mutex_unlock(&mutex);
                        flag = false;
                    }
                    /*
                    else {
                        flag = true; 
                    }
                    */
                    
                }
                else {
                    flag = true;
                }
                //if (revalidation(it->second, input, to_server_fd, id) == false) {  // check Etag and Last Modified
                if (flag == false){
                    // ask_server(id, parser_line, req_msg, len, client_fd, server_fd, host);
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": "
                            << "Requesting \"" << parser_line << "\" from " << hostname << std::endl;
                    pthread_mutex_unlock(&mutex);

                    send(to_server_fd, msg, len, 0);
                    handleGet(client_connection_fd, to_server_fd, msg, length, id, hostname, parser_line);
                } else {
                    // use_cache(it->second, id, client_fd);
                    char cache_res[it->second.response.length()];
                    strcpy(cache_res, it->second.response.c_str());
                    send(client_connection_fd, cache_res, it->second.response.length(), 0);
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": Responding \"" << it->second.line << "\"" << std::endl;
                    pthread_mutex_unlock(&mutex);
                }
            } else {
            
                time_t rep_time = mktime(it->second.response_time.getTimeStruct());
                int max_age = it->second.max_age;
                time_t expire_time;
                time_t dead_time;
                struct tm *asc_time;
                const char *t;

                if ((it->second.max_age != -1 && rep_time + max_age <= curr_time) || (it->second.exp_str != "" && curr_time > expire_time)) {
                    if (it->second.max_age != -1 && rep_time + max_age <= curr_time){
                        dead_time = mktime(it->second.response_time.getTimeStruct()) + it->second.max_age;
                    }
                    else{
                        expire_time = mktime(it->second.expire_time.getTimeStruct());
                        dead_time = mktime(it->second.expire_time.getTimeStruct());
                    }
                    Cache.erase(parser_line);
                    asc_time = gmtime(&dead_time);
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": in cache, but expired at " << t;
                    pthread_mutex_unlock(&mutex);
                    valid = false;
                }
                else{

                    //valid = revalidation(it->second, input, to_server_fd, id);
                    if (it->second.ETag != "" || it->second.LastModified != "") {
                        
                        if (it->second.ETag != "") {
                            std::string add_etag = "If-None-Match: " + it->second.ETag.append("\r\n");
                            changed_input = changed_input.insert(changed_input.length() - 2, add_etag);
                        }
                        if (it->second.LastModified != "") {
                            std::string add_modified = "If-Modified-Since: " + it->second.LastModified.append("\r\n");
                            changed_input = changed_input.insert(changed_input.length() - 2, add_modified);
                        }
                        std::string req_msg_str = changed_input;
                        int new_msg_len = changed_input.size() + 1;
                        char req_new_msg[new_msg_len];
                        //const char *req_new_msg = changed_input.c_str();
                        //size_t new_msg_len = sizeof(req_new_msg);
                        int send_len = send(to_server_fd, req_new_msg, new_msg_len, 0);
                        char new_resp[65536] = {0};
                        int new_len = recv(to_server_fd, &new_resp, sizeof(new_resp), 0);
                        
                        std::string checknew(new_resp, new_len);
                        if (checknew.find("HTTP/1.1 200 OK") != std::string::npos) {
                            pthread_mutex_lock(&mutex);
                            logFile << id << ": in cache, requires validation" << std::endl;
                            pthread_mutex_unlock(&mutex);
                            valid = false;
                        }
                        
                        else {
                            valid = true; 
                        }
                        
                        
                    }
                    else {
                        valid = true;
                    }
                    if (valid != false) {
                        pthread_mutex_lock(&mutex);
                        logFile << id << ": in cache, valid" << std::endl;
                        pthread_mutex_unlock(&mutex);
                        valid = true;
                    }
                }
                if (!valid) {  // ask for server,check res and put in cache if needed
                    // ask_server(id, parser_line, req_msg, len, client_fd, server_fd, host);
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": "
                            << "Requesting \"" << parser_line << "\" from " << hostname << std::endl;
                    pthread_mutex_unlock(&mutex);

                    send(to_server_fd, msg, len, 0);
                    handleGet(client_connection_fd, to_server_fd, msg, length, id, hostname, parser_line);
                } else {  // send from cache
                    // use_cache(it->second, id, client_fd);
                    char cache_res[it->second.response.length()];
                    strcpy(cache_res, it->second.response.c_str());
                    send(client_connection_fd, cache_res, it->second.response.length(), 0);
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": Responding \"" << it->second.line << "\"" << std::endl;
                    pthread_mutex_unlock(&mutex);
                }
            }
    
        }

    } else if (strcmp(request.method.c_str(), "POST") == 0) {
        to_server_fd = client.init(hostname, "80");
        if (to_server_fd != -1) {
            std::cout << "connection to " << hostname << " succeed" << endl;
        }
        pthread_mutex_lock(&mutex);
        logFile << id << ": "
                << "Requesting \"" << parser_line << "\" from " << hostname << std::endl;
        pthread_mutex_unlock(&mutex);
        handlePOST(client_connection_fd, to_server_fd, msg, length, id, hostname, parser_line);
    } else if (strcmp(request.method.c_str(), "CONNECT") == 0) {
        string host(hostname);
        int prefix = host.find(":");
        host = host.substr(0, prefix);
        to_server_fd = client.init(host.c_str(), "443");
        if (to_server_fd != -1) {
            std::cout << "connection to " << hostname << " succeed" << endl;
        }
        pthread_mutex_lock(&mutex);
        logFile << id << ": "
                << "Requesting \"" << parser_line << "\" from " << hostname << std::endl;
        pthread_mutex_unlock(&mutex);
        handleConnect(client_connection_fd, to_server_fd, id, hostname);
        pthread_mutex_lock(&mutex);
        logFile << id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
    }
    close(to_server_fd);
    close(client_connection_fd);
    return NULL;
}

void handleGet(int client_connection_fd, int to_server_fd, const char *msg, int length, int id, const char *hostname, std::string req_line) {
    send(to_server_fd, msg, length, 0);
    std::cout << "msg sent" << endl;
    cout << msg << endl;
    char response[BUFSIZ];
    std::vector<char> wholeResponse;
    int wholeLen = 0;

    Response httpresponse;
    int byte_receive;
    byte_receive = recv(to_server_fd, response, BUFSIZ, 0);
    httpresponse.parse(response);
    cout << "response is: " << response << endl;
    if (byte_receive == 0) {
        return;
    }

    wholeResponse.insert(wholeResponse.end(), response, response+strlen(response));
    wholeLen += byte_receive;

    std::string input = std::string(response, byte_receive);
    // get line
    size_t pos1 = input.find_first_of("\r\n");
    std::string parser_line = input.substr(0, pos1);
    httpresponse.line = parser_line;

    pthread_mutex_lock(&mutex);
    logFile << id << ": Received \"" << parser_line << "\" from " << hostname
            << std::endl;
    pthread_mutex_unlock(&mutex);
    

    int Remaining_length;
    int oversized = 0;
    if (httpresponse.length() != -1) {
        oversized = 1;
        Remaining_length = httpresponse.length() - byte_receive;
    }


    bool no_store = false;
    size_t nostore_pos;
    if ((nostore_pos = input.find("no-store")) != std::string::npos) {
        no_store = true;
    }
    if (oversized != 0){
        printnote(response, byte_receive, id);
    }

    if (response[0] == '0') {
        string ending = "0\r\n\r\n";
        send(client_connection_fd, ending.c_str(), ending.size(), 0);

        return;
    }
    std::cout << response << endl;
    int byte_send = send(client_connection_fd, response, byte_receive, 0);
    // when msg can be recv() at the first place

    if (Remaining_length <= 0 && oversized == 1) {

        // printcachelog(parse_res, no_store, req_line, id);
            //httpresponse.ParseField(response, byte_receive);
            parsefield(httpresponse, response, byte_receive);
            httpresponse.response = input;

            int max_age;
            size_t max_age_pos;
            if ((max_age_pos = input.find("max-age=")) != std::string::npos) {
                std::string max_age_str = input.substr(max_age_pos + 8);
                max_age = atoi(max_age_str.c_str());
            }

            std::string exp_str;
            parsetime expire_time;
            size_t expire_pos;
            if ((expire_pos = input.find("Expires: ")) != std::string::npos) {
                size_t GMT_pos = input.find(" GMT");
                exp_str = input.substr(expire_pos + 9, GMT_pos - expire_pos - 9);
                expire_time.init(exp_str);
            }

            parsetime response_time;
          

            if (input.find("HTTP/1.1 200 OK") != std::string::npos) {
           

                if (no_store) {
                    pthread_mutex_lock(&mutex);
                    logFile << id << ": not cacheable becaues NO STORE" << std::endl;
                    pthread_mutex_unlock(&mutex);
                }
                else{
                    if (max_age != -1) {
                        time_t dead_time =
                            mktime(httpresponse.response_time.getTimeStruct()) + max_age;
                        struct tm *asc_time = gmtime(&dead_time);
                        const char *t = asctime(asc_time);
                        pthread_mutex_lock(&mutex);
                        logFile << id << ": cached, expires at " << t << std::endl;
                        pthread_mutex_unlock(&mutex);
                    } else if (exp_str != "") {
                        pthread_mutex_lock(&mutex);
                        logFile << id << ": cached, expires at " << exp_str << std::endl;
                        pthread_mutex_unlock(&mutex);
                    }
                    //Response storedres(httpresponse);
                    if (Cache.size() > 10) {
                        std::unordered_map<std::string, Response>::iterator it = Cache.begin();
                        Cache.erase(it);
                    }
                    Cache.insert(std::pair<std::string, Response>(req_line, httpresponse));
                }
            }

        std::cout << "Responding for GET\n";

        pthread_mutex_lock(&mutex);
        std::cout << "logfile responding\n";
        logFile << id << ": Responding \"" << parser_line << "\"" << std::endl;
        //logFile << id << ": test: cache size is: " << Cache.size() << std::endl;
        //logFile << id << ": test: input is: " << input << std::endl;
        pthread_mutex_unlock(&mutex);

        return;
    }
    if (oversized == 0){
            pthread_mutex_lock(&mutex);
            logFile << id << ": not cacheable because it is chunked" << std::endl;
            pthread_mutex_unlock(&mutex);
    }
    

    while (byte_receive > 0) {
    //if (byte_receive > 0) {
        // some problem here !!!
        // code block when no message is to be recv
        byte_receive = recv(to_server_fd, response, BUFSIZ, 0);

        wholeResponse.insert(wholeResponse.end(), response, response+strlen(response));
        wholeLen += byte_receive;

        if (response[0] == '0') {
            string ending = "0\r\n\r\n";
            send(client_connection_fd, ending.c_str(), ending.size(), 0);

            break;
        }
        if (oversized == 1) {
            Remaining_length -= byte_receive;
            if (Remaining_length <= 0) {

                break;
            }
        }

        response[BUFSIZ] = '\0';
        //std::cout << response << endl;
        int byte_send = send(client_connection_fd, response, byte_receive, 0);

    }
    

    std::cout << "Responding for GET\n";

    pthread_mutex_lock(&mutex);
    std::cout << "logfile responding\n";
    logFile << id << ": Responding \"" << parser_line << "\"" << std::endl;
    //logFile << id << ": test: cache size is: " << Cache.size() << std::endl;
    //logFile << id << ": test: input is: " << input << std::endl;
    pthread_mutex_unlock(&mutex);
}

void handlePOST(int client_connection_fd, int to_server_fd, const char *msg, int length, int id, const char *hostname, std::string req_line) {
    send(to_server_fd, msg, length, 0);
    std::cout << "msg sent" << endl;
    cout << msg << endl;
    char response[BUFSIZ];
    Response httpresponse;
    int byte_receive;
    byte_receive = recv(to_server_fd, response, BUFSIZ, 0);
    httpresponse.parse(response);

    pthread_mutex_lock(&mutex);
    logFile << id << ": Received \"" << req_line << "\" from " << hostname
            << std::endl;
    pthread_mutex_unlock(&mutex);

    //std::cout << "receive response from server which is:" << response << std::endl;

    //std::cout << response << endl;
    int byte_send = send(client_connection_fd, response, byte_receive, 0);

    pthread_mutex_lock(&mutex);
    logFile << id << ": Responding \"" << req_line << std::endl;
    pthread_mutex_unlock(&mutex);
}

void handleConnect(int client_connection_fd, int to_server_fd, int id, const char *hostname) {
    std::cout << "method is CONNECT" << endl;
    send(client_connection_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);

    pthread_mutex_lock(&mutex);
    logFile << id << ": Responding \"HTTP/1.1 200 OK\"" << std::endl;
    pthread_mutex_unlock(&mutex);

    Client client;

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = to_server_fd >= client_connection_fd ? to_server_fd : client_connection_fd;
        FD_SET(to_server_fd, &read_fds);
        FD_SET(client_connection_fd, &read_fds);
        char message[65536];
        struct timeval timeout;
        timeout.tv_sec = 5;  // set timeout to 5 seconds
        timeout.tv_usec = 0;
        if (select(max_fd + 1, &read_fds, NULL, NULL, &timeout) == -1) {
            perror("select() error");
            break;
        } else if (select(max_fd + 1, &read_fds, NULL, NULL, &timeout) == 0) {
            // select timed out
            std::cout << "select() timed out" << endl;
            return;
        }
        if (FD_ISSET(to_server_fd, &read_fds)) {
            int byte_recv = recv(to_server_fd, message, 65536, 0);
            if (byte_recv == 0) {
                break;
            } else if (byte_recv == -1) {
                perror("recv() error");
                break;
            }
            send(client_connection_fd, message, byte_recv, 0);
        }
        if (FD_ISSET(client_connection_fd, &read_fds)) {
            int byte_recv = recv(client_connection_fd, message, 65536, 0);
            if (byte_recv == 0) {
                break;
            } else if (byte_recv == -1) {
                perror("recv() error");
                break;
            }
            send(to_server_fd, message, byte_recv, 0);
        }
    }
    // close(to_server_fd);
    // close(client_connection_fd);
}

void run() {
    Server server;
    char ptr[512];
    int server_fd = server.init(PORT, ptr);
    char msg[5000];
    int num;
    int id = 0;
    const char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nWhat the fuck!";
    int client_connection_fd;
    while (1) {
        struct sockaddr_storage socket_addr;
        socklen_t socket_addr_len = sizeof(socket_addr);
        client_connection_fd = accept(server_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
        if (client_connection_fd == -1) {
            cerr << "Error: cannot accept connection on socket" << endl;
            break;
        }  // if

        struct sockaddr_in *addr = (struct sockaddr_in *)&socket_addr;
        std::string ip = inet_ntoa(addr->sin_addr);

        pthread_t thread;
        pthread_mutex_lock(&mutex);
        Client *newclient = new Client();
        newclient->client_fd = client_connection_fd;
        newclient->ip = ip;
        newclient->id = id;
        id++;
        pthread_mutex_unlock(&mutex);
        pthread_create(&thread, NULL, handle, newclient);
        //handle(newclient);

        
    }
}
