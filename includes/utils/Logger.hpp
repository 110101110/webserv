#pragma once
#include <string>
namespace Logger{
	enum LogLevel{
		DEBUG,
		INFO,
		WARNING,
		ERROR,
	};
	void log(LogLevel Level, const std::string& msg);
}

#define LOG_DEBUG(msg) Logger::log(Logger::DEBUG, msg)
#define LOG_INFO(msg) Logger::log(Logger::INFO, msg)
#define LOG_WARNING(msg) Logger::log(Logger::WARNING, msg)
#define LOG_ERROR(msg) Logger::log(Logger::ERROR, msg)
