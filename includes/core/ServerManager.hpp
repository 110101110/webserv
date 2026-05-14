#pragma once

#include "core/Client.hpp"
#include "config/ServerConfig.hpp"
#include "http/CgiContext.hpp"
#include "http/HttpRequest.hpp"
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>

static const int CGI_TIMEOUT_SEC = 7;

class ServerManager {
	private:
	std::vector<ServerConfig>	_configs;
	std::vector<int>			_listen_fds;
	std::map<int, int>			_fd_to_port; // listen_fd → port
	std::vector<struct pollfd>	_pollfds;
	std::map<int, Client>		_clients;
	std::map<int, CgiContext>	_cgiContexts; // clé : pipe_out fd

	//helper funtion for setup
	in_addr_t	_convertIP(const std::string& ip);
	size_t		_findConfig(int serverFd, const HttpRequest &req) const;

	//helper function for run
	void	_acceptNewConnection(int serverFd);
	void	_readFromClient(int clientFd);
	void	_writeToClient(int clientFd);
	void	_closeConnection(int clientFd);
	bool	_isRequestComplete(const std::string& buffer);

	// CGI non-bloquant
	void	_readCgiOutput(int pipeFd);
	void	_finalizeCgi(int pipeFd);
	void	_checkCgiTimeouts();

	public :
	ServerManager();
	ServerManager(const std::vector<ServerConfig> &configs);
	ServerManager(const ServerManager &other);
	~ServerManager();

	void setupServers();
	void run();
};

