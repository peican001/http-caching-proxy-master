#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

// #include <boost/algorithm/string.hpp>
// #include <boost/asio.hpp>
// #include <boost/beast.hpp>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
// using namespace boost::beast;

// namespace beast = boost::beast;
// namespace http = beast::http;
// namespace net = boost::asio;
// using tcp = boost::asio::ip::tcp;
using namespace std;
// using namespace boost;
class Request {
   public:
    std::string method;
    std::string url;
    std::string protocol;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string input;

   public:
    void setinput(std::string Input){
        input = Input;
    }
    
     void parse(const char* data, std::size_t size) {
         // Parse request line
         std::stringstream ss(std::string(data, size));
         ss >> method >> url >> protocol;

         // Parse headers
         std::string header_name, header_value;
         while (std::getline(ss, header_name, ':') && std::getline(ss, header_value)) {
             // Remove leading and trailing spaces from header value
             header_value = header_value.substr(header_value.find_first_not_of(' '),
                                                header_value.find_last_not_of(' ') -
                                                    header_value.find_first_not_of(' ') + 1);
             headers[header_name] = header_value;
         }

         // Check for empty line before body
         auto body_start_pos = std::string(data, size).find("\r\n\r\n");
         if (body_start_pos != std::string::npos && std::string(data, body_start_pos).find("\r\n\r\n") != std::string::npos) {
            // Parse body (if present)
             body = std::string(data + body_start_pos + 4, size - body_start_pos - 4);
         }

     }
  /*
    void parse(const char* request_string) {
        // Parse the request line
        string http_request(request_string);
        std::istringstream request_stream(http_request);
        std::getline(request_stream, method, ' ');
        std::getline(request_stream, url, ' ');
        std::getline(request_stream, protocol, '\r');
        request_stream.ignore(1, '\n');

        // Parse the headers
        std::string header_line;
        while (std::getline(request_stream, header_line, '\r')) {
            if (header_line == "") {
                break;  // End of headers
            }
            std::string::size_type pos = header_line.find(':');
            if (pos != std::string::npos) {
                std::string header_name = header_line.substr(1, pos);
                std::string header_value = header_line.substr(pos + 2, header_line.length() - pos - 4);
                headers[header_name] = header_value;
            }
        }
        request_stream.ignore(1, '\n');

        // Parse the body
        std::getline(request_stream, body);
    }
  */

    void print() {
        std::cout << "method: " << method << endl;
        std::cout << "url: " << url << endl;
        std::cout << "protocal:" << protocol << endl;
        std::cout << "***********head***********" << endl;
        for (const auto& header : headers) {
            std::cout << header.first << ": " << header.second << endl;
        }
        std::cout << "body: " << body << endl;
    }
    std::string gethost() {
        auto it = headers.find("\r\nHost");
        if (it == headers.end()) {
            it = headers.find("Host\n");
        }
        if (it == headers.end()) {
            it = headers.find("\nHost");
        }
        if (it == headers.end()) {
            it = headers.find("Host");
        }
        if (it == headers.end()) {
            it = headers.find("\rHost");
        }
        if (it != headers.end()) {
            return it->second;
        }
        return "can not find host";
    }
};

class timemap {
    public:
    std::map<std::string, int> time_map;

    public:
    timemap() {
        time_map.insert(std::pair<std::string, int>("Jan", 1));
        time_map.insert(std::pair<std::string, int>("Feb", 2));
        time_map.insert(std::pair<std::string, int>("Mar", 3));
        time_map.insert(std::pair<std::string, int>("Apr", 4));
        time_map.insert(std::pair<std::string, int>("May", 5));
        time_map.insert(std::pair<std::string, int>("Jun", 6));
        time_map.insert(std::pair<std::string, int>("Jul", 7));
        time_map.insert(std::pair<std::string, int>("Aug", 8));
        time_map.insert(std::pair<std::string, int>("Sep", 9));
        time_map.insert(std::pair<std::string, int>("Oct", 10));
        time_map.insert(std::pair<std::string, int>("Nov", 11));
        time_map.insert(std::pair<std::string, int>("Dec", 12));
        time_map.insert(std::pair<std::string, int>("Sun", 0));
        time_map.insert(std::pair<std::string, int>("Mon", 1));
        time_map.insert(std::pair<std::string, int>("Tue", 2));
        time_map.insert(std::pair<std::string, int>("Wed", 3));
        time_map.insert(std::pair<std::string, int>("Thu", 4));
        time_map.insert(std::pair<std::string, int>("Fri", 5));
        time_map.insert(std::pair<std::string, int>("Sat", 6));
    }
    int getMap(std::string str) { return time_map.find(str)->second; }
};

class parsetime {
    public:
    struct tm time_struct;

    public:
    parsetime() {}
    void init(std::string exp) {
        timemap mymap;
        time_struct.tm_mday = stoi(exp.substr(5));
        time_struct.tm_mon = mymap.getMap(exp.substr(8, 3).c_str()) - 1;
        time_struct.tm_year = stoi(exp.substr(12)) - 1900;
        time_struct.tm_hour = stoi(exp.substr(17));
        time_struct.tm_min = stoi(exp.substr(20));
        time_struct.tm_sec = stoi(exp.substr(23));
        time_struct.tm_wday = mymap.getMap(exp.substr(0, 3).c_str());
        time_struct.tm_isdst = 0;
    }
    struct tm * getTimeStruct() {
        return &time_struct;
    }
};

class Response {
   public:
    std::string protocol;
    int statusCode;
    std::string message;
    std::map<std::string, std::string> headers;
    std::string body;

    public:
    std::string response;
    std::string line;
    std::string Etag;
    std::string Last_modified;
    int max_age;
    parsetime expire_time;
    std::string exp_str;
    parsetime response_time;
    bool nocache_flag;
    std::string ETag;
    std::string LastModified;

   public:
    // Parse the response from a char array
    void parse(const char* response) {
        // Parse the protocol
        const char* start = response;
        const char* end = strchr(start, ' ');
        if (end == NULL) return;
        protocol.assign(start, end - start);

        // Parse the status code and message
        start = end + 1;
        end = strchr(start, ' ');
        if (end == NULL) return;
        statusCode = atoi(start);
        message.assign(end + 1);

        // Parse the headers and body
        start = strstr(response, "\r\n") + 2;
        while (*start != '\r' && *start != '\n') {
            end = strstr(start, "\r\n");
            if (end == NULL) return;
            const char* colon = strchr(start, ':');
            if (colon == NULL || colon > end) return;
            std::string key(start, colon - start);
            std::string value(colon + 1, end - colon - 1);
            headers[key] = value;
            start = end + 2;
        }

        // Parse the body
        start = strstr(response, "\r\n\r\n") + 4;
        body.assign(start);
    }
    void print() {
        std::cout << protocol << std::endl;
        std::cout << statusCode << std::endl;
        std::cout << message << std::endl;
        for (const auto& header : headers) {
            std::cout << header.first << ": " << header.second << endl;
        }
        std::cout << body << std::endl;
    }
    int length() {
        auto it = headers.find("Content-Length");
        if (it != headers.end()) {
            return stoi(it->second);
        }
        return -1;
    }



    public:
    // Response(std::string msg) : response(msg) {}
    Response() :
        Etag(""),
        Last_modified(""),
        max_age(-1),
        exp_str(""),
        nocache_flag(false),
        ETag(""),
        LastModified("") {}
    std::string getLine() { return line; }
};

