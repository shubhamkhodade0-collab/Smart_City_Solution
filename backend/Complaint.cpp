#include "Complaint.h"
#include "AsmEngine.h"
#include "Exceptions.h"
#include <ostream>

namespace scms {

long long Complaint::liveInstances_ = 0;

Complaint::Complaint(std::string categoryCode, std::string citizenName, std::string phone,
                     Location loc, std::string description, int impact, int urgency, Priority priority)
    : categoryCode_(std::move(categoryCode)),
      citizenName_(std::move(citizenName)),
      phone_(std::move(phone)),
      loc_(std::move(loc)),
      description_(std::move(description)),
      impact_(impact),
      urgency_(urgency),
      priority_(priority),
      createdAt_(std::time(nullptr)),
      updatedAt_(createdAt_) {
    ++liveInstances_;   // static member: number of Complaint objects alive in memory
}

Complaint::~Complaint() { --liveInstances_; }

bool Complaint::isPublicSafetyRisk() const {
    return severityLevel_ == SeverityLevel::HIGH || severityLevel_ == SeverityLevel::CRITICAL;
}

// Polymorphic call: asmCategoryCode() resolves to the concrete subclass at runtime,
// then the value travels to Assembly in RCX.
int Complaint::computeSeverity() {
    this->severityScore_ = AsmEngine::severityScore(this->asmCategoryCode(), impact_, urgency_);
    this->severityLevel_ = static_cast<SeverityLevel>(AsmEngine::classify(severityScore_));
    return severityScore_;
}

void Complaint::transitionTo(Status next) {
    if (!isValidTransition(status_, next))
        throw InvalidTransitionException(std::string("Invalid status change for ") + reference_ + ": " +
                                         displayName(status_) + " -> " + displayName(next) +
                                         ". Allowed order is Pending -> Assigned -> In Progress -> Resolved -> Closed.");
    if (next == Status::ASSIGNED && departmentId_ == 0)
        throw InvalidTransitionException("A complaint must have a department before it is assigned.");
    status_ = next;
    updatedAt_ = std::time(nullptr);
}

void Complaint::assignDepartment(int departmentId, const std::string& name) {
    if (departmentId <= 0) throw ValidationException("department", "Invalid department");
    departmentId_ = departmentId;
    departmentName_ = name;
    transitionTo(Status::ASSIGNED);
}

bool Complaint::isOverdue(std::time_t now) const {
    if (status_ == Status::RESOLVED || status_ == Status::CLOSED) return false;
    return std::difftime(now, createdAt_) > slaHours() * 3600.0;
}

bool Complaint::matches(const std::string& needle) const {
    // every searchable field goes through the Assembly case-insensitive scanner
    const std::string fields[] = {reference_, std::to_string(id_), citizenName_, loc_.city, loc_.district,
                                  loc_.state, loc_.locality, typeName(), toString(status_), departmentName_,
                                  description_, toString(severityLevel_), toString(priority_)};
    for (const auto& f : fields)
        if (AsmEngine::containsIgnoreCase(f, needle)) return true;
    return false;
}

std::unique_ptr<Complaint> Complaint::create(const std::string& code, const std::string& name,
                                             const std::string& phone, const Location& loc,
                                             const std::string& desc, int impact, int urgency, Priority p) {
    if (code == "TRAFFIC") return std::make_unique<TrafficComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    if (code == "GARBAGE") return std::make_unique<GarbageComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    if (code == "ELECTRICITY") return std::make_unique<ElectricityComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    if (code == "WATER") return std::make_unique<WaterComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    if (code == "ROADS") return std::make_unique<RoadComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    if (code == "DRAINAGE") return std::make_unique<DrainageComplaint>(code, name, phone, loc, desc, impact, urgency, p);
    throw ValidationException("category", "Unknown complaint category '" + code + "'");
}

std::ostream& operator<<(std::ostream& os, const Complaint& c) {
    return os << c.reference_ << " [" << c.typeName() << ", " << toString(c.status_) << ", severity "
              << c.severityScore_ << "] " << c.loc_.locality << ", " << c.loc_.city;
}

// ---- overrides -------------------------------------------------------------
std::string TrafficComplaint::handlingAdvice() const {
    return "Dispatch traffic personnel; check signal timing and enforce no-parking zones.";
}
std::string GarbageComplaint::handlingAdvice() const {
    return "Schedule a pickup vehicle within the collection window and sanitise the spot.";
}
std::string ElectricityComplaint::handlingAdvice() const {
    return "Isolate the feeder before inspection; exposed conductors must be cordoned off.";
}
bool ElectricityComplaint::isPublicSafetyRisk() const {
    return Complaint::isPublicSafetyRisk() || impact() >= 4;
}
std::string WaterComplaint::handlingAdvice() const {
    return "Collect a water sample, inspect the pipeline joint and issue a boil-water advisory if needed.";
}
bool WaterComplaint::isPublicSafetyRisk() const {
    return Complaint::isPublicSafetyRisk() || urgency() >= 4;
}
std::string RoadComplaint::handlingAdvice() const {
    return "Barricade the damaged stretch and schedule patching with the PWD maintenance crew.";
}
std::string DrainageComplaint::handlingAdvice() const {
    return "Desilt the drain line and deploy pumps if waterlogging exceeds 15 cm.";
}

} // namespace scms
