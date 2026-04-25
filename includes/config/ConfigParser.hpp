#pragma once

#include <vector>
#include <string>
#include "ServerConfig.hpp"

class ConfigParser{
	private:
	std::vector<ServerConfig> _configs;

public:
	ConfigParser();
	ConfigParser(const ConfigParser &other);
	ConfigParser &operator=(const ConfigParser &other);
	~ConfigParser();

	void parse(const std::string &filename);
	std::vector<ServerConfig> getConfigs() const;
};
