// =====================================================================
// Types.h - enums shared by backend, DSA and frontend + string mapping
// =====================================================================
#pragma once
#include <string>
#include <vector>

namespace scms {

enum class Status { PENDING, ASSIGNED, IN_PROGRESS, RESOLVED, CLOSED };
enum class Role { CITIZEN, DEPARTMENT, ADMIN };
enum class SeverityLevel { LOW = 1, MEDIUM = 2, HIGH = 3, CRITICAL = 4 };
enum class Priority { LOW, NORMAL, HIGH, URGENT };

const char* toString(Status s);
const char* toString(Role r);
const char* toString(SeverityLevel l);
const char* toString(Priority p);
const char* displayName(Status s);     // "In Progress"

Status statusFromString(const std::string& s);       // throws ValidationException
Role roleFromString(const std::string& s);
SeverityLevel severityFromString(const std::string& s);
Priority priorityFromString(const std::string& s);

const std::vector<Status>& allStatuses();
const std::vector<Priority>& allPriorities();

// The single place that encodes the lifecycle
//   PENDING -> ASSIGNED -> IN_PROGRESS -> RESOLVED -> CLOSED
bool isValidTransition(Status from, Status to);
bool nextStatus(Status from, Status& next);   // false for CLOSED

} // namespace scms
