// =====================================================================
// User.h - role hierarchy with polymorphic access control
//
//   User (abstract)
//   ├── Citizen         - own complaints only
//   ├── DepartmentUser  - complaints of its department only
//   └── Administrator   - system-wide
//
// The UI and the service never test roles with if/else chains; they ask the
// object: user->canView(c), user->canTransition(c, to). [OOPS-CO2 runtime polymorphism]
// =====================================================================
#pragma once
#include "Types.h"
#include <memory>
#include <string>

namespace scms {

class Complaint;

class User {
public:
    User(int id, std::string username, std::string fullName, std::string phone);
    virtual ~User() = default;

    virtual Role role() const = 0;
    virtual std::string portalName() const = 0;
    virtual bool canView(const Complaint& c) const = 0;
    virtual bool canTransition(const Complaint& c, Status to) const = 0;
    virtual bool canAccessAdminData() const { return false; }

    int id() const { return id_; }
    const std::string& username() const { return username_; }
    const std::string& fullName() const { return fullName_; }
    const std::string& phone() const { return phone_; }
    int cityId() const { return cityId_; }
    void setCityId(int c) { cityId_ = c; }
    std::string initials() const;

    // builds the right subclass for a row coming from PostgreSQL
    static std::unique_ptr<User> create(Role role, int id, const std::string& username,
                                        const std::string& fullName, const std::string& phone,
                                        int departmentId, const std::string& departmentName);

private:
    int id_;
    std::string username_, fullName_, phone_;
    int cityId_ = 0;
};

class Citizen : public User {
public:
    using User::User;
    Role role() const override { return Role::CITIZEN; }
    std::string portalName() const override { return "Citizen Portal"; }
    bool canView(const Complaint& c) const override;
    bool canTransition(const Complaint& c, Status to) const override;   // only RESOLVED -> CLOSED on own
};

class DepartmentUser : public User {
public:
    DepartmentUser(int id, std::string username, std::string fullName, std::string phone,
                   int departmentId, std::string departmentName);
    Role role() const override { return Role::DEPARTMENT; }
    std::string portalName() const override { return "Department Portal"; }
    bool canView(const Complaint& c) const override;
    bool canTransition(const Complaint& c, Status to) const override;   // ASSIGNED->IN_PROGRESS->RESOLVED
    int departmentId() const { return departmentId_; }
    const std::string& departmentName() const { return departmentName_; }

private:
    int departmentId_;
    std::string departmentName_;
};

class Administrator : public User {
public:
    using User::User;
    Role role() const override { return Role::ADMIN; }
    std::string portalName() const override { return "Administrator Portal"; }
    bool canView(const Complaint&) const override { return true; }
    bool canTransition(const Complaint& c, Status to) const override;
    bool canAccessAdminData() const override { return true; }
};

} // namespace scms
