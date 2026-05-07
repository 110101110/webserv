#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include "HttpRequest.hpp"
# include "HttpResponse.hpp"
# include "../config/ServerConfig.hpp"
# include <algorithm>
# include <fstream>
# include <iostream>
# include <sys/stat.h>
# include <dirent.h>
# include <sstream>
# include <string>
# include <stdio.h>

class RequestHandler
{
  public:
	HttpResponse handleRequest(const HttpRequest &request,
		const ServerConfig &config);

  private:
	bool findLocation(const std::string &path, const ServerConfig &config,
		Location &result);
	HttpResponse handleGet(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse handlePost(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse handleDelete(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse handleCgi(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse buildErrorResponse(int code, const ServerConfig &config);
	std::string  _getStatusMessage(int code);
};

#endif
