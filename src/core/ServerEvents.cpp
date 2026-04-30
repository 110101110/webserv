#include "core/ServerManager.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"
#include "core/Client.hpp"

// #include "http/HttpRequest.h"

#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

in_addr_t ServerManager::_convertIP(const std::string &ip)
{
	in_addr_t result = 0;
	std::stringstream ss(ip);
	std::string segment;
	int count = 0;

	while (std::getline(ss, segment, '.'))
	{
		if (segment.empty() || segment.length() > 3 || count >= 4 || !Utils::isNumber(segment))
			throw std::runtime_error("Invalid IP format: " + ip);
		int val = std::atoi(segment.c_str());
		if (val < 0 || val > 255)
			throw std::runtime_error("IP value out of range (0-255)");
		result = (result << 8) + val;
		count++;
	}
	if (count != 4)
		throw std::runtime_error("Incomplete IP (4 segments expected)");
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
	if (bytesRead < 0)
	{
		_closeConnection(clientFd);
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_INFO("Client on FD " + Utils::intToString(clientFd) + " cleanly disconnected.");
		_closeConnection(clientFd);
		return;
	}
	client.appendToRequestBuffer(buffer, bytesRead);
	// when we are sure that request is sent, we parse the request with httpRequest:parse()
	if (_isRequestComplete(client.getRequestBuffer()))
	{
		LOG_INFO("Full request buffered on FD: " + Utils::intToString(clientFd));
		client.setState(PROCESSING);
		std::string mockResponse =
			"HTTP/1.0 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 45\r\n"
			"\r\n"
			"<h1>Hello from Teammate 2's Multiplexer!</h1>";
		client.appendToResponseBuffer(mockResponse);
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


//to link httprequest with readfromClient
// if (status == -1){
// 	client.setState(READING_REQUEST);
// 	return;
// }
// else if (status > 0){
// 	LOG_WARNING("HTTP error: " + Utils::intToString(status));
// 	std::string errorResponse = "HTTP/1.1 " + Utils::intToString(status) + " Error\r\nContent-Length: 0\r\n\r\n";
// 	client.appendToResponseBuffer(errorResponse);
// }
// else{
// 	LOG_INFO("Method: " + request.getMethod() + " Path: " + request.getPath());
// 	client.appendToResponseBuffer("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello World!");
// }
