#pragma once
#include <string>
#include <sys/types.h>

enum ClientState{
	READING_REQUEST,
	PROCESSING,
	WRITING_REPONSE,
	CGI_READING,
	DONE
};

class Client{
	private:
	int _fd;
	int _server_fd; // fd du socket serveur qui a accepté cette connexion
	ClientState _state;
	std::string _requestBuffer;
	std::string _responseBuffer;

	public : Client();
	Client(int fd, int serverFd);
	Client(const Client &other);
	Client &operator=(const Client &other);
	~Client();


	int getFd() const;
	int getServerFd() const;
	ClientState getState() const;
	void setState(ClientState state);

	void appendToRequestBuffer(const char *data, ssize_t size);
	std::string &getRequestBuffer();

	void appendToResponseBuffer(const std::string &data);
	std::string &getResponseBuffer();
	void eraseFromResponseBuffer(ssize_t bytesSent);
};

