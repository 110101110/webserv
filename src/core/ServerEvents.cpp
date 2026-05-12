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
	if (contentLengthPos != std::string::npos && contentLengthPos < headerEnd)
	{
		size_t valueStart = contentLengthPos + 16;
		size_t valueEnd = buffer.find("\r\n", valueStart);
		std::string lengthStr = buffer.substr(valueStart, valueEnd - valueStart);
		size_t expectedBodySize = (size_t)std::strtol(lengthStr.c_str(), NULL, 10);
		size_t actualBodyReceived = buffer.length() - (headerEnd + 4);

		return (actualBodyReceived >= expectedBodySize);
	}
	size_t chunkedPos = buffer.find("Transfer-Encoding: chunked");
	if (chunkedPos != std::string::npos && chunkedPos < headerEnd)
	{
		if (buffer.find("0\r\n\r\n") != std::string::npos)
			return true;
		return false;
	}
	return true;
}

void ServerManager::_acceptNewConnection(int serverFd)
{
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int newFd = accept(serverFd, (sockaddr *)&clientAddr, &clientLen);
	if (newFd < 0)
		LOG_ERROR("Failed to accept new connection");
	// set socket non blocking
	fcntl(newFd, F_SETFL, O_NONBLOCK);
	_clients[newFd] = Client(newFd);
	struct pollfd pfd;
	pfd.fd = newFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);
	LOG_INFO("New connection accpeted on FD: " + Utils::intToString(newFd));
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
	const ServerConfig &config = _configs[0];

	CgiContext cgi_ctx;
	cgi_ctx.client_fd  = clientFd;
	cgi_ctx.config_idx = 0;

	HttpResponse response = handler.handleRequest(request, config, &cgi_ctx);

	if (cgi_ctx.isValid())
	{
		// CGI lancé : on surveille pipe_out via poll
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
	client.setState(WRITING_REPONSE);

	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == clientFd)
		{
			_pollfds[i].events = POLLOUT;
			break;
		}
	}
}

// Lit la sortie du processus CGI depuis pipe_fd.
// Si EOF (bytes == 0) ou erreur → finalise la réponse.
void ServerManager::_readCgiOutput(int pipeFd)
{
	CgiContext &ctx = _cgiContexts[pipeFd];
	char buffer[4096];
	ssize_t bytes = read(pipeFd, buffer, sizeof(buffer));
	if (bytes > 0)
	{
		ctx.output.append(buffer, bytes);
		return; // attendre le prochain POLLIN
	}
	// bytes == 0 : EOF (processus terminé) ou bytes < 0 : erreur
	_finalizeCgi(pipeFd);
}

// Construit la réponse HTTP depuis la sortie CGI accumulée,
// l'envoie au client, nettoie le contexte CGI.
void ServerManager::_finalizeCgi(int pipeFd)
{
	CgiContext ctx = _cgiContexts[pipeFd];

	// Ferme le pipe et le retire de poll
	close(pipeFd);
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin();
		it != _pollfds.end(); ++it)
	{
		if (it->fd == pipeFd) { _pollfds.erase(it); break; }
	}
	_cgiContexts.erase(pipeFd);

	// Récupère le processus enfant (sans bloquer)
	int wstatus;
	if (waitpid(ctx.pid, &wstatus, WNOHANG) == 0)
	{
		kill(ctx.pid, SIGKILL);
		waitpid(ctx.pid, NULL, 0);
	}

	// Le client a peut-être été déconnecté entre-temps
	if (_clients.count(ctx.client_fd) == 0)
		return;

	const ServerConfig &config = _configs[ctx.config_idx];
	HttpResponse response = RequestHandler::parseCgiOutput(ctx.output);
	response.addHeader("Connection", "close");

	Client &client = _clients[ctx.client_fd];
	client.appendToResponseBuffer(response.toString());
	client.setState(WRITING_REPONSE);

	for (size_t i = 0; i < _pollfds.size(); ++i)
	{
		if (_pollfds[i].fd == ctx.client_fd)
		{
			_pollfds[i].events = POLLOUT;
			break;
		}
	}
	LOG_DEBUG("CGI finalized for client FD " + Utils::intToString(ctx.client_fd));
	(void)config;
}

// Vérifie si un processus CGI dépasse CGI_TIMEOUT_SEC secondes.
// Si oui : kill + réponse 504.
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
		client.setState(WRITING_REPONSE);

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

	if (client.getState() != WRITING_REPONSE)
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

