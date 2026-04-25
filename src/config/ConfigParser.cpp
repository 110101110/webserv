#include "config/ConfigParser.hpp"
#include "utils/Utils.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

ConfigParser::ConfigParser() {}

ConfigParser::ConfigParser(const ConfigParser &other) { *this = other; }

ConfigParser &ConfigParser::operator=(const ConfigParser &other)
{
	if (this != &other)
		this->_configs = other._configs;
	return *this;
}

ConfigParser::~ConfigParser() {}

void ConfigParser::parse(const std::string& filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Impossible d'ouvrir le fichier : " + filename);

	std::string line, full_content;
	while (std::getline(file, line))
	{
		size_t comment_pos = line.find('#');
		if (comment_pos != std::string::npos)
			line = line.substr(0, comment_pos);
		full_content += line + " ";
	}

	std::stringstream ss(full_content);
	std::string word;

	while (ss >> word)
	{
		if (word == "server")
		{
			if (!(ss >> word) || word != "{")
				throw std::runtime_error("'{' attendu après 'server'");
			ServerConfig new_server;
			bool server_closed = false;
			bool has_listen = false;

			while (ss >> word)
			{
				if (word == "}")
				{
					server_closed = true;
					break;
				}

				if (word == "listen")
				{
					if (!(ss >> word) || word.find(';') == std::string::npos)
						throw std::runtime_error("Syntaxe listen invalide (manque ';')");
					std::string val = Utils::cleanToken(word);
					if (!Utils::isNumber(val) || val.length() > 5)
						throw std::runtime_error("Port invalide (doit être 1-65535)");
					int p = std::atoi(val.c_str());
					if (p <= 0 || p > 65535)
						throw std::runtime_error("Port hors limite : " + val);
					new_server.port = p;
					has_listen = true;
				}
				else if (word == "host")
				{
					if (!(ss >> word))
						throw std::runtime_error("Host manquant");
					new_server.host = Utils::cleanToken(word);
				}
				else if (word == "server_name")
				{
					if (!(ss >> word))
						throw std::runtime_error("server_name manquant");
					new_server.server_name = Utils::cleanToken(word);
				}
				else if (word == "client_max_body_size")
				{
					if (!(ss >> word))
						throw std::runtime_error("body_size manquant");
					std::string val = Utils::cleanToken(word);
					if (!Utils::isNumber(val) || val.length() > 10)
						throw std::runtime_error("body_size invalide ou trop grand");
					long val_long = std::atol(val.c_str());
					if (val_long < 0)
						throw std::runtime_error("client_max_body_size négatif interdit");
					new_server.client_max_body_size = static_cast<size_t>(val_long);
				}
				else if (word == "error_page")
				{
					std::string code, path;
					if (!(ss >> code >> path))
						throw std::runtime_error("error_page invalide");
					new_server.error_pages[std::atoi(code.c_str())] = Utils::cleanToken(path);
				}
				else if (word == "location")
				{
					Location loc;
					if (!(ss >> word))
						throw std::runtime_error("Path location manquant");
					loc.path = word;
					if (!(ss >> word) || word != "{")
						throw std::runtime_error("'{' attendu pour location " + loc.path);
					bool loc_closed = false;
					while (ss >> word)
					{
						if (word == "}")
						{
							loc_closed = true;
							break;
						}
						if (word == "root")
						{
							ss >> word;
							loc.root = Utils::cleanToken(word);
						}
						else if (word == "index")
						{
							ss >> word;
							loc.index = Utils::cleanToken(word);
						}
						else if (word == "autoindex")
						{
							ss >> word;
							loc.autoindex = (Utils::cleanToken(word) == "on");
						}
						else if (word == "return")
						{
							ss >> word;
							loc.return_url = Utils::cleanToken(word);
						}
						else if (word == "upload_store")
						{
							ss >> word;
							loc.upload_store = Utils::cleanToken(word);
						}
						else if (word == "cgi_ext")
						{
							ss >> word;
							loc.cgi_ext = Utils::cleanToken(word);
						}
						else if (word == "cgi_path")
						{
							ss >> word;
							loc.cgi_path = Utils::cleanToken(word);
						}
						else if (word == "methods")
						{
							while (ss >> word && word.find(';') == std::string::npos && word != "}")
							{
								if (word == "GET" || word == "POST" || word == "DELETE")
									loc.methods.push_back(word);
								else
									throw std::runtime_error("Méthode HTTP invalide : " + word);
							}
							if (word == "}")
								throw std::runtime_error("Accolade fermante inattendue dans methods");
							std::string last = Utils::cleanToken(word);
							if (last == "GET" || last == "POST" || last == "DELETE")
								loc.methods.push_back(last);
							else if (last != "")
								throw std::runtime_error("Méthode HTTP invalide : " + last);
						}
						else
							throw std::runtime_error("Directive location inconnue : " + word);
					}
					if (!loc_closed)
						throw std::runtime_error("Bloc location non fermé");
					new_server.locations.push_back(loc);
				}
				else
					throw std::runtime_error("Directive server inconnue : " + word);
			}
			if (!server_closed)
				throw std::runtime_error("Bloc server non fermé");
			if (!has_listen)
				throw std::runtime_error("Chaque serveur doit avoir un port (listen)");
			if (new_server.locations.empty())
				throw std::runtime_error("Chaque serveur doit avoir au moins une location");
			_configs.push_back(new_server);
		}
	}
	if (_configs.empty())
		throw std::runtime_error("Le fichier de configuration est vide ou invalide");
}

std::vector<ServerConfig> ConfigParser::getConfigs() const{
	return _configs;
}
