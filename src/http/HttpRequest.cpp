#include "http/HttpRequest.hpp"

HttpRequest::HttpRequest() : _is_complete(false), _error_code(0) {}

HttpRequest::HttpRequest(const HttpRequest& other)
    : _method(other._method), _path(other._path), _query_string(other._query_string),
      _http_version(other._http_version), _header(other._header), _body(other._body),
      _is_complete(other._is_complete), _error_code(other._error_code) {}

HttpRequest& HttpRequest::operator=(const HttpRequest& other)
{
    if (this != &other)
    {
        _method = other._method;
        _path = other._path;
        _query_string = other._query_string;
        _http_version = other._http_version;
        _header = other._header;
        _body = other._body;
        _is_complete = other._is_complete;
        _error_code = other._error_code;
    }
    return *this;
}

HttpRequest::~HttpRequest() {}

static char toLowerChar(unsigned char c) { return std::tolower(c); }

static long toInt(const std::string& str)
{
    if (str.empty())
        return -1;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (!std::isdigit(str[i]))
            return -1;
    }
    char *end;
    long val = std::strtol(str.c_str(), &end, 10);
    if (*end != '\0' || val < 0)
        return -1;
    return val;
}

int HttpRequest::parse(std::string request)
{
    _method       = "";
    _path         = "";
    _query_string = "";
    _http_version = "";
    _header.clear();
    _body         = "";
    _is_complete   = false;
    _error_code   = 0;

    if(request.empty() || request.find("\r\n\r\n") == std::string::npos)
    {
        _error_code = -1; // Incomplete Request
        return _error_code;
    }
    else
    {
        size_t method_end = request.find(' ');
        if (method_end == std::string::npos)
        {
            _error_code = 400; // Bad Request
            return _error_code;
        }
        _method = request.substr(0, method_end);
        std::string valid_methods[] = {"GET", "POST", "DELETE"};
        bool method_valid = false;
        for (size_t i = 0; i < sizeof(valid_methods) / sizeof(valid_methods[0]); ++i)
        {
            if (_method == valid_methods[i])
            {
                method_valid = true;
                break;
            }
        }
        if (!method_valid)
        {
            _error_code = 501; // Not Implemented
            return _error_code;
        }

        size_t path_end = request.find(' ', method_end + 1);
        if (path_end == std::string::npos)
        {
            _error_code = 400; // Bad Request
            return _error_code;
        }
        size_t query_pos = request.find('?', method_end + 1);
        if (query_pos != std::string::npos && query_pos < path_end)
        {
            _path = request.substr(method_end + 1, query_pos - method_end - 1);
            _query_string = request.substr(query_pos + 1, path_end - query_pos - 1);
        }
        else
        _path = request.substr(method_end + 1, path_end - method_end - 1);

        size_t version_end = request.find("\r\n", path_end + 1);
        if (version_end == std::string::npos)
        {
            _error_code = 400; // Bad Request
            return _error_code;
        }
        _http_version = request.substr(path_end + 1, version_end - path_end - 1);
        if (_http_version != "HTTP/1.1" && _http_version != "HTTP/1.0")
        {
            _error_code = 505; // HTTP Version Not Supported
            return _error_code;
        }
        while (version_end != std::string::npos)
        {
            size_t header_end = request.find("\r\n", version_end + 2);
            if (header_end == std::string::npos || header_end == version_end + 2)
            {
                break; // End of headers
            }
            std::string header_line = request.substr(version_end + 2, header_end - version_end - 2);
            size_t delimiter_pos = header_line.find(": ");
            if (delimiter_pos != std::string::npos)
            {
                std::string key = header_line.substr(0, delimiter_pos);
                std::transform(key.begin(), key.end(), key.begin(), toLowerChar);
                std::string value = header_line.substr(delimiter_pos + 2);

                if (_header.count(key))
                {
                    // Host et Content-Length dupliqués = 400 (HTTP Request Smuggling)
                    if (key == "host" || key == "content-length")
                    {
                        _error_code = 400;
                        return _error_code;
                    }
                    // Autres headers : concaténation RFC 7230 
                    _header[key] += ", " + value;
                }
                else
                    _header[key] = value;
            }
            version_end = header_end;
        }
        if (_header.find("host") == _header.end())
        {
            _error_code = 400; // Bad Request
            return _error_code;
        }
        size_t body_pos = request.find("\r\n\r\n");
        if (body_pos != std::string::npos)
        {
            _body = request.substr(body_pos + 4);
        }
        if (_header.find("content-length") != _header.end())
        {
            long content_length = toInt(_header["content-length"]);
            if (content_length == -1)
            {
                _error_code = 400;
                return _error_code;
            }
            if ((long)_body.size() < content_length)
            {
                _error_code = -1;
                return _error_code;
            }
        }

        _is_complete = true;
    }


    return 0; // Placeholder return value
}

std::string HttpRequest::getMethod() const { return _method; }

std::string HttpRequest::getPath() const { return _path; }

std::string HttpRequest::getQueryString() const { return _query_string; }

std::string HttpRequest::getHttpVersion() const { return _http_version; }

const std::map<std::string, std::string>& HttpRequest::getHeader() const { return _header; }

std::string HttpRequest::getBody() const { return _body; }

bool HttpRequest::isComplete() const { return _is_complete; }

int HttpRequest::getErrorCode() const {return _error_code;}


