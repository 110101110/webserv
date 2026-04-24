#include <iostream>
#include <vector>
#include <exception>
#include <unistd.h>
#include <csignal>
#include "config.hpp"
#include "ServerManager.hpp"

bool g_running = true;

void handle_sigint(int sig) {
	(void)sig;
	g_running = false;
}

int main(int ac, char **av) {
	if (ac != 2) {
		std::cerr << "Usage: ./webserv config.conf" << std::endl;
		return (1);
	}

	signal(SIGINT, handle_sigint);

	try {
		ServerManager manager;
		manager.parseConfig(av[1]);
		manager.setupServers();
		std::vector<int> fds = manager.getListenFds();
		std::cout << "Prêt pour le poll(). En attente de connexions..." << std::endl;
		while (g_running) {
			sleep(1);
		}
		std::cout << "\nArrêt propre du serveur demandé..." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Erreur fatale: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
