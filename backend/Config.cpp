#include "Config.h"
#include <cstdlib>
#include <fstream>

namespace scms {

Config& Config::instance() {
    static Config c;
    return c;
}

static std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    path_ = path;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        values_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return true;
}

std::string Config::get(const std::string& key, const std::string& fallback) const {
    // environment variables override the file, e.g. SCMS_DB_PORT=5432
    if (const char* env = std::getenv(("SCMS_" + key).c_str())) return env;
    auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

int Config::getInt(const std::string& key, int fallback) const {
    try {
        return std::stoi(get(key, std::to_string(fallback)));
    } catch (...) {
        return fallback;
    }
}

} // namespace scms
