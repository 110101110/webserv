#ifndef REQUESTHANDLER_HPP
# define REQUESTHANDLER_HPP

# include "HttpRequest.hpp"
# include "HttpResponse.hpp"
# include "CgiContext.hpp"
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
	// cgi_out : si non-NULL et qu'une requête CGI est détectée, lance le CGI
	// de façon non-bloquante et remplit cgi_out (cgi_out->isValid() == true).
	// Dans ce cas, la réponse retournée est ignorée par l'appelant.
	HttpResponse handleRequest(const HttpRequest &request,
		const ServerConfig &config, CgiContext *cgi_out = NULL);

	HttpResponse buildErrorResponse(int code, const ServerConfig &config);

	// Parse la sortie brute d'un processus CGI en HttpResponse.
	static HttpResponse parseCgiOutput(const std::string &raw);

  private:
	bool findLocation(const std::string &path, const ServerConfig &config,
		Location &result);
	HttpResponse handleGet(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse handlePost(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	HttpResponse handleDelete(const HttpRequest &req, const Location &loc, const ServerConfig &config);
	bool launchCgi(const HttpRequest &req, const Location &loc,
		const ServerConfig &config, CgiContext &ctx);
	std::string _getStatusMessage(int code);
};

#endif
