#include <boost/asio.hpp>
#include <iostream>

using namespace boost::asio;
using namespace boost::asio::ip;

int main() {
    io_service io_service;
    tcp::socket socket(io_service);
    tcp::resolver resolver(io_service);

    std::string host = "www.google.com";
    std::string path = "/";
    std::string request = "GET " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Accept: */*\r\n" + "Connection: close\r\n\r\n";

    tcp::resolver::query query(host, "http");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::endpoint endpoint = *endpoint_iterator;

    socket.connect(endpoint);

    boost::asio::streambuf request_buf;
    std::ostream request_stream(&request_buf);
    request_stream << request;

    boost::asio::write(socket, request_buf);

    boost::asio::streambuf response_buf;
    boost::asio::read_until(socket, response_buf, "\r\n");

    std::istream response_stream(&response_buf);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;

    std::string status_message;
    std::getline(response_stream, status_message);

    if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
        std::cout << "Invalid response\n";
        return 1;
    }

    if (status_code != 200) {
        std::cout << "Response returned with status code " << status_code << "\n";
        return 1;
    }

    boost::asio::read_until(socket, response_buf, "\r\n\r\n");

    std::string header;
    while (std::getline(response_stream, header) && header != "\r") {
    }

    std::stringstream ss;
    boost::system::error_code ec;
    boost::asio::read(socket, response_buf, transfer_all(), ec);

    if (ec && ec != boost::asio::error::eof) {
        std::cout << "Error receiving response: " << ec.message() << "\n";
        return 1;
    }

    ss << &response_buf;
    std::string response = ss.str();
    std::cout << response << std::endl;

    return 0;
}
