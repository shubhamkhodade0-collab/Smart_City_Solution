#include "User.h"
#include "Complaint.h"
#include <cctype>

namespace scms {

User::User(int id, std::string username, std::string fullName, std::string phone)
    : id_(id), username_(std::move(username)), fullName_(std::move(fullName)), phone_(std::move(phone)) {}

std::string User::initials() const {
    std::string out;
    bool take = true;
    for (char ch : fullName_) {
        if (ch == ' ') { take = true; continue; }
        if (take && out.size() < 2) out += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        take = false;
    }
    return out.empty() ? "?" : out;
}

std::unique_ptr<User> User::create(Role role, int id, const std::string& username, const std::string& fullName,
                                   const std::string& phone, int departmentId, const std::string& departmentName) {
    switch (role) {
        case Role::CITIZEN: return std::make_unique<Citizen>(id, username, fullName, phone);
        case Role::DEPARTMENT:
            return std::make_unique<DepartmentUser>(id, username, fullName, phone, departmentId, departmentName);
        case Role::ADMIN: return std::make_unique<Administrator>(id, username, fullName, phone);
    }
    return nullptr;
}

bool Citizen::canView(const Complaint& c) const { return c.citizenId() == id(); }

bool Citizen::canTransition(const Complaint& c, Status to) const {
    // a citizen may only confirm (close) their own resolved complaint
    return canView(c) && c.status() == Status::RESOLVED && to == Status::CLOSED;
}

DepartmentUser::DepartmentUser(int id, std::string username, std::string fullName, std::string phone,
                               int departmentId, std::string departmentName)
    : User(id, std::move(username), std::move(fullName), std::move(phone)),
      departmentId_(departmentId), departmentName_(std::move(departmentName)) {}

bool DepartmentUser::canView(const Complaint& c) const { return c.departmentId() == departmentId_; }

bool DepartmentUser::canTransition(const Complaint& c, Status to) const {
    if (!canView(c) || !isValidTransition(c.status(), to)) return false;
    return to == Status::IN_PROGRESS || to == Status::RESOLVED;
}

bool Administrator::canTransition(const Complaint& c, Status to) const {
    return isValidTransition(c.status(), to);
}

} // namespace scms
