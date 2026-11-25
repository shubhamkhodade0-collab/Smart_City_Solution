// =====================================================================
// Validator.h - field validation; every failure throws ValidationException(field, message)
// [CO-EVIDENCE] OOPS-CO3: wrong input handled with exceptions + logic
// =====================================================================
#pragma once
#include <string>

namespace scms {

class Validator {
public:
    static void personName(const std::string& v, const char* field = "name");
    static void phone(const std::string& v);
    static void username(const std::string& v);
    static void password(const std::string& v, const std::string& confirm);
    static void locality(const std::string& v);
    static void description(const std::string& v);
    static void scale(int v, const char* field);          // 1..5
    static void requiredId(int id, const char* field);    // > 0
    static std::string trim(const std::string& s);
};

} // namespace scms
