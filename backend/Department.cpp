#include "Department.h"

namespace scms {

Department::Department(int id, std::string code, std::string name, std::string description)
    : id_(id), code_(std::move(code)), name_(std::move(name)), description_(std::move(description)) {}

bool Department::handles(const std::string& categoryCode) const {
    for (const auto& c : categories_)
        if (c == categoryCode) return true;
    return false;
}

double Department::resolutionRate() const {
    int t = workload_.total();
    return t == 0 ? 0.0 : 100.0 * (workload_.resolved + workload_.closed) / t;
}

} // namespace scms
