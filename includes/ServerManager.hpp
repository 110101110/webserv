#ifndef SERVER_MANAGER_HPP
#define SERVER_MANAGER_HPP

#include "config.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <netinet/in.h>

class ServerManager {
private:
	std::vector<ServerConfig>	_configs;
	std::vector<int>			_listen_fds;

	in_addr_t	convertIP(const std::string& ip);
	std::string	cleanToken(std::string str);

public:
	ServerManager();
	~ServerManager();

	void	parseConfig(std::string filename);
	void	setupServers();

	std::vector<int>			getListenFds() const;
	std::vector<ServerConfig>	getConfigs() const;
};

#endif
