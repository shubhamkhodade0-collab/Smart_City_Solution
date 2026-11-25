// AiUtil.h - tiny string helpers shared by the AI assistant sources (internal, header-only)
#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace scms {
namespace ai {
namespace util {

inline unsigned char uc(char c) { return static_cast<unsigned char>(c); }

inline std::string lowerCopy(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(uc(ch)));
    return s;
}

inline std::string capitalise(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(uc(s[0])));
    return s;
}

inline std::string trimCopy(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(uc(s[a]))) ++a;
    while (b > a && std::isspace(uc(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// shortens at a word boundary so a drafted note always fits the 200-character note box
inline std::string fitNote(const std::string& s, std::size_t maxLen = 200) {
    if (s.size() <= maxLen) return s;
    std::string cut = s.substr(0, maxLen - 3);
    std::size_t sp = cut.find_last_of(' ');
    if (sp != std::string::npos && sp > maxLen / 2) cut.resize(sp);
    return cut + "...";
}

// first sentence of a paragraph (keeps the full stop)
inline std::string firstSentence(const std::string& s) {
    std::size_t dot = s.find(". ");
    return dot == std::string::npos ? s : s.substr(0, dot + 1);
}

} // namespace util
} // namespace ai
} // namespace scms
