#include "ComplaintService.h"
#include "AsmEngine.h"
#include "Exceptions.h"
#include "Validator.h"
#include <cctype>

namespace scms {

ComplaintService::ComplaintService(Database& db)
    : db_(db), index_(211), statusStack_(16), adminActions_(30) {}

ComplaintService::~ComplaintService() = default;   // members (DLL, hash, queue, stacks) clean themselves up

// ============================================================ session
std::unique_ptr<User> ComplaintService::login(const std::string& username, const std::string& password) {
    if (Validator::trim(username).empty() || password.empty())
        throw ValidationException("username", "Enter your username and password.");
    auto user = db_.authenticateUser(Validator::trim(username), password);
    if (!user) {
        db_.createAuditLog(0, "LOGIN_FAILED", "USER", Validator::trim(username), "Invalid username or password");
        throw AccessDeniedException("Invalid username or password.");
    }
    db_.createAuditLog(user->id(), "LOGIN", "USER", user->username(), user->portalName());
    return user;
}

void ComplaintService::logout(const User& u) {
    db_.createAuditLog(u.id(), "LOGOUT", "USER", u.username(), "");
}

int ComplaintService::registerCitizen(const NewUser& in, const std::string& confirm) {
    NewUser u = in;
    u.role = Role::CITIZEN;
    u.departmentId = 0;
    u.username = Validator::trim(u.username);
    Validator::personName(u.fullName, "fullName");
    Validator::username(u.username);
    Validator::phone(u.phone);
    Validator::password(u.password, confirm);
    if (u.cityId > 0) db_.getCity(u.cityId);    // throws NotFoundException for an invalid city
    try {
        int id = db_.createUser(u);
        db_.createAuditLog(id, "REGISTER", "USER", u.username, "Citizen self-registration");
        return id;
    } catch (const DuplicateException&) {
        throw DuplicateException("The username '" + u.username + "' is already registered.");
    }
}

int ComplaintService::createStaffUser(const User& admin, const NewUser& in, const std::string& confirm) {
    if (!admin.canAccessAdminData()) throw AccessDeniedException("Only administrators can create users.");
    NewUser u = in;
    u.username = Validator::trim(u.username);
    Validator::personName(u.fullName, "fullName");
    Validator::username(u.username);
    Validator::phone(u.phone);
    Validator::password(u.password, confirm);
    if (u.role == Role::DEPARTMENT) Validator::requiredId(u.departmentId, "department");
    else u.departmentId = 0;
    try {
        Database::Transaction tx(db_);
        int id = db_.createUser(u);
        db_.createAuditLog(admin.id(), "USER_CREATED", "USER", u.username, std::string("Role ") + toString(u.role));
        db_.createNotification(admin.id(), 0, "ADMIN_ACTION", "User created", "Account '" + u.username + "' was created.");
        tx.commit();
        adminActions_.pushBounded({"USER_CREATED", u.username, std::time(nullptr)});
        return id;
    } catch (const DuplicateException&) {
        throw DuplicateException("The username '" + u.username + "' already exists.");
    }
}

// ============================================================ reference data
void ComplaintService::loadReferenceData() {
    categories_ = db_.getCategories();
    departments_ = db_.getDepartments();
    db_.loadDepartmentWorkload(departments_);
}

const CategoryRow* ComplaintService::categoryByCode(const std::string& code) const {
    for (const auto& c : categories_)
        if (c.code == code) return &c;
    return nullptr;
}

Department* ComplaintService::departmentFor(const std::string& categoryCode) {
    for (auto& d : departments_)
        if (d.handles(categoryCode)) return &d;
    return nullptr;
}

Scope ComplaintService::scopeFor(const User& u) const {
    Scope s;
    if (u.role() == Role::CITIZEN) s.citizenId = u.id();
    if (u.role() == Role::DEPARTMENT) s.departmentId = static_cast<const DepartmentUser&>(u).departmentId();
    return s;
}

// ============================================================ runtime structures
void ComplaintService::indexComplaint(Complaint* c) {
    index_.insert(c->referenceCode(), c);
    index_.insert(std::to_string(c->id()), c);
}

void ComplaintService::unindexComplaint(const Complaint& c) {
    index_.remove(c.referenceCode());
    index_.remove(std::to_string(c.id()));
}

void ComplaintService::loadRuntime(const User& u) {
    if (categories_.empty()) loadReferenceData();
    active_.clear();
    index_.clear();
    queue_.clear();
    // active complaints visible to this user -> DLL (owner) + hash index
    ComplaintFilter f;
    f.scope = scopeFor(u);
    f.activeOnly = true;
    f.orderBy = "oldest";
    f.limit = 20000;
    for (auto& c : db_.getComplaints(f)) indexComplaint(active_.insertBack(std::move(c)));
    // the FIFO queue is system-wide: every PENDING complaint in arrival order
    for (auto& [id, ref] : db_.pendingQueueOrder()) {
        Complaint* c = index_.search(ref);
        QueueItem item{id, ref, c ? c->categoryCode() : std::string(), c ? c->severityScore() : 0, c ? c->createdAt() : 0};
        if (!c) {
            if (auto fresh = db_.getComplaintById(id)) {
                item.categoryCode = fresh->categoryCode();
                item.severity = fresh->severityScore();
                item.enqueuedAt = fresh->createdAt();
            }
        }
        queue_.enqueue(item);
    }
    db_.loadDepartmentWorkload(departments_);
}

void ComplaintService::notifyAll(const std::vector<int>& users, long long complaintId, const std::string& type,
                                 const std::string& title, const std::string& message) {
    for (int uid : users) db_.createNotification(uid, complaintId, type, title, message);
}

// ============================================================ submit (14-step pipeline)
SubmitResult ComplaintService::submitComplaint(const User& user, const ComplaintInput& in) {
    if (user.role() != Role::CITIZEN) throw AccessDeniedException("Only citizens can report complaints.");
    SubmitResult res;

    // 1. validate input
    Validator::personName(in.citizenName, "citizenName");
    Validator::phone(in.phone);
    Validator::requiredId(in.stateId, "state");
    Validator::requiredId(in.districtId, "district");
    Validator::requiredId(in.cityId, "city");
    Validator::locality(in.locality);
    const CategoryRow* cat = categoryByCode(in.categoryCode);
    if (!cat) throw ValidationException("category", "Select a complaint category.");
    Validator::description(in.description);
    Validator::scale(in.impact, "impact");
    Validator::scale(in.urgency, "urgency");
    Priority priority = priorityFromString(in.priority);
    CityInfo city = db_.getCity(in.cityId);
    if (city.districtId != in.districtId) throw ValidationException("city", "The city does not belong to the selected district.");
    if (city.stateId != in.stateId) throw ValidationException("district", "The district does not belong to the selected state.");
    if (db_.duplicateExists(user.id(), in.cityId, in.categoryCode, Validator::trim(in.description)))
        throw DuplicateException("You already reported this exact problem in this city within the last 7 days.");
    res.steps.push_back("1. Input validated (12 fields, city -> district -> state verified in PostgreSQL)");

    // 4. create C++ object (factory picks the subclass)
    Location loc{city.cityId, city.districtId, city.stateId, city.city, city.district, city.state,
                 Validator::trim(in.locality)};
    std::unique_ptr<Complaint> obj = Complaint::create(in.categoryCode, Validator::trim(in.citizenName),
                                                       Validator::trim(in.phone), loc, Validator::trim(in.description),
                                                       in.impact, in.urgency, priority);
    obj->setCitizenId(user.id());

    // 2 + 3. severity via Assembly (virtual asmCategoryCode() -> RCX)
    res.severityScore = obj->computeSeverity();
    res.severityLevel = toString(obj->severityLevel());
    res.steps.push_back("2. Severity calculated  3. calculateSeverityAsm(" + std::to_string(obj->asmCategoryCode()) + ", " +
                        std::to_string(in.impact) + ", " + std::to_string(in.urgency) + ") = " +
                        std::to_string(res.severityScore) + " -> " + res.severityLevel);

    obj->id_ = db_.reserveComplaintId();
    obj->reference_ = db_.makeReference(obj->id_);
    res.steps.push_back(std::string("4. ") + obj->typeName() + "Complaint object created (" + obj->reference_ + ")");

    // 5-7. DSA inserts
    Complaint* c = active_.insertBack(std::move(obj));
    indexComplaint(c);
    queue_.enqueue({c->id(), c->referenceCode(), c->categoryCode(), c->severityScore(), c->createdAt()});
    res.steps.push_back("5. Inserted into Doubly Linked List (size " + std::to_string(active_.size()) + ")");
    res.steps.push_back("6. Inserted into Hash Table (bucket " + std::to_string(index_.bucketOf(c->referenceCode())) + ")");
    res.steps.push_back("7. Enqueued in FIFO queue (position " + std::to_string(queue_.size()) + ")");

    // 8-12. PostgreSQL transaction - on failure every DSA change is undone
    try {
        Database::Transaction tx(db_);
        db_.insertComplaint(*c, cat->id);
        db_.addHistory(c->id(), "CREATED", "Complaint registered by " + c->citizenName() + " (severity " +
                       std::to_string(c->severityScore()) + ")", user.id());
        db_.createNotification(user.id(), c->id(), "COMPLAINT_CREATED", "Complaint registered",
                               c->referenceCode() + " was received and is waiting for department assignment.");
        notifyAll(db_.getUserIdsByRole(Role::ADMIN), c->id(), "COMPLAINT_CREATED", "New complaint " + c->referenceCode(),
                  std::string(c->typeName()) + " issue in " + c->location().city + " - severity " + res.severityLevel);
        db_.createAuditLog(user.id(), "COMPLAINT_CREATED", "COMPLAINT", c->referenceCode(), c->location().city);
        tx.commit();
    } catch (...) {
        long long id = c->id();
        std::string ref = c->referenceCode();
        unindexComplaint(*c);
        active_.remove(id);
        // rebuild queue without the failed item (FIFO has no middle delete by design)
        ComplaintQueue keep;
        while (!queue_.isEmpty()) {
            QueueItem q = queue_.dequeue();
            if (q.complaintId != id) keep.enqueue(q);
        }
        while (!keep.isEmpty()) queue_.enqueue(keep.dequeue());
        throw;   // the UI shows the error - never "success"
    }
    res.steps.push_back("8. Inserted into PostgreSQL (complaints + complaint_status_history)");
    res.steps.push_back("9. Complaint history created   10. Notifications created");
    db_.loadDepartmentWorkload(departments_);
    res.steps.push_back("11. City statistics refreshed from PostgreSQL   12. Department workload updated");
    res.id = c->id();
    res.reference = c->referenceCode();
    res.queuePosition = queue_.size();
    res.steps.push_back("13. Complaint ID generated: " + res.reference);
    return res;
}

// ============================================================ FIFO processing
ProcessResult ComplaintService::processNextInQueue(const User& actor) {
    if (!actor.canAccessAdminData()) throw AccessDeniedException("Only administrators can run the assignment queue.");
    ProcessResult r;
    QueueItem item = queue_.dequeue();                                    // 1. dequeue (throws if empty)
    r.reference = item.reference;
    r.steps.push_back("1. Dequeued " + item.reference + " (front of FIFO)");

    std::unique_ptr<Complaint> fresh = db_.getComplaintById(item.complaintId);
    if (!fresh || fresh->status() != Status::PENDING) {
        r.steps.push_back("Skipped: complaint is no longer PENDING in PostgreSQL");
        r.remaining = queue_.size();
        return r;
    }
    r.category = fresh->typeName();                                       // 2. determine category
    r.steps.push_back(std::string("2. Category determined: ") + fresh->typeName());
    Department* dep = departmentFor(fresh->categoryCode());               // 3. find Department object
    if (!dep) {
        queue_.enqueue(item);
        throw NotFoundException("No department handles category " + fresh->categoryCode());
    }
    r.department = dep->name();
    r.steps.push_back("3. Department object found: " + dep->name());

    try {
        Database::Transaction tx(db_);
        db_.assignDepartment(item.complaintId, dep->id(), actor.id());        // 4-6
        db_.addHistory(item.complaintId, "ASSIGNED", "Assigned to " + dep->name() + " by the FIFO processor", actor.id());
        db_.createNotification(fresh->citizenId(), item.complaintId, "COMPLAINT_ASSIGNED", "Complaint assigned",
                               item.reference + " was assigned to " + dep->name() + ".");
        notifyAll(db_.getDepartmentUserIds(dep->id()), item.complaintId, "DEPARTMENT_ASSIGNMENT",
                  "New work: " + item.reference,
                  std::string(fresh->typeName()) + " complaint in " + fresh->location().city + " (severity " +
                      std::to_string(fresh->severityScore()) + ")");
        db_.createAuditLog(actor.id(), "COMPLAINT_ASSIGNED", "COMPLAINT", item.reference, dep->name());
        tx.commit();
    } catch (...) {
        // restore exact FIFO order from PostgreSQL, then report the failure
        try { loadRuntime(actor); } catch (...) {}
        throw;
    }
    r.steps.push_back("4. Department assigned   5. Status -> ASSIGNED   6. PostgreSQL updated");
    r.steps.push_back("7. History created   8. Notifications sent to citizen and department");

    if (Complaint* c = index_.search(item.reference)) c->assignDepartment(dep->id(), dep->name());   // keep DSA in sync
    ++processed_;
    lastProcessed_ = item.reference;
    adminActions_.pushBounded({"ASSIGNED " + item.reference, dep->code(), std::time(nullptr)});
    db_.loadDepartmentWorkload(departments_);
    r.remaining = queue_.size();
    return r;
}

// ============================================================ lifecycle
void ComplaintService::changeStatus(const User& actor, long long id, Status to, const std::string& note) {
    std::unique_ptr<Complaint> c = db_.getComplaintById(id);
    if (!c) throw NotFoundException("Complaint not found.");
    if (!actor.canView(*c)) throw AccessDeniedException("You do not have access to this complaint.");
    if (!isValidTransition(c->status(), to))
        throw InvalidTransitionException(std::string("Invalid transition ") + displayName(c->status()) + " -> " +
                                         displayName(to) + ". Allowed: Pending -> Assigned -> In Progress -> Resolved -> Closed.");
    if (to == Status::ASSIGNED) throw InvalidTransitionException("Assignment is done by the FIFO processing queue.");
    if (!actor.canTransition(*c, to))
        throw AccessDeniedException(actor.portalName() + " cannot move a complaint to " + displayName(to) + ".");

    Status from = c->status();
    c->transitionTo(to);   // C++ object validates again (single lifecycle rule in Types.cpp)

    std::string evt = to == Status::RESOLVED ? "RESOLVED" : to == Status::CLOSED ? "CLOSED" : "STATUS_CHANGED";
    std::string ntype = to == Status::RESOLVED ? "COMPLAINT_RESOLVED" : to == Status::CLOSED ? "COMPLAINT_CLOSED" : "STATUS_CHANGED";
    std::string audit = to == Status::RESOLVED ? "COMPLAINT_RESOLVED" : to == Status::CLOSED ? "COMPLAINT_CLOSED" : "STATUS_CHANGED";
    std::string msg = c->referenceCode() + ": " + displayName(from) + " -> " + displayName(to);
    std::string cleanNote = Validator::trim(note);

    {
        Database::Transaction tx(db_);
        db_.updateStatus(id, from, to, actor.id(), cleanNote.empty() ? msg : cleanNote);
        db_.addHistory(id, evt, msg + (cleanNote.empty() ? "" : " - " + cleanNote), actor.id());
        if (actor.id() != c->citizenId())
            db_.createNotification(c->citizenId(), id, ntype, std::string("Status: ") + displayName(to), msg);
        if (actor.role() != Role::DEPARTMENT && c->departmentId() > 0)
            notifyAll(db_.getDepartmentUserIds(c->departmentId()), id, ntype, std::string("Status: ") + displayName(to), msg);
        db_.createAuditLog(actor.id(), audit, "COMPLAINT", c->referenceCode(), msg);
        tx.commit();
    }

    // DSA sync: update the live object, or delete it from DLL + hash once CLOSED
    if (to == Status::CLOSED) {
        if (Complaint* live = index_.search(c->referenceCode())) {
            unindexComplaint(*live);
            active_.remove(live->id());
        }
    } else {
        active_.update(id, [to](Complaint& live) {
            if (isValidTransition(live.status(), to)) live.transitionTo(to);
        });
    }
    if (actor.canAccessAdminData()) adminActions_.pushBounded({std::string(toString(to)) + " " + c->referenceCode(), cleanNote, std::time(nullptr)});
    db_.loadDepartmentWorkload(departments_);
}

std::unique_ptr<Complaint> ComplaintService::openComplaint(const User& viewer, long long id) {
    std::unique_ptr<Complaint> c = db_.getComplaintById(id);
    if (!c) throw NotFoundException("Complaint not found.");
    if (!viewer.canView(*c)) throw AccessDeniedException("You do not have access to this complaint.");
    statusStack_.clear();
    for (const auto& row : db_.getStatusHistory(id)) statusStack_.push({row.newStatus, row.actor + " - " + row.when, 0});
    return c;
}

// ============================================================ search
SearchOutcome ComplaintService::search(const User& viewer, const std::string& rawText) {
    SearchOutcome out;
    std::string text = Validator::trim(rawText);
    if (text.empty()) return out;
    std::vector<long long> seen;
    auto addUnique = [&](std::unique_ptr<Complaint> c) {
        for (long long s : seen)
            if (s == c->id()) return;
        seen.push_back(c->id());
        out.results.push_back(std::move(c));
    };

    // 1) O(1) Hash Table lookup when the text looks like a reference code or numeric id
    bool numeric = !text.empty();
    for (unsigned char ch : text) numeric &= std::isdigit(ch) != 0;
    if (numeric || startsWithIgnoreCase(text, "SCMS-")) {
        out.hashUsed = true;
        Complaint* hit = index_.search(text);
        out.hashProbes = index_.stats().lastProbes;
        if (hit && viewer.canView(*hit)) {
            out.hashHit = true;
            if (auto fresh = db_.getComplaintById(hit->id())) addUnique(std::move(fresh));
        }
    }

    // 2) Linear Search over the Doubly Linked List (each field scanned by asm_contains_ci)
    std::vector<Complaint*> list = active_.toVector(true);
    std::vector<std::size_t> idx;
    out.linear = linearSearch(list, text, [&](Complaint* c) { return viewer.canView(*c) && c->matches(text); }, idx);
    for (std::size_t i : idx)
        if (auto fresh = db_.getComplaintById(list[i]->id())) addUnique(std::move(fresh));

    // 3) PostgreSQL ILIKE (parameterised) - also finds CLOSED complaints not kept in memory
    auto dbRows = db_.searchComplaints(text, scopeFor(viewer), 300);
    out.databaseMatches = static_cast<int>(dbRows.size());
    for (auto& c : dbRows)
        if (viewer.canView(*c)) addUnique(std::move(c));
    return out;
}

// ============================================================ rankings
CityRanking ComplaintService::cityRanking(bool descending, int topN, const std::string& cityQuery, const Scope& scope) {
    CityRanking r;
    CityArray arr;
    db_.loadCityStats(arr, scope, true);                       // PostgreSQL -> CityArray
    r.total = arr.totalComplaints();

    // Quick Sort on the raw array (ties broken by name for a deterministic ranking)
    r.sort = quickSort(arr.data(), arr.size(), [descending](const CityStat& a, const CityStat& b) {
        if (a.complaints != b.complaints) return descending ? a.complaints > b.complaints : a.complaints < b.complaints;
        return compareIgnoreCase(a.city, b.city) < 0;
    });
    std::size_t n = topN > 0 ? std::min<std::size_t>(arr.size(), static_cast<std::size_t>(topN)) : arr.size();
    for (std::size_t i = 0; i < n; ++i) r.rows.push_back(arr.at(i));

    // "Search City": Merge Sort a copy by name, then Binary Search it
    std::string q = Validator::trim(cityQuery);
    if (!q.empty()) {
        std::vector<CityStat> byName(arr.data(), arr.data() + arr.size());
        mergeSort(byName, [](const CityStat& a, const CityStat& b) { return compareIgnoreCase(a.city, b.city) < 0; });
        r.binary = binarySearch(byName.data(), byName.size(), q,
                                [&](const CityStat& s) { return compareIgnoreCase(s.city, q); });
        if (r.binary.resultIndex >= 0) {
            int cityId = byName[static_cast<std::size_t>(r.binary.resultIndex)].cityId;
            for (std::size_t i = 0; i < arr.size(); ++i)
                if (arr.at(i).cityId == cityId) r.foundRank = static_cast<long long>(i) + 1;
        }
    }
    return r;
}

DepartmentRanking ComplaintService::departmentRanking() {
    db_.loadDepartmentWorkload(departments_);
    DepartmentRanking r;
    r.rows = departments_;
    r.sort = bubbleSort(r.rows.data(), r.rows.size(), [](const Department& a, const Department& b) {
        return a.workload().open() > b.workload().open();
    });
    return r;
}

} // namespace scms
