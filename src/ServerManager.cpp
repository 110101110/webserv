#include "ServerManager.hpp"
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <utility>
#include <algorithm>
#include <cctype>
#include <cstring>

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _listen_fds.size(); ++i) {
		if (_listen_fds[i] != -1) {
			close(_listen_fds[i]);
			std::cout << "Socket FD " << _listen_fds[i] << " fermé proprement." << std::endl;
		}
	}
}

bool isNumber(const std::string& s) {
	if (s.empty()) return false;
	for (size_t i = 0; i < s.size(); i++) {
		if (!isdigit(s[i])) return false;
	}
	return true;
}

std::string ServerManager::cleanToken(std::string str) {
	size_t pos = str.find(';');
	if (pos != std::string::npos)
		str.erase(pos);
	return str;
}

in_addr_t ServerManager::convertIP(const std::string& ip) {
	in_addr_t result = 0;
	std::stringstream ss(ip);
	std::string segment;
	int count = 0;

	while (std::getline(ss, segment, '.')) {
		if (segment.empty() || segment.length() > 3 || count >= 4 || !isNumber(segment))
			throw std::runtime_error("Format IP invalide : " + ip);
		int val = std::atoi(segment.c_str());
		if (val < 0 || val > 255) throw std::runtime_error("Valeur IP hors limite (0-255)");
		result = (result << 8) + val;
		count++;
	}
	if (count != 4) throw std::runtime_error("IP incomplète (4 segments attendus)");
	return htonl(result);
}

void ServerManager::parseConfig(std::string filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open()) throw std::runtime_error("Impossible d'ouvrir le fichier : " + filename);

	std::string line, full_content;
	while (std::getline(file, line)) {
		size_t comment_pos = line.find('#');
		if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
		full_content += line + " ";
	}

	std::stringstream ss(full_content);
	std::string word;

	while (ss >> word) {
		if (word == "server") {
			if (!(ss >> word) || word != "{") throw std::runtime_error("'{' attendu après 'server'");
			ServerConfig new_server;
			bool server_closed = false;
			bool has_listen = false;

			while (ss >> word) {
				if (word == "}") { server_closed = true; break; }
				
				if (word == "listen") {
					if (!(ss >> word) || word.find(';') == std::string::npos) throw std::runtime_error("Syntaxe listen invalide (manque ';')");
					std::string val = cleanToken(word);
					if (!isNumber(val) || val.length() > 5) throw std::runtime_error("Port invalide (doit être 1-65535)");
					int p = std::atoi(val.c_str());
					if (p <= 0 || p > 65535) throw std::runtime_error("Port hors limite : " + val);
					new_server.port = p;
					has_listen = true;
				} else if (word == "host") {
					if (!(ss >> word)) throw std::runtime_error("Host manquant");
					new_server.host = cleanToken(word);
				} else if (word == "server_name") {
					if (!(ss >> word)) throw std::runtime_error("server_name manquant");
					new_server.server_name = cleanToken(word);
				} else if (word == "client_max_body_size") {
					if (!(ss >> word)) throw std::runtime_error("body_size manquant");
					std::string val = cleanToken(word);
					if (!isNumber(val) || val.length() > 10) throw std::runtime_error("body_size invalide ou trop grand");
					long val_long = std::atol(val.c_str());
					if (val_long < 0) throw std::runtime_error("client_max_body_size négatif interdit");
					new_server.client_max_body_size = static_cast<size_t>(val_long);
				} else if (word == "error_page") {
					std::string code, path;
					if (!(ss >> code >> path)) throw std::runtime_error("error_page invalide");
					new_server.error_pages[std::atoi(code.c_str())] = cleanToken(path);
				} else if (word == "location") {
					Location loc;
					if (!(ss >> word)) throw std::runtime_error("Path location manquant");
					loc.path = word;
					if (!(ss >> word) || word != "{") throw std::runtime_error("'{' attendu pour location " + loc.path);
					bool loc_closed = false;
					while (ss >> word) {
						if (word == "}") { loc_closed = true; break; }
						if (word == "root") { ss >> word; loc.root = cleanToken(word); }
						else if (word == "index") { ss >> word; loc.index = cleanToken(word); }
						else if (word == "autoindex") { ss >> word; loc.autoindex = (cleanToken(word) == "on"); }
						else if (word == "return") { ss >> word; loc.return_url = cleanToken(word); }
						else if (word == "upload_store") { ss >> word; loc.upload_store = cleanToken(word); }
						else if (word == "cgi_ext") { ss >> word; loc.cgi_ext = cleanToken(word); }
						else if (word == "cgi_path") { ss >> word; loc.cgi_path = cleanToken(word); }
						else if (word == "methods") {
							while (ss >> word && word.find(';') == std::string::npos && word != "}") {
								if (word == "GET" || word == "POST" || word == "DELETE")
									loc.methods.push_back(word);
								else throw std::runtime_error("Méthode HTTP invalide : " + word);
							}
							if (word == "}") throw std::runtime_error("Accolade fermante inattendue dans methods");
							std::string last = cleanToken(word);
							if (last == "GET" || last == "POST" || last == "DELETE")
								loc.methods.push_back(last);
							else if (last != "") throw std::runtime_error("Méthode HTTP invalide : " + last);
						} else throw std::runtime_error("Directive location inconnue : " + word);
					}
					if (!loc_closed) throw std::runtime_error("Bloc location non fermé");
					new_server.locations.push_back(loc);
				} else throw std::runtime_error("Directive server inconnue : " + word);
			}
			if (!server_closed) throw std::runtime_error("Bloc server non fermé");
			if (!has_listen) throw std::runtime_error("Chaque serveur doit avoir un port (listen)");
			if (new_server.locations.empty()) throw std::runtime_error("Chaque serveur doit avoir au moins une location");
			_configs.push_back(new_server);
		}
	}
	if (_configs.empty()) throw std::runtime_error("Le fichier de configuration est vide ou invalide");
}

void ServerManager::setupServers() {
	std::vector< std::pair<std::string, int> > bound_sockets;

	for (size_t i = 0; i < _configs.size(); ++i) {
		bool already_bound = false;
		for (size_t j = 0; j < bound_sockets.size(); ++j) {
			if (bound_sockets[j].first == _configs[i].host && bound_sockets[j].second == _configs[i].port) {
				already_bound = true; break;
			}
		}
		if (already_bound) {
			std::cout << " Virtual Host (IP:Port déjà bind) pour " << _configs[i].host << ":" << _configs[i].port << std::endl;
			continue;
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) throw std::runtime_error("Erreur socket()");

		int opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) throw std::runtime_error("setsockopt failed");
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { close(fd); throw std::runtime_error("fcntl failed"); }

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = convertIP(_configs[i].host);
		addr.sin_port = htons(_configs[i].port);

		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			close(fd);
			std::cerr << "Impossible de bind sur " << _configs[i].host << ":" << _configs[i].port << " (OS). Skip..." << std::endl;
			continue;
		}
		if (listen(fd, 128) < 0) { close(fd); throw std::runtime_error("listen failed"); }

		_listen_fds.push_back(fd);
		bound_sockets.push_back(std::make_pair(_configs[i].host, _configs[i].port));
		std::cout << "📡 Socket " << fd << " à l'écoute sur " << _configs[i].host << ":" << _configs[i].port << std::endl;
	}
	if (_listen_fds.empty())
		throw std::runtime_error("Aucun serveur n'a pu être lancé.");
}

std::vector<int> ServerManager::getListenFds() const { return _listen_fds; }
std::vector<ServerConfig> ServerManager::getConfigs() const { return _configs; }
