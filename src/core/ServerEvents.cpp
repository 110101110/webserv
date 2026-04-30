#include "core/ServerManager.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"
#include "core/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/RequestHandler.hpp"
#include <sys/socket.h>
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
		size_t expectedBodySize = std::atoi(lengthStr.c_str());
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
	if (_isRequestComplete(client.getRequestBuffer()))
	{
		LOG_DEBUG("Full request buffered on FD: " + Utils::intToString(clientFd));
		client.setState(PROCESSING);

		HttpRequest request;
		request.parse(client.getRequestBuffer());
		HttpResponse response;
		RequestHandler handler;
		ServerConfig& config = _configs[0];
		response = handler.handleRequest(request, config);
		//fro http 1.0 where no keep alive
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
		LOG_ERROR("Failed to send datd to FD: " + Utils::intToString(clientFd));
		close(clientFd);
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

