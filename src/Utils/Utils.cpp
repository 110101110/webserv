#include "utils/Utils.hpp"
#include <cctype>

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
}
