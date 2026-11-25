// =====================================================================
// Department.h - a municipal department and its live workload
// Composition: a Department owns its Workload counters; ComplaintService
// owns a vector<Department> loaded from PostgreSQL.
// =====================================================================
#pragma once
#include "Types.h"
#include <string>
#include <vector>

namespace scms {

struct Workload {
    int assigned = 0, inProgress = 0, resolved = 0, closed = 0, highSeverity = 0;
    int open() const { return assigned + inProgress; }
    int total() const { return assigned + inProgress + resolved + closed; }
};

class Department {
public:
    Department(int id, std::string code, std::string name, std::string description);

    int id() const { return id_; }
    const std::string& code() const { return code_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const std::vector<std::string>& categories() const { return categories_; }

    void addCategory(const std::string& categoryCode) { categories_.push_back(categoryCode); }
    bool handles(const std::string& categoryCode) const;

    Workload& workload() { return workload_; }
    const Workload& workload() const { return workload_; }
    double resolutionRate() const;     // resolved+closed / total

private:
    int id_;
    std::string code_, name_, description_;
    std::vector<std::string> categories_;
    Workload workload_;                // composition
};

} // namespace scms
