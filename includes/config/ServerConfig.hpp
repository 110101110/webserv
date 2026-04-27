#pragma once

#include <vector>
#include <string>
#include <map>

struct Location {
	std::string path;
	std::string root;
	std::vector<std::string> methods;
	std::string index;
	bool autoindex;
	std::string return_url;
	std::string upload_store;
	std::string cgi_ext;
	std::string cgi_path;

	Location() : autoindex(false) {}
};

class ServerConfig {
public:
	int port;
	std::string host;
	std::string server_name;
	size_t client_max_body_size;
	
	std::string root;
	std::string index;
	
	std::map<int, std::string> error_pages;
	std::vector<Location> locations;

	ServerConfig() : port(8080), host("127.0.0.1"), client_max_body_size(1000000), root(""), index("") {}
};
