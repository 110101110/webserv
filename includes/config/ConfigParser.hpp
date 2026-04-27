#pragma once

#include <vector>
#include <string>
#include <map>
#include <sstream>
#include "ServerConfig.hpp"

class ConfigParser {
private:
	std::vector<ServerConfig> _configs;

	//  Définition des pointeurs sur fonctions membres (Dispatch Table)
	typedef void (ConfigParser::*ServerDirectiveHandler)(std::stringstream&, ServerConfig&);
	typedef void (ConfigParser::*LocationDirectiveHandler)(std::stringstream&, Location&);

	//  Les maps de routage
	std::map<std::string, ServerDirectiveHandler> _serverHandlers;
	std::map<std::string, LocationDirectiveHandler> _locationHandlers;

	//  Sous-fonctions de parsing pour "server"
	void parseListen(std::stringstream& ss, ServerConfig& server);
	void parseHost(std::stringstream& ss, ServerConfig& server);
	void parseServerName(std::stringstream& ss, ServerConfig& server);
	void parseClientMaxBodySize(std::stringstream& ss, ServerConfig& server);
	void parseErrorPage(std::stringstream& ss, ServerConfig& server);
	void parseServerRoot(std::stringstream& ss, ServerConfig& server);
	void parseServerIndex(std::stringstream& ss, ServerConfig& server);

	//  Sous-fonctions de parsing pour "location"
	void parseLocationBlock(std::stringstream& ss, ServerConfig& server);
	void parseRoot(std::stringstream& ss, Location& loc);
	void parseIndex(std::stringstream& ss, Location& loc);
	void parseAutoindex(std::stringstream& ss, Location& loc);
	void parseReturnUrl(std::stringstream& ss, Location& loc);
	void parseUploadStore(std::stringstream& ss, Location& loc);
	void parseCgiExt(std::stringstream& ss, Location& loc);
	void parseCgiPath(std::stringstream& ss, Location& loc);
	void parseMethods(std::stringstream& ss, Location& loc);

public:
	ConfigParser();
	ConfigParser(const ConfigParser &other);
	ConfigParser &operator=(const ConfigParser &other);
	~ConfigParser();

	void parse(const std::string &filename);
	std::vector<ServerConfig> getConfigs() const;
};
