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

	//helper funtion for setup
	in_addr_t	_convertIP(const std::string& ip);
	public :
	ServerManager();
	ServerManager(const std::vector<ServerConfig> &configs);
	ServerManager(const ServerManager &other);
	ServerManager &operator=(const ServerManager &other);
	~ServerManager();

	void setupServers();
	void run();
	std::vector<int>			getListenFds() const;
	std::vector<ServerConfig>	getConfigs() const;
};

