#include "core/ServerManager.hpp"
#include "utils/Utils.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

ServerManager::ServerManager() {}

ServerManager::ServerManager(const std::vector<ServerConfig> &configs) : _configs(configs) {}

ServerManager::ServerManager(const ServerManager &other) { *this = other; }

ServerManager &ServerManager::operator=(const ServerManager &other)
{
	if (this != &other)
	{
		this->_configs = other._configs;
		this->_listen_fds = other._listen_fds;
	}
	return *this;
}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _listen_fds.size(); ++i) {
		if (_listen_fds[i] != -1) {
			close(_listen_fds[i]);
			std::cout << "Socket FD " << _listen_fds[i] << " fermé proprement." << std::endl;
		}
	}
}

in_addr_t ServerManager::_convertIP(const std::string& ip) {
	in_addr_t result = 0;
	std::stringstream ss(ip);
	std::string segment;
	int count = 0;

	while (std::getline(ss, segment, '.')) {
		if (segment.empty() || segment.length() > 3 || count >= 4 || !Utils::isNumber(segment))
			throw std::runtime_error("Format IP invalide : " + ip);
		int val = std::atoi(segment.c_str());
		if (val < 0 || val > 255) throw std::runtime_error("Valeur IP hors limite (0-255)");
		result = (result << 8) + val;
		count++;
	}
	if (count != 4) throw std::runtime_error("IP incomplète (4 segments attendus)");
	return htonl(result);
}

void ServerManager::setupServers() {
	std::vector< std::pair<std::string, int> > bound_sockets;

	for (size_t i = 0; i < _configs.size(); ++i) {
		bool already_bound = false;
		for (size_t j = 0; j < bound_sockets.size(); ++j) {
			if (bound_sockets[j].first == _configs[i].host && bound_sockets[j].second == _configs[i].port) {
				already_bound = true; break;
			}
		}
		if (already_bound) {
			std::cout << " Virtual Host (IP:Port déjà bind) pour " << _configs[i].host << ":" << _configs[i].port << std::endl;
			continue;
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) throw std::runtime_error("Erreur socket()");

		int opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) throw std::runtime_error("setsockopt failed");
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { close(fd); throw std::runtime_error("fcntl failed"); }

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = _convertIP(_configs[i].host);
		addr.sin_port = htons(_configs[i].port);

		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			close(fd);
			std::cerr << "Impossible de bind sur " << _configs[i].host << ":" << _configs[i].port << " (OS). Skip..." << std::endl;
			continue;
		}
		if (listen(fd, 128) < 0) { close(fd); throw std::runtime_error("listen failed"); }

		_listen_fds.push_back(fd);
		bound_sockets.push_back(std::make_pair(_configs[i].host, _configs[i].port));
		std::cout << "📡 Socket " << fd << " à l'écoute sur " << _configs[i].host << ":" << _configs[i].port << std::endl;
	}
	if (_listen_fds.empty())
		throw std::runtime_error("Aucun serveur n'a pu être lancé.");
}

std::vector<int> ServerManager::getListenFds() const { return _listen_fds; }

std::vector<ServerConfig> ServerManager::getConfigs() const { return _configs; }
