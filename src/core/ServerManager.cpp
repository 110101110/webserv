#include "core/ServerManager.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"
#include "Client.hpp"

// #include "http/HttpRequest.h"

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
			LOG_INFO( "Server socket FD " + Utils::intToString(_listen_fds[i]) + " closed");
		}
	}
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
			LOG_INFO("Virtual host configured for " + _configs[i].host + ":" + Utils::intToString(_configs[i].port));
			continue;
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			throw std::runtime_error("socket() failed");
		int opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
			close(fd); throw std::runtime_error("Failed to set SO_REUSEADDR option");
		}
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0){
			close(fd); throw std::runtime_error("Failed to set non-blocking mode");
		}

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = _convertIP(_configs[i].host);
		addr.sin_port = htons(_configs[i].port);

		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
			close(fd);
			LOG_ERROR("Failed to bind on " + _configs[i].host + ":" + Utils::intToString(_configs[i].port) + " Skipping...");
			continue;
		}
		if (listen(fd, 128) < 0){
			close(fd);
			throw std::runtime_error("listen failed");
		}
		_listen_fds.push_back(fd);
		bound_sockets.push_back(std::make_pair(_configs[i].host, _configs[i].port));

		//adding fd to pollfd for multiplexer
		struct pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pollfds.push_back(pfd);

		LOG_INFO("Server listening on " + _configs[i].host + ":" + Utils::intToString( _configs[i].port) + " FD: " + Utils::intToString(fd));
	}
	if (_listen_fds.empty())
		throw std::runtime_error("Failed to start any servers. Check your configuration file.");
}

void ServerManager::run() {
	LOG_DEBUG("Starting main server loop...");
	while (true){
		//check if actions at a socket
		int ready = poll(&_pollfds[0], _pollfds.size(), -1);
		if (ready < 0){
			throw std::runtime_error("poll() failed");
			break;
		}
		for (int i = _pollfds.size() - 1; i >= 0; i --){
			if (_pollfds[i].revents == 0) continue;
			int currentFd = _pollfds[i].fd;
			//3 cases :case 1 socket error or a client disconnet
			if (_pollfds[i].revents && (POLL_ERR || POLL_HUP || POLLNVAL)){
				LOG_WARNING("Disconnection on FD " + Utils::intToString(currentFd));
				_closeConnection(currentFd);
				continue;
			}
			//case 2, ready to read
			if (_pollfds[i].revents && POLL_IN){
				if (std::find(_listen_fds.begin(), _listen_fds.end(), currentFd) != _listen_fds.end()){
					_acceptNewConnection(currentFd);
				} else{
					_readFromClient(currentFd);
				}
			}
			//case 3, ready too write and have to check the state of client
			if (_pollfds[i].revents && POLL_OUT){
				if (_clients[currentFd].getState() == WRITING_REPONSE){
					_writeToClient(currentFd);
				}
			}
		}
	}
}

