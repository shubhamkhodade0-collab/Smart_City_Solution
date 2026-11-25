#include "Searching.h"
#include <cctype>

namespace scms {

int compareIgnoreCase(const std::string& a, const std::string& b) {
    std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        int x = std::tolower(static_cast<unsigned char>(a[i]));
        int y = std::tolower(static_cast<unsigned char>(b[i]));
        if (x != y) return x < y ? -1 : 1;
    }
    if (a.size() == b.size()) return 0;
    return a.size() < b.size() ? -1 : 1;
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
            return false;
    return true;
}

} // namespace scms
