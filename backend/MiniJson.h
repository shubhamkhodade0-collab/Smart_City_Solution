// =====================================================================
// MiniJson.h - a tiny JSON reader / writer (used by the optional Groq connection)
//
//   json::parse(text, value)   strict RFC 8259 parser (objects, arrays, strings with \uXXXX escapes, numbers,
//                              true/false/null); returns false with a message on malformed input
//   json::quote(text)          turns text into a JSON string literal (with the quotes); invalid UTF-8 bytes
//                              are replaced by '?', so the result is always valid JSON
// =====================================================================
#pragma once
#include <string>
#include <vector>

namespace scms {
namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string text;               // String value
    std::vector<Value> items;       // Array elements, or Object values
    std::vector<std::string> keys;  // Object keys (same order / length as items)

    const Value* get(const std::string& key) const;   // object member or nullptr
    const Value* at(std::size_t index) const;         // array element or nullptr
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
    std::string asString(const std::string& fallback = "") const { return type == Type::String ? text : fallback; }
};

bool parse(const std::string& source, Value& out, std::string* error = nullptr);
std::string quote(const std::string& text);

} // namespace json
} // namespace scms
