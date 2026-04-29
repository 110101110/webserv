#include "core/Client.hpp"
#include "utils/Logger.hpp"

Client::Client() : _fd(-1), _state(READING_REQUEST) {}

Client::Client(int fd) : _fd(fd), _state(READING_REQUEST){}

Client::Client(const Client &other){
	*this = other;
}

Client &Client::operator=(const Client &other){
	if (this != &other)
	{
		this->_fd = other._fd;
		this->_state = other._state;
		this->_requestBuffer = other._requestBuffer;
		this->_responseBuffer = other._responseBuffer;
	}
	return *this;
}

Client::~Client() {}

int Client::getFd() const{
	return _fd;
}

ClientState Client::getState() const{
	return _state;
}

void Client::setState(ClientState state){
	_state = state;
}

void Client::appendToRequestBuffer(const char *data, ssize_t size){
	if (size > 0){
		_requestBuffer.append(data, size);
	}
}

std::string& Client::getRequestBuffer(){
	return _requestBuffer;
}

void Client::appendToResponseBuffer(const std::string &data){
	_responseBuffer.append(data);
}

std::string& Client::getResponseBuffer(){
	return _responseBuffer;
}

void Client::eraseFromResponseBuffer(ssize_t bytesSent){
	if (bytesSent > 0 && bytesSent <= (ssize_t)_responseBuffer.length()){
		_responseBuffer.erase(0, bytesSent);
	}
}
