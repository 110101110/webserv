#include "utils/Utils.hpp"
#include <cctype>
#include <cstdlib>

namespace Utils{

	bool isNumber(const std::string& s) {
        if (s.empty()) return false;
        for (size_t i = 0; i < s.size(); i++) {
            if (!isdigit(s[i])) return false;
        }
        return true;
    }

    std::string cleanToken(std::string str) {
        size_t pos = str.find(';');
        if (pos != std::string::npos)
            str.erase(pos);
        return str;
    }

	bool isValidIP(const std::string &ip) {
		if (ip.empty() || ip[ip.length() - 1] == '.')
			return false;

		std::stringstream ss(ip);
		std::string segment;
		int count = 0;

		while (std::getline(ss, segment, '.')) {
			if (segment.empty() || segment.length() > 3 || !isNumber(segment))
				return false;

			int val = std::atoi(segment.c_str());
			if (val < 0 || val > 255)
				return false;

			count++;
		}
		return count == 4;
	}
}
