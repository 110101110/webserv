#include "core/ServerManager.hpp"
#include "core/Client.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include "http/HttpRequest.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <cerrno>
#include <csignal>

extern volatile sig_atomic_t g_running;

ServerManager::ServerManager() {}

ServerManager::ServerManager(const std::vector<ServerConfig> &configs) : _configs(configs) {}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _listen_fds.size(); ++i) {
		if (_listen_fds[i] != -1) {
			close(_listen_fds[i]);
			std::stringstream ss;
			ss << "Socket FD " << _listen_fds[i] << " closed properly.";
			Logger::log(Logger::INFO, ss.str());
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
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = _convertIP(_configs[i].host);
		addr.sin_port = htons(_configs[i].port);
		for (int k = 0; k < 8; k++) {
			addr.sin_zero[k] = 0;
		}

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
		_fd_to_port[fd] = _configs[i].port;
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
	while (g_running){
		int ready = poll(&_pollfds[0], _pollfds.size(), 1000); // timeout 1s pour les checks CGI
		if (ready < 0){
			if (errno == EINTR)
				break;
			throw std::runtime_error("poll() failed");
		}
		for (int i = _pollfds.size() - 1; i >= 0; i--){
			short revents = _pollfds[i].revents;
			if (revents == 0) continue;
			int currentFd = _pollfds[i].fd;

			// POLLHUP / POLLERR : pipe CGI fermé (processus terminé) ou erreur client
			if (revents & (POLLERR | POLLHUP | POLLNVAL)){
				if (_cgiContexts.count(currentFd)){
					_readCgiOutput(currentFd); // vide les données restantes puis finalise
				} else {
					LOG_WARNING("Disconnection on FD " + Utils::intToString(currentFd));
					_closeConnection(currentFd);
				}
				continue;
			}
			if (revents & POLLIN){
				if (std::find(_listen_fds.begin(), _listen_fds.end(), currentFd) != _listen_fds.end()){
					_acceptNewConnection(currentFd);
				} else if (_cgiContexts.count(currentFd)){
					_readCgiOutput(currentFd);
				} else {
					_readFromClient(currentFd);
				}
			}
			if (revents & POLLOUT){
				if (_clients.count(currentFd) && _clients[currentFd].getState() == WRITING_RESPONSE){
					_writeToClient(currentFd);
				}
			}
		}
		_checkCgiTimeouts();
	}
}

