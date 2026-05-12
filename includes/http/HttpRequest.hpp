#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <iostream>
# include <map>
# include <stdlib.h>
# include <string>
# include <algorithm>



class HttpRequest
{
  public:
    static const int INCOMPLETE = -1;
    static const int OK = 0;
    HttpRequest();
    HttpRequest(const HttpRequest& other);
    HttpRequest& operator=(const HttpRequest& other);

    ~HttpRequest();

	int parse(std::string request);

    std::string getMethod() const;
    std::string getPath() const;
    std::string getQueryString() const;
    std::string getHttpVersion() const;
    const std::map<std::string, std::string>& getHeader() const;
    std::string getBody() const;
    bool isComplete() const;

    int getErrorCode() const;

  private:
	std::string _method;
	std::string _path;
	std::string _query_string;
	std::string _http_version;
	std::map<std::string, std::string> _header;
	std::string _body;
	bool _is_complete;
	int _error_code;
};

#endif