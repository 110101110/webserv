#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

struct Location {
	std::string path;
	std::string root;
	std::vector<std::string> methods;
	std::string index;
	bool autoindex;

	Location() : autoindex(false) {}
};

class ServerConfig {
public:
	int port;
	std::string host;
	std::string server_name;
	size_t client_max_body_size;
	std::map<int, std::string> error_pages;
	std::vector<Location> locations;

	ServerConfig() : port(8080), host("127.0.0.1"), client_max_body_size(1000000) {}
};

#endif
