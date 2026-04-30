#include "utils/Logger.hpp"
#include <iostream>
#include <ctime>
#include <sstream>

namespace Logger
{

	const std::string RESET = "\033[0m";
	const std::string RED = "\033[31m";
	const std::string GREEN = "\033[32m";
	const std::string YELLOW = "\033[33m";
	const std::string BLUE = "\033[34m";
	const std::string CYAN = "\033[36m";

	std::string _getTimestamp()
	{
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[80];
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", timeinfo);
		return std::string(buffer);
	}

	void log(LogLevel level, const std::string &message)
	{
		std::string color;
		std::string levelStr;
		std::ostream *out = &std::cout;

		switch (level)
		{
		case DEBUG:
			color = CYAN;
			levelStr = "DEBUG";
			break;
		case INFO:
			color = GREEN;
			levelStr = "INFO";
			break;
		case WARNING:
			color = YELLOW;
			levelStr = "WARNING";
			break;
		case ERROR:
			color = RED;
			levelStr = "ERROR";
			out = &std::cerr;
			break;
		}

		// Format: [Timestamp] [LEVEL] [File.cpp:Line] Message
		*out << color << _getTimestamp() << " [" << levelStr << "] "
			 << RESET << message << std::endl;
	}
}
