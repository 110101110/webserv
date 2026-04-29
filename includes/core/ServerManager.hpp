#pragma once

#include "core/Client.hpp"
#include "config/ServerConfig.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>

class ServerManager {
	private:
	std::vector<ServerConfig>	_configs;
	std::vector<int>			_listen_fds;
	std::vector<struct pollfd>	_pollfds;
	std::map<int, Client> _clients;

	//helper funtion for setup
	in_addr_t	_convertIP(const std::string& ip);

	//helper function for run
	void	_acceptNewConnection(int serverFd);
	void	_readFromClient(int clientFd);
	void	_writeToClient(int clientFd);
	void	_closeConnection(int clientFd);
	bool	_isRequestComplete(const std::string& buffer);

	public :
	ServerManager();
	ServerManager(const std::vector<ServerConfig> &configs);
	ServerManager(const ServerManager &other);
	ServerManager &operator=(const ServerManager &other);
	~ServerManager();

	void setupServers();
	void run();
};

