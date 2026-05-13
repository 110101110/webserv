#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <climits>
#include <algorithm>

ConfigParser::ConfigParser() {
	_serverHandlers["listen"] = &ConfigParser::parseListen;
	_serverHandlers["host"] = &ConfigParser::parseHost;
	_serverHandlers["server_name"] = &ConfigParser::parseServerName;
	_serverHandlers["client_max_body_size"] = &ConfigParser::parseClientMaxBodySize;
	_serverHandlers["error_page"] = &ConfigParser::parseErrorPage;
	_serverHandlers["root"] = &ConfigParser::parseServerRoot;
	_serverHandlers["index"] = &ConfigParser::parseServerIndex;

	_locationHandlers["root"] = &ConfigParser::parseRoot;
	_locationHandlers["index"] = &ConfigParser::parseIndex;
	_locationHandlers["autoindex"] = &ConfigParser::parseAutoindex;
	_locationHandlers["return"] = &ConfigParser::parseReturnUrl;
	_locationHandlers["upload_store"] = &ConfigParser::parseUploadStore;
	_locationHandlers["cgi_ext"] = &ConfigParser::parseCgiExt;
	_locationHandlers["methods"] = &ConfigParser::parseMethods;

	_locationHandlers["allow_methods"] = &ConfigParser::parseMethods;
	_locationHandlers["cgi_path"] = &ConfigParser::parseCgiPath;
}

ConfigParser::ConfigParser(const ConfigParser &other) {
	*this = other;
}

ConfigParser &ConfigParser::operator=(const ConfigParser &other) {
	if (this != &other) {
		this->_configs = other._configs;
		this->_serverHandlers = other._serverHandlers;
		this->_locationHandlers = other._locationHandlers;
	}
	return *this;
}

ConfigParser::~ConfigParser() {}

void ConfigParser::parse(const std::string& filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Cannot open file: " + filename);

	std::string line, full_content;
	while (std::getline(file, line)) {
		size_t comment_pos = line.find('#');
		if (comment_pos != std::string::npos)
			line = line.substr(0, comment_pos);
		full_content += line + " ";
	}

	std::stringstream ss(full_content);
	std::string word;

	while (ss >> word) {
		if (word == "server") {
			if (!(ss >> word) || word != "{")
				throw std::runtime_error("Expected '{' after 'server'");

			ServerConfig new_server;
			new_server.port = -1;
			bool server_closed = false;

			while (ss >> word) {
				if (word == "}") {
					server_closed = true;
					break;
				}

				if (word == "location") {
					parseLocationBlock(ss, new_server);
				} else {
					std::map<std::string, ServerDirectiveHandler>::iterator it = _serverHandlers.find(word);
					if (it != _serverHandlers.end()) {
						(this->*(it->second))(ss, new_server);
					} else {
						throw std::runtime_error("Unknown server directive: " + word);
					}
				}
			}
			if (!server_closed) throw std::runtime_error("Unclosed server block");
			if (new_server.port == -1) throw std::runtime_error("Each server must have a port (listen)");
			if (new_server.locations.empty()) throw std::runtime_error("Each server must have at least one location");

			// inheritance check
			for (size_t i = 0; i < new_server.locations.size(); ++i){
				if (new_server.locations[i].root.empty()){
					new_server.locations[i].root = new_server.root;
				}
				if (new_server.locations[i].index.empty() && !new_server.index.empty()){
					new_server.locations[i].index = new_server.index;
				}
			}
			_configs.push_back(new_server);
		}
	}
	// fixing port conflict
	for (size_t i = 0; i < _configs.size(); ++i)
	{
		for (size_t j = i + 1; j < _configs.size(); ++j){
			if (_configs[i].port == _configs[j].port &&
				_configs[i].host == _configs[j].host &&
				_configs[i].server_name == _configs[j].server_name){
					throw std::runtime_error("Duplicate virtual server definition for host: " + _configs[i].host + ":" + Utils::intToString(_configs[i].port) + " server_name: " + _configs[i].server_name);
			}
		}
	}
	if (_configs.empty())
		throw std::runtime_error("Configuration file is empty or invalid");
}

std::vector<ServerConfig> ConfigParser::getConfigs() const {
	return _configs;
}

void ConfigParser::parseListen(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos)
		throw std::runtime_error("Invalid listen syntax (missing ';')");
	std::string val = Utils::cleanToken(word);
	if (!Utils::isNumber(val) || val.length() > 5)
		throw std::runtime_error("Invalid port (must be 1-65535)");
	int p = std::atoi(val.c_str());
	if (p <= 0 || p > 65535)
		throw std::runtime_error("Port out of range: " + val);
	server.port = p;
}

void ConfigParser::parseHost(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos)
		throw std::runtime_error("Invalid host syntax (missing ';')");

	std::string host_val = Utils::cleanToken(word);

	if (host_val == "localhost") {
		host_val = "127.0.0.1";
	}

	if (!Utils::isValidIP(host_val)) {
		throw std::runtime_error("Invalid IP address for host: " + host_val);
	}

	server.host = host_val;
}

void ConfigParser::parseServerName(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos)
		throw std::runtime_error("Invalid server_name syntax (missing ';')");
	server.server_name = Utils::cleanToken(word);
}

void ConfigParser::parseClientMaxBodySize(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Invalid client_max_body_size syntax (missing ';')");

	std::string val = Utils::cleanToken(word);
	size_t multiplier = 1;

	if (!val.empty()) {
		char last = val[val.length() - 1];
		if (last == 'K' || last == 'k') {
			multiplier = 1024;
			val.erase(val.length() - 1);
		} else if (last == 'M' || last == 'm') {
			multiplier = 1024 * 1024;
			val.erase(val.length() - 1);
		} else if (last == 'G' || last == 'g') {
			multiplier = 1024 * 1024 * 1024;
			val.erase(val.length() - 1);
		}
	}

	if (!Utils::isNumber(val) || val.length() > 10)
		throw std::runtime_error("Invalid or too large client_max_body_size");

	long val_long = std::atol(val.c_str());
	if (val_long < 0) throw std::runtime_error("Negative client_max_body_size is not allowed");

	size_t max_limit = static_cast<size_t>(-1);
	if (multiplier > 0 && static_cast<size_t>(val_long) > (max_limit / multiplier)) {
		throw std::runtime_error("client_max_body_size overflow");
	}
	server.client_max_body_size = static_cast<size_t>(val_long) * multiplier;
}

void ConfigParser::parseServerRoot(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for server root");
	server.root = Utils::cleanToken(word);
}

void ConfigParser::parseServerIndex(std::stringstream& ss, ServerConfig& server) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for server index");
	server.index = Utils::cleanToken(word);
}

void ConfigParser::parseErrorPage(std::stringstream& ss, ServerConfig& server) {
	std::vector<int> codes;
	std::string word;

	while (ss >> word) {
		if (word.find(';') != std::string::npos) {
			std::string path = Utils::cleanToken(word);
			if (codes.empty()) throw std::runtime_error("Invalid error_page: no codes provided");
			if (Utils::isNumber(path)) throw std::runtime_error("Invalid error_page: path cannot be a number");
			for (size_t i = 0; i < codes.size(); ++i) {
				server.error_pages[codes[i]] = path;
			}
			return;
		} else {
			if (!Utils::isNumber(word)) throw std::runtime_error("Invalid error_page: codes must be numbers");
			codes.push_back(std::atoi(word.c_str()));
		}
	}
	throw std::runtime_error("Invalid error_page directive: missing ';'");
}

void ConfigParser::parseLocationBlock(std::stringstream& ss, ServerConfig& server) {
	Location loc;
	std::string word;

	if (!(ss >> word)) throw std::runtime_error("Missing location path");
	loc.path = word;

	if (loc.path == "~") {
		if (!(ss >> word)) throw std::runtime_error("Missing regex path after ~");
		loc.path += " " + word;
	}

	if (!(ss >> word) || word != "{")
		throw std::runtime_error("Expected '{' for location " + loc.path);

	bool loc_closed = false;
	while (ss >> word) {
		if (word == "}") {
			loc_closed = true;
			break;
		}

		std::map<std::string, LocationDirectiveHandler>::iterator it = _locationHandlers.find(word);
		if (it != _locationHandlers.end()) {
			(this->*(it->second))(ss, loc);
		} else {
			throw std::runtime_error("Unknown location directive: " + word);
		}
	}
	if (!loc_closed) throw std::runtime_error("Unclosed location block");

	server.locations.push_back(loc);
}

void ConfigParser::parseRoot(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for root");
	loc.root = Utils::cleanToken(word);
}

void ConfigParser::parseIndex(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for index");
	loc.index = Utils::cleanToken(word);
}

void ConfigParser::parseAutoindex(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for autoindex");
	loc.autoindex = (Utils::cleanToken(word) == "on");
}

void ConfigParser::parseReturnUrl(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word)) throw std::runtime_error("Missing value for return");

	if (word.find(';') != std::string::npos) {
		loc.return_url = Utils::cleanToken(word);
		return;
	}

	std::string url;
	if (!(ss >> url) || url.find(';') == std::string::npos)
		throw std::runtime_error("Missing ';' for return directive");

	if (!Utils::isNumber(word))
		throw std::runtime_error("First argument of return must be an HTTP status code");

	loc.return_url = word + " " + Utils::cleanToken(url);
}

void ConfigParser::parseUploadStore(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for upload_store");
	loc.upload_store = Utils::cleanToken(word);
}

void ConfigParser::parseCgiExt(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for cgi_ext");
	loc.cgi_ext = Utils::cleanToken(word);
	LOG_DEBUG(loc.cgi_ext);
}

void ConfigParser::parseCgiPath(std::stringstream& ss, Location& loc) {
	std::string word;
	if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Missing ';' for cgi_path");
	loc.cgi_path = Utils::cleanToken(word);
	LOG_DEBUG(loc.cgi_path);
}

void ConfigParser::parseMethods(std::stringstream& ss, Location& loc) {
	std::string word;
	loc.methods.clear();
	bool has_method = false;

	while (ss >> word && word.find(';') == std::string::npos && word != "}") {
		if (word == "GET" || word == "POST" || word == "DELETE") {
			if (std::find(loc.methods.begin(), loc.methods.end(), word) == loc.methods.end()) {
				loc.methods.push_back(word);
			}
			has_method = true;
		} else {
			throw std::runtime_error("Invalid HTTP method: " + word);
		}
	}
	if (word == "}") throw std::runtime_error("Unexpected closing brace in methods");

	std::string last = Utils::cleanToken(word);
	if (last == "GET" || last == "POST" || last == "DELETE") {
		if (std::find(loc.methods.begin(), loc.methods.end(), last) == loc.methods.end()) {
			loc.methods.push_back(last);
		}
		has_method = true;
	} else if (!last.empty()) {
		throw std::runtime_error("Invalid HTTP method: " + last);
	}

	if (!has_method) {
		throw std::runtime_error("No methods specified in allow_methods");
	}
}
