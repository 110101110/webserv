#pragma once

#include "config/ServerConfig.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <netinet/in.h>

class ServerManager {
	private:
	std::vector<ServerConfig>	_configs;
	std::vector<int>			_listen_fds;
	in_addr_t	_convertIP(const std::string& ip);
	public :
	ServerManager();
	ServerManager(const std::vector<ServerConfig> &configs);
	ServerManager(const ServerManager &other);
	ServerManager &operator=(const ServerManager &other);
	~ServerManager();
	void	setupServers();
	std::vector<int>			getListenFds() const;
	std::vector<ServerConfig>	getConfigs() const;
};

