// =====================================================================
// ComplaintService.h - application core: owns the runtime DSA structures and
// orchestrates every use case against PostgreSQL (the single source of truth).
//
// Composition: ComplaintService HAS-A DoublyLinkedList, HashTable, ComplaintQueue,
// two Stacks, a CityArray and the Department objects.
// =====================================================================
#pragma once
#include "Database.h"
#include "../dsa/CityArray.h"
#include "../dsa/DoublyLinkedList.h"
#include "../dsa/HashTable.h"
#include "../dsa/Queue.h"
#include "../dsa/Searching.h"
#include "../dsa/Sorting.h"
#include "../dsa/Stack.h"
#include <memory>
#include <string>
#include <vector>

namespace scms {

struct ComplaintInput {
    std::string citizenName, phone, locality, categoryCode, description, priority = "NORMAL";
    int stateId = 0, districtId = 0, cityId = 0;
    int impact = 0, urgency = 0;
};

struct SubmitResult {
    long long id = 0;
    std::string reference;
    int severityScore = 0;
    std::string severityLevel;
    std::size_t queuePosition = 0;
    std::vector<std::string> steps;      // pipeline trace shown in the UI
};

struct ProcessResult {
    std::string reference, category, department;
    std::size_t remaining = 0;
    std::vector<std::string> steps;
};

struct SearchOutcome {
    std::vector<std::unique_ptr<Complaint>> results;
    SearchMetrics linear;                // Linear Search over the DLL (Assembly substring scan)
    bool hashUsed = false, hashHit = false;
    unsigned long long hashProbes = 0;
    int databaseMatches = 0;             // PostgreSQL ILIKE (includes closed complaints)
};

struct CityRanking {
    std::vector<CityStat> rows;          // after Quick Sort
    SortMetrics sort;
    long long total = 0;
    SearchMetrics binary;                // Binary Search for "Search City"
    long long foundRank = -1;
};

struct DepartmentRanking {
    std::vector<Department> rows;        // after Bubble Sort
    SortMetrics sort;
};

class ComplaintService {
public:
    explicit ComplaintService(Database& db);
    ~ComplaintService();

    // ---- session ----------------------------------------------------------------
    std::unique_ptr<User> login(const std::string& username, const std::string& password);
    void logout(const User& u);
    int registerCitizen(const NewUser& u, const std::string& confirmPassword);
    int createStaffUser(const User& admin, const NewUser& u, const std::string& confirmPassword);

    // ---- reference data -----------------------------------------------------------
    void loadReferenceData();
    const std::vector<CategoryRow>& categories() const { return categories_; }
    std::vector<Department>& departments() { return departments_; }
    const CategoryRow* categoryByCode(const std::string& code) const;
    Department* departmentFor(const std::string& categoryCode);

    // ---- runtime structures (reloaded from PostgreSQL on login / refresh) -----------
    void loadRuntime(const User& u);
    DoublyLinkedList& activeList() { return active_; }
    HashTable& index() { return index_; }
    ComplaintQueue& queue() { return queue_; }
    Stack& statusStack() { return statusStack_; }
    Stack& adminActions() { return adminActions_; }
    unsigned long long processedCount() const { return processed_; }
    const std::string& lastProcessed() const { return lastProcessed_; }

    // ---- use cases ----------------------------------------------------------------
    SubmitResult submitComplaint(const User& citizen, const ComplaintInput& in);
    ProcessResult processNextInQueue(const User& actor);
    void changeStatus(const User& actor, long long complaintId, Status to, const std::string& note);
    std::unique_ptr<Complaint> openComplaint(const User& viewer, long long complaintId);   // fills statusStack
    SearchOutcome search(const User& viewer, const std::string& text);
    CityRanking cityRanking(bool descending, int topN, const std::string& cityQuery, const Scope& scope);
    DepartmentRanking departmentRanking();
    Scope scopeFor(const User& u) const;

private:
    void indexComplaint(Complaint* c);
    void unindexComplaint(const Complaint& c);
    void notifyAll(const std::vector<int>& users, long long complaintId, const std::string& type,
                   const std::string& title, const std::string& message);

    Database& db_;
    std::vector<CategoryRow> categories_;
    std::vector<Department> departments_;
    DoublyLinkedList active_;
    HashTable index_;
    ComplaintQueue queue_;
    Stack statusStack_;
    Stack adminActions_;
    unsigned long long processed_ = 0;
    std::string lastProcessed_;
};

} // namespace scms
