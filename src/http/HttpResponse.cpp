#include "http/HttpResponse.hpp"
#include "utils/Utils.hpp"

HttpResponse::HttpResponse() : _status_code(200) {}

void HttpResponse::setStatus(int code, const std::string &message)
{
    _status_code = code;
    _status_message = message;
}

void HttpResponse::addHeader(const std::string &key, const std::string &value)
{
    _header[key] = value;
}

void HttpResponse::setBody(const std::string &body)
{
    _body = body;
}

std::string HttpResponse::toString() const
{
    std::string response = "HTTP/1.1 " + Utils::intToString(_status_code) + " " + _status_message + "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = _header.begin(); it != _header.end(); ++it)
    {
        response += it->first + ": " + it->second + "\r\n";
    }
    response += "\r\n" + _body;
    return response;
}

