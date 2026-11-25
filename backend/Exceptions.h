// =====================================================================
// Exceptions.h - application exception hierarchy
//
// [CO-EVIDENCE] OOPS-CO3: exception handling for wrong input and conflicts.
// Every failure path in the system throws one of these typed exceptions;
// the UI catches AppException and shows the message - it never reports
// "success" after a thrown exception.
// =====================================================================
#pragma once
#include <stdexcept>
#include <string>

namespace scms {

class AppException : public std::runtime_error {
public:
    explicit AppException(const std::string& msg) : std::runtime_error(msg) {}
    virtual const char* kind() const noexcept { return "Error"; }
};

// wrong user input (one field at a time so the form can highlight it)
class ValidationException : public AppException {
    std::string field_;
public:
    ValidationException(std::string field, const std::string& msg)
        : AppException(msg), field_(std::move(field)) {}
    const std::string& field() const noexcept { return field_; }
    const char* kind() const noexcept override { return "Validation"; }
};

class DatabaseException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Database"; }
};

class NotConnectedException : public DatabaseException {
public:
    NotConnectedException() : DatabaseException("PostgreSQL is not connected. Check that the server is running.") {}
    const char* kind() const noexcept override { return "Connection"; }
};

// lifecycle conflict: e.g. PENDING -> RESOLVED
class InvalidTransitionException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Lifecycle"; }
};

class DuplicateException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Duplicate"; }
};

class AccessDeniedException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Access denied"; }
};

class NotFoundException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Not found"; }
};

// underflow on Queue / Stack
class EmptyStructureException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Empty"; }
};

class CapacityException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Capacity"; }
};

class MapDataException : public AppException {
public:
    using AppException::AppException;
    const char* kind() const noexcept override { return "Map data"; }
};

} // namespace scms
