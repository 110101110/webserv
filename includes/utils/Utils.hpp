#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>

namespace Utils{

	bool isNumber(const std::string &s);
	std::string cleanToken(std::string str);
	std::string intToString(int number);
	bool isValidIP(const std::string &ip);
	std::string htmlEscape(const std::string &s);
}
