#include <vector>
#include <exception>
#include <unistd.h>
#include <iostream>
#include <csignal>
#include "core/ServerManager.hpp"
#include "utils/Logger.hpp"

bool g_running = true;

// void handle_sigint(int sig) {
// 	(void)sig;
// 	g_running = false;
// }

int main()
{
	try
	{
		LOG_INFO("--- STARTING PURE MULTIPLEXER TEST ---");

		// 1. Manually build a configuration
		ServerConfig testConfig;
		testConfig.host = "127.0.0.1";
		testConfig.port = 8080;

		// You can even add a second port to test your _listen_fds array!
		ServerConfig testConfig2;
		testConfig2.host = "127.0.0.1";
		testConfig2.port = 8081;

		std::vector<ServerConfig> configs;
		configs.push_back(testConfig);
		configs.push_back(testConfig2);

		// 2. Initialize your engine
		ServerManager manager(configs);

		// 3. Fire it up
		manager.setupServers();
		manager.run();
	}
	catch (const std::exception &e)
	{
		LOG_ERROR(e.what());
		return 1;
	}

	return 0;
}
