#include "core/ServerManager.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"
#include "core/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/RequestHandler.hpp"
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <string>


size_t ServerManager::_findConfig(int serverFd, const HttpRequest &req) const
{
	int port = -1;

	std::map<int, int>::const_iterator fd_it = _fd_to_port.find(serverFd);
	if (fd_it != _fd_to_port.end())
		port = fd_it->second;

	std::string host_header;

	std::map<std::string, std::string>::const_iterator h =
		req.getHeader().find("host");

	if (h != req.getHeader().end())
	{
		host_header = Utils::cleanToken(h->second);
		size_t pos = host_header.find(':');
		if (pos != std::string::npos)
			host_header = host_header.substr(0, pos);
	}

	size_t fallback = (size_t)-1;

	for (size_t i = 0; i < _configs.size(); i++)
	{
		if (_configs[i].port != port)
			continue;

		if (fallback == (size_t)-1)
			fallback = i;

		for (size_t j = 0; j < _configs[i].server_names.size(); j++)
		{
			if (_configs[i].server_names[j] == host_header)
				return i;
		}
	}

	if (fallback == (size_t)-1)
		return 0;

	return fallback;
}

in_addr_t ServerManager::_convertIP(const std::string &ip)
{
	in_addr_t result = 0;
	std::stringstream ss(ip);
	std::string segment;
	int count = 0;

	while (std::getline(ss, segment, '.'))
	{
		if (segment.empty() || segment.length() > 3 || count >= 4 || !Utils::isNumber(segment))
			LOG_ERROR("Invalid IP format: " + ip);
		int val = std::atoi(segment.c_str());
		if (val < 0 || val > 255)
			LOG_ERROR("IP value out of range (0-255)");
		result = (result << 8) + val;
		count++;
	}
	if (!Utils::isValidIP(ip))
		LOG_ERROR("Invalid IP");
	return htonl(result);
}

bool ServerManager::_isRequestComplete(const std::string &buffer)
{
	size_t headerEnd = buffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return false;
	size_t contentLengthPos = buffer.find("Content-Length: ");
	if (contentLengthPos == std::string::npos)
      contentLengthPos = buffer.find("content-length: ");
	if (contentLengthPos != std::string::npos && contentLengthPos < headerEnd)
	{
		size_t valueStart = contentLengthPos + 16;
		size_t valueEnd = buffer.find("\r\n", valueStart);
		std::string lengthStr = buffer.substr(valueStart, valueEnd - valueStart);
		size_t expectedBodySize = (size_t)std::strtol(lengthStr.c_str(), NULL, 10);
		size_t actualBodyReceived = buffer.length() - (headerEnd + 4);

		return (actualBodyReceived >= expectedBodySize);
	}
	return true;
}

void ServerManager::_acceptNewConnection(int serverFd)
{
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int newFd = accept(serverFd, (sockaddr *)&clientAddr, &clientLen);
	if (newFd < 0){
		LOG_ERROR("Failed to accept new connection");
		return;
	}

	if (fcntl(newFd, F_SETFL, O_NONBLOCK) < 0){
		LOG_ERROR("Failed to set non-blocking on client fd");
		close(newFd);
		return;
	}
	_clients[newFd] = Client(newFd, serverFd);
	struct pollfd pfd;
	pfd.fd = newFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);
	LOG_INFO("New connection accepted on FD: " + Utils::intToString(newFd));
}

void ServerManager::_readFromClient(int clientFd)
{
	Client &client = _clients[clientFd];

	if (client.getState() != READING_REQUEST)
		return;
	char buffer[8192];
	int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead <= 0)
	{
		LOG_INFO("Client on FD " + Utils::intToString(clientFd) + " cleanly disconnected.");
		_closeConnection(clientFd);
		return;
	}
	client.appendToRequestBuffer(buffer, bytesRead);
	if (!_isRequestComplete(client.getRequestBuffer()))
		return;

	LOG_DEBUG("Full request buffered on FD: " + Utils::intToString(clientFd));
	client.setState(PROCESSING);

	HttpRequest request;
	request.parse(client.getRequestBuffer());

	RequestHandler handler;
	size_t cfg_idx = _findConfig(client.getServerFd(), request);
	const ServerConfig &config = _configs[cfg_idx];

	CgiContext cgi_ctx;
	cgi_ctx.client_fd  = clientFd;
	cgi_ctx.config_idx = cfg_idx;

	HttpResponse response = handler.handleRequest(request, config, &cgi_ctx);

	if (cgi_ctx.isValid())
	{
		client.setState(CGI_READING);
		_cgiContexts[cgi_ctx.pipe_out] = cgi_ctx;

		struct pollfd pfd;
		pfd.fd      = cgi_ctx.pipe_out;
		pfd.events  = POLLIN;
		pfd.revents = 0;
		_pollfds.push_back(pfd);

		LOG_DEBUG("CGI launched for FD " + Utils::intToString(clientFd)
			+ " pipe_out=" + Utils::intToString(cgi_ctx.pipe_out));
		return;
	}

	response.addHeader("Connection", "close");
	client.appendToResponseBuffer(response.toString());
	client.setState(WRITING_RESPONSE);

	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == clientFd)
		{
			_pollfds[i].events = POLLOUT;
			break;
		}
	}
}


void ServerManager::_readCgiOutput(int pipeFd)
{
	CgiContext &ctx = _cgiContexts[pipeFd];
	char buffer[4096];
	ssize_t bytes = read(pipeFd, buffer, sizeof(buffer));
	if (bytes > 0)
	{
		ctx.output.append(buffer, bytes);
		return; 
	}
	_finalizeCgi(pipeFd);
}


void ServerManager::_finalizeCgi(int pipeFd)
{
	CgiContext ctx = _cgiContexts[pipeFd];

	close(pipeFd);
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin();
		it != _pollfds.end(); ++it)
	{
		if (it->fd == pipeFd) { _pollfds.erase(it); break; }
	}
	_cgiContexts.erase(pipeFd);

	int wstatus;
	if (waitpid(ctx.pid, &wstatus, WNOHANG) == 0)
	{
		kill(ctx.pid, SIGKILL);
		waitpid(ctx.pid, NULL, 0);
	}

	if (_clients.count(ctx.client_fd) == 0)
		return;

	HttpResponse response = RequestHandler::parseCgiOutput(ctx.output);
	response.addHeader("Connection", "close");

	Client &client = _clients[ctx.client_fd];
	client.appendToResponseBuffer(response.toString());
	client.setState(WRITING_RESPONSE);

	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == ctx.client_fd)
		{
			_pollfds[i].events = POLLOUT;
			break;
		}
	}
	LOG_DEBUG("CGI finalized for client FD " + Utils::intToString(ctx.client_fd));
}


void ServerManager::_checkCgiTimeouts()
{
	time_t now = time(NULL);
	for (std::map<int, CgiContext>::iterator it = _cgiContexts.begin();
		it != _cgiContexts.end(); )
	{
		if (now - it->second.start_time <= CGI_TIMEOUT_SEC)
		{
			++it;
			continue;
		}
		int pipeFd   = it->first;
		CgiContext ctx = it->second;
		LOG_WARNING("CGI timeout on pipe FD " + Utils::intToString(pipeFd));

		kill(ctx.pid, SIGKILL);
		waitpid(ctx.pid, NULL, 0);
		close(pipeFd);

		for (std::vector<struct pollfd>::iterator pfd = _pollfds.begin();
			pfd != _pollfds.end(); ++pfd)
		{
			if (pfd->fd == pipeFd) { _pollfds.erase(pfd); break; }
		}
		_cgiContexts.erase(it++);

		if (_clients.count(ctx.client_fd) == 0)
			continue;

		RequestHandler handler;
		const ServerConfig &config = _configs[ctx.config_idx];
		HttpResponse response = handler.buildErrorResponse(504, config);
		response.addHeader("Connection", "close");

		Client &client = _clients[ctx.client_fd];
		client.appendToResponseBuffer(response.toString());
		client.setState(WRITING_RESPONSE);

		for (size_t i = 0; i < _pollfds.size(); ++i)
		{
			if (_pollfds[i].fd == ctx.client_fd)
			{
				_pollfds[i].events = POLLOUT;
				break;
			}
		}
	}
}

void ServerManager::_writeToClient(int clientFd)
{
	Client &client = _clients[clientFd];

	if (client.getState() != WRITING_RESPONSE)
		return;
	std::string &response = client.getResponseBuffer();
	int byteSent = send(clientFd, response.c_str(), response.size(), 0);
	if (byteSent < 0)
	{
		LOG_ERROR("Failed to send data to FD: " + Utils::intToString(clientFd));
		_closeConnection(clientFd);
		return;
	}

	client.eraseFromResponseBuffer(byteSent);
	if (client.getResponseBuffer().empty())
	{
		client.setState(DONE);
		LOG_INFO("HTTP/1.0 Transaction complete, closing FD: " + Utils::intToString(clientFd));
		_closeConnection(clientFd);
	}
}

void ServerManager::_closeConnection(int clientFd)
{
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); it++)
	{
		if (it->fd == clientFd)
		{
			_pollfds.erase(it);
			break;
		}
	}
	_clients.erase(clientFd);
	close(clientFd);
}

