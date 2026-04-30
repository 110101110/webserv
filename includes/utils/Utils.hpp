#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace Utils{

	bool isNumber(const std::string &s);
	std::string cleanToken(std::string str);

	bool isValidIP(const std::string &ip);
}
