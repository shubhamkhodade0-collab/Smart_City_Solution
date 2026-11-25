// =====================================================================
// Complaint.h - complaint domain model
//
//   Complaint (abstract)
//   ├── TrafficComplaint
//   ├── GarbageComplaint
//   ├── ElectricityComplaint
//   ├── WaterComplaint
//   ├── RoadComplaint
//   └── DrainageComplaint
//
// [CO-EVIDENCE] OOPS-CO2: abstraction (pure virtual), inheritance, method overriding,
//               runtime polymorphism through Complaint* / virtual calls, encapsulation
//               (private state + validated setters), constructors/destructor, this pointer,
//               static members, friend class/function.
// =====================================================================
#pragma once
#include "Types.h"
#include <ctime>
#include <iosfwd>
#include <memory>
#include <string>

namespace scms {

class Database;   // friend: the persistence layer may set DB-generated fields

struct Location {
    int cityId = 0;
    int districtId = 0;
    int stateId = 0;
    std::string city, district, state, locality;
};

class Complaint {
public:
    Complaint(std::string categoryCode, std::string citizenName, std::string phone,
              Location loc, std::string description, int impact, int urgency, Priority priority);
    virtual ~Complaint();

    Complaint(const Complaint&) = delete;            // complaints are owned by exactly one list
    Complaint& operator=(const Complaint&) = delete;

    // ---- polymorphic behaviour (overridden by every category) -------------
    virtual const char* typeName() const = 0;         // pure virtual -> abstract class
    virtual int asmCategoryCode() const = 0;          // index into Assembly weight table
    virtual int slaHours() const = 0;                 // resolution target
    virtual std::string handlingAdvice() const = 0;   // shown in complaint details
    virtual bool isPublicSafetyRisk() const;          // default: severity >= HIGH

    // ---- behaviour common to all complaints -------------------------------
    int computeSeverity();                    // calls Assembly, stores score + level
    void transitionTo(Status next);           // throws InvalidTransitionException
    bool isOverdue(std::time_t now) const;
    bool matches(const std::string& needle) const;   // case-insensitive, Assembly scan

    // ---- accessors (encapsulation: read-only from outside) ----------------
    long long id() const { return id_; }
    const std::string& referenceCode() const { return reference_; }
    int citizenId() const { return citizenId_; }
    const std::string& citizenName() const { return citizenName_; }
    const std::string& phone() const { return phone_; }
    const Location& location() const { return loc_; }
    const std::string& categoryCode() const { return categoryCode_; }
    const std::string& description() const { return description_; }
    int impact() const { return impact_; }
    int urgency() const { return urgency_; }
    int severityScore() const { return severityScore_; }
    SeverityLevel severityLevel() const { return severityLevel_; }
    Priority priority() const { return priority_; }
    Status status() const { return status_; }
    int departmentId() const { return departmentId_; }
    const std::string& departmentName() const { return departmentName_; }
    std::time_t createdAt() const { return createdAt_; }
    std::time_t updatedAt() const { return updatedAt_; }

    void setCitizenId(int id) { citizenId_ = id; }
    void assignDepartment(int departmentId, const std::string& name);   // PENDING -> ASSIGNED

    static long long liveInstances() { return liveInstances_; }
    static std::unique_ptr<Complaint> create(const std::string& categoryCode, const std::string& citizenName,
                                             const std::string& phone, const Location& loc,
                                             const std::string& description, int impact, int urgency,
                                             Priority priority);    // factory -> concrete subclass

    friend class Database;
    friend class ComplaintService;
    friend std::ostream& operator<<(std::ostream& os, const Complaint& c);

protected:
    std::string categoryCode_;

private:
    long long id_ = 0;
    std::string reference_;
    int citizenId_ = 0;
    std::string citizenName_, phone_;
    Location loc_;
    std::string description_;
    int impact_, urgency_;
    int severityScore_ = 0;
    SeverityLevel severityLevel_ = SeverityLevel::LOW;
    Priority priority_;
    Status status_ = Status::PENDING;
    int departmentId_ = 0;
    std::string departmentName_;
    std::time_t createdAt_, updatedAt_;

    static long long liveInstances_;
};

// ---------------------------------------------------------------------------
class TrafficComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Traffic"; }
    int asmCategoryCode() const override { return 0; }
    int slaHours() const override { return 24; }
    std::string handlingAdvice() const override;
};

class GarbageComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Garbage"; }
    int asmCategoryCode() const override { return 1; }
    int slaHours() const override { return 48; }
    std::string handlingAdvice() const override;
};

class ElectricityComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Electricity"; }
    int asmCategoryCode() const override { return 2; }
    int slaHours() const override { return 12; }
    std::string handlingAdvice() const override;
    bool isPublicSafetyRisk() const override;     // exposed wiring is always a risk
};

class WaterComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Water"; }
    int asmCategoryCode() const override { return 3; }
    int slaHours() const override { return 24; }
    std::string handlingAdvice() const override;
    bool isPublicSafetyRisk() const override;     // contamination
};

class RoadComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Roads"; }
    int asmCategoryCode() const override { return 4; }
    int slaHours() const override { return 72; }
    std::string handlingAdvice() const override;
};

class DrainageComplaint : public Complaint {
public:
    using Complaint::Complaint;
    const char* typeName() const override { return "Drainage"; }
    int asmCategoryCode() const override { return 5; }
    int slaHours() const override { return 36; }
    std::string handlingAdvice() const override;
};

} // namespace scms
