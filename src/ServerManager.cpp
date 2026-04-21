#include "ServerManager.hpp"
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _listen_fds.size(); ++i) {
		if (_listen_fds[i] != -1) {
			close(_listen_fds[i]);
			std::cout << "Socket FD " << _listen_fds[i] << " fermé." << std::endl;
		}
	}
}

in_addr_t ServerManager::convertIP(const std::string& ip) {
	in_addr_t result = 0;
	int current_part = 0;
	int shift = 24;

	for (size_t i = 0; i <= ip.length(); ++i) {
		if (ip[i] == '.' || i == ip.length()) {
			result |= (current_part << shift);
			shift -= 8;
			current_part = 0;
		} else {
			current_part = current_part * 10 + (ip[i] - '0');
		}
	}
	return htonl(result);
}

void ServerManager::parseConfig(std::string filename) {
	(void)filename;
	ServerConfig config;
	config.port = 8080;
	config.host = "127.0.0.1";
	config.server_name = "localhost";
	
	Location loc;
	loc.path = "/";
	loc.root = "./www";
	loc.methods.push_back("GET");
	
	config.locations.push_back(loc);
	_configs.push_back(config);
	std::cout << "Config chargée : Port " << config.port << std::endl;
}

void ServerManager::setupServers() {
	std::vector<int> bound_ports;

	for (size_t i = 0; i < _configs.size(); ++i) {
		// Vérification pour ne pas bind() deux fois le même port
		bool already_bound = false;
		for (size_t j = 0; j < bound_ports.size(); ++j) {
			if (bound_ports[j] == _configs[i].port) {
				already_bound = true;
				break;
			}
		}
		if (already_bound) {
			std::cout << "Port " << _configs[i].port << " déjà sur écoute, on passe..." << std::endl;
			continue;
		}

		int fd;
		struct sockaddr_in addr;
		int opt = 1;

		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			throw std::runtime_error("Erreur socket");

		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		fcntl(fd, F_SETFL, O_NONBLOCK);

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = convertIP(_configs[i].host);
		addr.sin_port = htons(_configs[i].port);

		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			close(fd);
			throw std::runtime_error("Erreur bind");
		}

		if (listen(fd, 128) < 0) {
			close(fd);
			throw std::runtime_error("Erreur listen");
		}

		_listen_fds.push_back(fd);
		bound_ports.push_back(_configs[i].port);
		std::cout << "Le serveur écoute sur le FD: " << fd << " (Port: " << _configs[i].port << ")" << std::endl;
	}
}

std::vector<int> ServerManager::getListenFds() const {
	return _listen_fds;
}

std::vector<ServerConfig> ServerManager::getConfigs() const {
	return _configs;
}
