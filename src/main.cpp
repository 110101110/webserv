#include <vector>
#include <exception>
#include <unistd.h>
#include <iostream>
#include <csignal>
#include "core/ServerManager.hpp"
#include "config/ConfigParser.hpp"

bool g_running = true;

// void handle_sigint(int sig) {
// 	(void)sig;
// 	g_running = false;
// }


int main(int argc, char **argv)
{
	if (argc != 2){
		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	try{
		ConfigParser parser;
		parser.parse(argv[1]);
		ServerManager manager(parser.getConfigs());
		manager.setupServers();
		// manager.run();
	}
	catch (const std::exception &e){
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
