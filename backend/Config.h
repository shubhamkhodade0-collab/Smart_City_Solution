// Config.h - key=value configuration loader (config/app.conf)
#pragma once
#include <map>
#include <string>

namespace scms {

class Config {
public:
    static Config& instance();                         // single shared configuration
    bool load(const std::string& path);
    std::string get(const std::string& key, const std::string& fallback = "") const;
    int getInt(const std::string& key, int fallback) const;
    const std::string& path() const { return path_; }

private:
    Config() = default;
    std::map<std::string, std::string> values_;
    std::string path_;
};

} // namespace scms
