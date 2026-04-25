#include "utils/Logger.hpp"
#include <iostream>
#include <ctime>
#include <sstream>

namespace Logger{

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
		std::string levelStr;
		std::ostream *out = &std::cout;

		switch (level){
		case DEBUG: levelStr = "DEBUG"; break;
		case INFO: levelStr = "INFO"; break;
		case WARNING: levelStr = "WARNING"; break;
		case ERROR: levelStr = "ERROR"; out = &std::cerr; break;
		}
		// Format: [Timestamp] [LEVEL] Message
		*out << _getTimestamp() << " [" << levelStr << "] " << message << std::endl;
	}
}
