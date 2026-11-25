// =====================================================================
// MiniJson.cpp - see MiniJson.h
// =====================================================================
#include "MiniJson.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace scms {
namespace json {

const Value* Value::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key) return &items[i];
    return nullptr;
}

const Value* Value::at(std::size_t index) const {
    if (type != Type::Array || index >= items.size()) return nullptr;
    return &items[index];
}

namespace {

struct Parser {
    const std::string& s;
    std::size_t pos = 0;
    std::string err;
    static constexpr int MAX_DEPTH = 64;

    explicit Parser(const std::string& src) : s(src) {}

    bool fail(const std::string& m) {
        if (err.empty()) err = m + " at position " + std::to_string(pos);
        return false;
    }
    void skipWs() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    }
    bool lit(const char* word) {
        std::size_t n = std::char_traits<char>::length(word);
        if (s.compare(pos, n, word) != 0) return fail("unexpected token");
        pos += n;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool hex4(unsigned& v) {
        if (pos + 4 > s.size()) return fail("truncated \\u escape");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }

    bool str(std::string& out) {
        if (pos >= s.size() || s[pos] != '"') return fail("expected string");
        ++pos;
        out.clear();
        while (pos < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[pos++]);
            if (c == '"') return true;
            if (c < 0x20) return fail("control character in string");
            if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
            if (pos >= s.size()) break;
            char e = s[pos++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {   // high surrogate: needs a following low surrogate
                        unsigned lo;
                        if (pos + 1 < s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                            pos += 2;
                            if (!hex4(lo)) return false;
                            if (lo < 0xDC00 || lo > 0xDFFF) return fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            return fail("lone surrogate");
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("lone surrogate");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool number(Value& v) {
        std::size_t start = pos;
        if (pos < s.size() && s[pos] == '-') ++pos;
        if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) return fail("bad number");
        if (s[pos] == '0') ++pos;
        else while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos < s.size() && s[pos] == '.') {
            ++pos;
            if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) return fail("bad number");
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) return fail("bad number");
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        v.type = Value::Type::Number;
        v.number = std::strtod(s.substr(start, pos - start).c_str(), nullptr);
        return true;
    }

    bool value(Value& v, int depth) {
        if (depth > MAX_DEPTH) return fail("nesting too deep");
        skipWs();
        if (pos >= s.size()) return fail("unexpected end");
        char c = s[pos];
        if (c == '{') {
            ++pos;
            v.type = Value::Type::Object;
            skipWs();
            if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
            for (;;) {
                skipWs();
                std::string key;
                if (!str(key)) return false;
                skipWs();
                if (pos >= s.size() || s[pos] != ':') return fail("expected ':'");
                ++pos;
                Value child;
                if (!value(child, depth + 1)) return false;
                v.keys.push_back(std::move(key));
                v.items.push_back(std::move(child));
                skipWs();
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++pos;
            v.type = Value::Type::Array;
            skipWs();
            if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
            for (;;) {
                Value child;
                if (!value(child, depth + 1)) return false;
                v.items.push_back(std::move(child));
                skipWs();
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Value::Type::String; return str(v.text); }
        if (c == 't') { v.type = Value::Type::Bool; v.boolean = true; return lit("true"); }
        if (c == 'f') { v.type = Value::Type::Bool; v.boolean = false; return lit("false"); }
        if (c == 'n') { v.type = Value::Type::Null; return lit("null"); }
        return number(v);
    }
};

} // namespace

bool parse(const std::string& source, Value& out, std::string* error) {
    Parser p(source);
    out = Value();
    bool ok = p.value(out, 0);
    if (ok) {
        p.skipWs();
        if (p.pos != source.size()) ok = p.fail("unexpected trailing characters");
    }
    if (!ok) {
        out = Value();
        if (error) *error = p.err;
    }
    return ok;
}

std::string quote(const std::string& text) {
    std::string out = "\"";
    for (std::size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"') { out += "\\\""; ++i; }
        else if (c == '\\') { out += "\\\\"; ++i; }
        else if (c == '\n') { out += "\\n"; ++i; }
        else if (c == '\r') { out += "\\r"; ++i; }
        else if (c == '\t') { out += "\\t"; ++i; }
        else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
            ++i;
        } else if (c < 0x80) { out.push_back(static_cast<char>(c)); ++i; }
        else {   // keep only well-formed UTF-8 sequences
            int n = (c >= 0xC2 && c <= 0xDF) ? 2 : (c >= 0xE0 && c <= 0xEF) ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
            bool good = n > 0 && i + static_cast<std::size_t>(n) <= text.size();
            for (int k = 1; good && k < n; ++k) good = (static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]) & 0xC0) == 0x80;
            if (good) { out.append(text, i, static_cast<std::size_t>(n)); i += static_cast<std::size_t>(n); }
            else { out.push_back('?'); ++i; }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace json
} // namespace scms
