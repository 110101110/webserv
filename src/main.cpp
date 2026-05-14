#include <vector>
#include <exception>
#include <unistd.h>
#include <iostream>
#include <csignal>
#include "core/ServerManager.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"
#include "config/ServerConfig.hpp"

volatile sig_atomic_t g_running = 1;

static void handle_sigint(int) { g_running = 0; }

int main(int argc, char **argv)
{
	if (argc != 2){
		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	signal(SIGINT, handle_sigint);
	try{
		ConfigParser parser;
		parser.parse(argv[1]);
		ServerManager manager(parser.getConfigs());
		manager.setupServers();
		signal(SIGPIPE, SIG_IGN);
		manager.run();
	}
	catch (const std::exception &e){
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

