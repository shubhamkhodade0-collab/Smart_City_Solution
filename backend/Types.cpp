#include "Types.h"
#include "Exceptions.h"

namespace scms {

const char* toString(Status s) {
    switch (s) {
        case Status::PENDING: return "PENDING";
        case Status::ASSIGNED: return "ASSIGNED";
        case Status::IN_PROGRESS: return "IN_PROGRESS";
        case Status::RESOLVED: return "RESOLVED";
        case Status::CLOSED: return "CLOSED";
    }
    return "PENDING";
}

const char* displayName(Status s) {
    switch (s) {
        case Status::PENDING: return "Pending";
        case Status::ASSIGNED: return "Assigned";
        case Status::IN_PROGRESS: return "In Progress";
        case Status::RESOLVED: return "Resolved";
        case Status::CLOSED: return "Closed";
    }
    return "Pending";
}

const char* toString(Role r) {
    switch (r) {
        case Role::CITIZEN: return "CITIZEN";
        case Role::DEPARTMENT: return "DEPARTMENT";
        case Role::ADMIN: return "ADMIN";
    }
    return "CITIZEN";
}

const char* toString(SeverityLevel l) {
    switch (l) {
        case SeverityLevel::LOW: return "LOW";
        case SeverityLevel::MEDIUM: return "MEDIUM";
        case SeverityLevel::HIGH: return "HIGH";
        case SeverityLevel::CRITICAL: return "CRITICAL";
    }
    return "LOW";
}

const char* toString(Priority p) {
    switch (p) {
        case Priority::LOW: return "LOW";
        case Priority::NORMAL: return "NORMAL";
        case Priority::HIGH: return "HIGH";
        case Priority::URGENT: return "URGENT";
    }
    return "NORMAL";
}

Status statusFromString(const std::string& s) {
    for (Status st : allStatuses())
        if (s == toString(st)) return st;
    throw ValidationException("status", "Unknown status '" + s + "'");
}

Role roleFromString(const std::string& s) {
    if (s == "CITIZEN") return Role::CITIZEN;
    if (s == "DEPARTMENT") return Role::DEPARTMENT;
    if (s == "ADMIN") return Role::ADMIN;
    throw ValidationException("role", "Unknown role '" + s + "'");
}

SeverityLevel severityFromString(const std::string& s) {
    if (s == "LOW") return SeverityLevel::LOW;
    if (s == "MEDIUM") return SeverityLevel::MEDIUM;
    if (s == "HIGH") return SeverityLevel::HIGH;
    if (s == "CRITICAL") return SeverityLevel::CRITICAL;
    throw ValidationException("severity", "Unknown severity '" + s + "'");
}

Priority priorityFromString(const std::string& s) {
    for (Priority p : allPriorities())
        if (s == toString(p)) return p;
    throw ValidationException("priority", "Unknown priority '" + s + "'");
}

const std::vector<Status>& allStatuses() {
    static const std::vector<Status> v{Status::PENDING, Status::ASSIGNED, Status::IN_PROGRESS,
                                       Status::RESOLVED, Status::CLOSED};
    return v;
}

const std::vector<Priority>& allPriorities() {
    static const std::vector<Priority> v{Priority::LOW, Priority::NORMAL, Priority::HIGH, Priority::URGENT};
    return v;
}

bool isValidTransition(Status from, Status to) {
    Status n;
    return nextStatus(from, n) && n == to;
}

bool nextStatus(Status from, Status& next) {
    switch (from) {
        case Status::PENDING: next = Status::ASSIGNED; return true;
        case Status::ASSIGNED: next = Status::IN_PROGRESS; return true;
        case Status::IN_PROGRESS: next = Status::RESOLVED; return true;
        case Status::RESOLVED: next = Status::CLOSED; return true;
        case Status::CLOSED: return false;
    }
    return false;
}

} // namespace scms
