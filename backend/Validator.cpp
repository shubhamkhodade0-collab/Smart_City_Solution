#include "Validator.h"
#include "Exceptions.h"
#include <cctype>

namespace scms {

std::string Validator::trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

void Validator::personName(const std::string& raw, const char* field) {
    std::string v = trim(raw);
    if (v.size() < 2) throw ValidationException(field, "Name must have at least 2 characters.");
    if (v.size() > 60) throw ValidationException(field, "Name is too long (max 60 characters).");
    for (unsigned char c : v)
        if (!std::isalpha(c) && c != ' ' && c != '.' && c != '\'' && c != '-')
            throw ValidationException(field, "Name may contain only letters, spaces, '.', '-' and apostrophes.");
}

void Validator::phone(const std::string& raw) {
    std::string v = trim(raw);
    if (v.size() != 10) throw ValidationException("phone", "Phone must be a 10-digit Indian mobile number.");
    for (unsigned char c : v)
        if (!std::isdigit(c)) throw ValidationException("phone", "Phone must contain digits only.");
    if (v[0] < '6') throw ValidationException("phone", "Indian mobile numbers start with 6, 7, 8 or 9.");
}

void Validator::username(const std::string& raw) {
    std::string v = trim(raw);
    if (v.size() < 3 || v.size() > 40) throw ValidationException("username", "Username must be 3-40 characters.");
    for (unsigned char c : v)
        if (!(std::islower(c) || std::isdigit(c) || c == '.' || c == '_'))
            throw ValidationException("username", "Use lowercase letters, digits, '.' or '_' only.");
}

void Validator::password(const std::string& v, const std::string& confirm) {
    if (v.size() < 8) throw ValidationException("password", "Password must have at least 8 characters.");
    bool up = false, low = false, dig = false;
    for (unsigned char c : v) {
        up |= std::isupper(c) != 0;
        low |= std::islower(c) != 0;
        dig |= std::isdigit(c) != 0;
    }
    if (!up || !low || !dig)
        throw ValidationException("password", "Password needs an uppercase letter, a lowercase letter and a digit.");
    if (v != confirm) throw ValidationException("confirm", "Passwords do not match.");
}

void Validator::locality(const std::string& raw) {
    std::string v = trim(raw);
    if (v.size() < 3) throw ValidationException("locality", "Enter the locality / landmark (at least 3 characters).");
    if (v.size() > 120) throw ValidationException("locality", "Locality is too long (max 120 characters).");
}

void Validator::description(const std::string& raw) {
    std::string v = trim(raw);
    if (v.size() < 15) throw ValidationException("description", "Describe the problem in at least 15 characters.");
    if (v.size() > 1000) throw ValidationException("description", "Description is too long (max 1000 characters).");
}

void Validator::scale(int v, const char* field) {
    if (v < 1 || v > 5) throw ValidationException(field, std::string("Select a value from 1 to 5 for ") + field + ".");
}

void Validator::requiredId(int id, const char* field) {
    if (id <= 0) throw ValidationException(field, std::string("Select a ") + field + ".");
}

} // namespace scms
