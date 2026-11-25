// =====================================================================
// Database.h - PostgreSQL access layer (libpq)
//
// * every statement uses PQexecParams with $1..$n placeholders -> user input is
//   NEVER concatenated into SQL text
// * passwords are verified/hashed inside PostgreSQL with pgcrypto bcrypt
// * connection loss is detected on every call; ensureConnected() attempts PQreset
//   and the caller receives NotConnectedException instead of a false "success"
// =====================================================================
#pragma once
#include "Complaint.h"
#include "Department.h"
#include "User.h"
#include "../dsa/CityArray.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

namespace scms {

struct DbConfig {
    std::string host = "localhost", port = "5433", dbname = "scms", user = "scms_app", password;
};

// one bound parameter ($n). null = SQL NULL
struct Param {
    std::string value;
    bool null = false;
    Param(const std::string& v) : value(v) {}
    Param(const char* v) : value(v ? v : ""), null(v == nullptr) {}
    Param(int v) : value(std::to_string(v)) {}
    Param(long long v) : value(std::to_string(v)) {}
    Param(bool v) : value(v ? "true" : "false") {}
    static Param nullValue() { Param p(""); p.null = true; return p; }
    static Param idOrNull(long long id) { return id > 0 ? Param(id) : nullValue(); }
};

// RAII wrapper around PGresult (destructor calls PQclear)
class PgResult {
public:
    explicit PgResult(PGresult* r) : r_(r) {}
    ~PgResult();
    PgResult(PgResult&& o) noexcept : r_(o.r_) { o.r_ = nullptr; }
    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;
    int rows() const;
    int affected() const;
    std::string str(int row, int col) const;
    int i(int row, int col) const;
    long long ll(int row, int col) const;
    double d(int row, int col) const;
    bool b(int row, int col) const;
    bool isNull(int row, int col) const;

private:
    PGresult* r_;
};

// --- plain rows returned to the UI ------------------------------------------
struct IdName { int id = 0; std::string name; };
struct CategoryRow { int id = 0; std::string code, name; int asmCode = 0, departmentId = 0, slaHours = 0; };
struct CityInfo { int cityId = 0, districtId = 0, stateId = 0; std::string city, district, state; double lat = 0, lon = 0; };
struct UserRow {
    int id = 0; std::string username, fullName, phone, role, department; bool active = true;
    std::string createdAt, lastLogin;
};
struct HistoryRow { std::string when, type, details, actor; long long epoch = 0; };
struct StatusRow { std::string oldStatus, newStatus, note, actor, when; };
struct NotificationRow { long long id = 0, complaintId = 0; std::string type, title, message, when; bool read = false; };
struct AuditRow { long long id = 0; std::string when, user, action, entityType, entityId, details; };
struct CountRow { std::string label; int count = 0; int extra = 0; };
struct RegionStat {
    int id = 0; std::string name; int complaints = 0, pending = 0, resolved = 0, highSeverity = 0, subRegions = 0;
};
struct Summary { int total = 0, pending = 0, assigned = 0, inProgress = 0, resolved = 0, closed = 0, high = 0, overdue = 0; };
struct NewUser { std::string username, password, fullName, phone; Role role = Role::CITIZEN; int departmentId = 0, cityId = 0; };

// restricts every statistics query: 0 = no restriction
struct Scope { int citizenId = 0; int departmentId = 0; };

struct ComplaintFilter {
    Scope scope;
    std::string status;          // "" = any
    std::string categoryCode;
    int stateId = 0, districtId = 0, cityId = 0;
    std::string text;            // ILIKE over every searchable column
    bool activeOnly = false;     // excludes CLOSED
    int limit = 500, offset = 0;
    std::string orderBy = "created";   // created | severity | status
};

class Database {
public:
    explicit Database(DbConfig cfg);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // ---- connection --------------------------------------------------------
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool ensureConnected();           // tries PQreset once
    std::string lastError() const { return lastError_; }
    std::string serverVersion() const;
    std::string connectionInfo() const;
    double pingMs();
    long long queryCount() const { return queries_; }

    // ---- transactions (RAII) ------------------------------------------------
    class Transaction {
    public:
        explicit Transaction(Database& db);
        ~Transaction();                // rolls back unless commit() was called
        void commit();
    private:
        Database& db_;
        bool done_ = false;
    };

    // ---- users -------------------------------------------------------------
    std::unique_ptr<User> authenticateUser(const std::string& username, const std::string& password);
    int createUser(const NewUser& u);                    // DuplicateException on username clash
    std::vector<UserRow> getUsers();
    void setUserActive(int userId, bool active);
    std::vector<int> getUserIdsByRole(Role role);
    std::vector<int> getDepartmentUserIds(int departmentId);

    // ---- reference data ----------------------------------------------------
    std::vector<IdName> getStates();
    std::vector<IdName> getDistricts(int stateId);
    std::vector<IdName> getCities(int districtId);
    CityInfo getCity(int cityId);                        // NotFoundException if invalid
    std::vector<CityInfo> getAllCities();
    std::vector<CategoryRow> getCategories();
    std::vector<Department> getDepartments();

    // ---- complaints --------------------------------------------------------
    long long reserveComplaintId();
    std::string makeReference(long long id);
    bool duplicateExists(int citizenId, int cityId, const std::string& categoryCode, const std::string& description);
    void insertComplaint(const Complaint& c, int categoryId);
    void updateComplaint(const Complaint& c);
    void updateStatus(long long id, Status from, Status to, int actorId, const std::string& note);
    void assignDepartment(long long id, int departmentId, int actorId);
    std::unique_ptr<Complaint> getComplaintById(long long id);
    std::unique_ptr<Complaint> getComplaintByReference(const std::string& ref);
    std::vector<std::unique_ptr<Complaint>> getComplaints(const ComplaintFilter& f);
    std::vector<std::unique_ptr<Complaint>> searchComplaints(const std::string& text, const Scope& scope, int limit = 200);
    std::vector<std::unique_ptr<Complaint>> getComplaintsByCity(int cityId, const Scope& s = {});
    std::vector<std::unique_ptr<Complaint>> getComplaintsByDistrict(int districtId, const Scope& s = {});
    std::vector<std::unique_ptr<Complaint>> getComplaintsByState(int stateId, const Scope& s = {});
    std::vector<std::unique_ptr<Complaint>> getComplaintsByCategory(const std::string& code, const Scope& s = {});
    std::vector<std::unique_ptr<Complaint>> getComplaintsByDepartment(int departmentId);
    int countComplaints(const ComplaintFilter& f);
    std::vector<std::pair<long long, std::string>> pendingQueueOrder();   // id, ref of PENDING by arrival

    // ---- history / notifications / audit -------------------------------------
    void addHistory(long long complaintId, const std::string& type, const std::string& details, int actorId);
    std::vector<HistoryRow> getHistory(long long complaintId);
    std::vector<StatusRow> getStatusHistory(long long complaintId);
    void createNotification(int userId, long long complaintId, const std::string& type,
                            const std::string& title, const std::string& message);
    std::vector<NotificationRow> getNotifications(int userId, int limit = 100);
    int unreadCount(int userId);
    void markNotificationRead(long long id, int userId);
    void markAllNotificationsRead(int userId);
    void createAuditLog(int userId, const std::string& action, const std::string& entityType,
                        const std::string& entityId, const std::string& details);
    std::vector<AuditRow> getAuditLogs(int limit = 200, const std::string& actionFilter = "");

    // ---- statistics (always computed from rows, never stored) ---------------
    Summary getSummary(const Scope& s);
    std::vector<CountRow> countByCategory(const Scope& s);
    std::vector<CountRow> countBySeverity(const Scope& s);
    std::vector<CountRow> monthlyTrend(const Scope& s, int months);       // label=YYYY-MM count=created extra=resolved
    void loadCityStats(CityArray& out, const Scope& s, bool onlyWithComplaints);
    std::vector<RegionStat> stateStats(const Scope& s);                    // all 36 states incl. zero
    std::vector<RegionStat> districtStats(int stateId, const Scope& s);
    void loadDepartmentWorkload(std::vector<Department>& departments);
    void loadStateCategoryGrid(StatGrid& grid, std::vector<std::string>& states,
                               std::vector<std::string>& categories, int topStates);
    double averageResolutionHours(const Scope& s);

    // demo dataset only (scms_cli --seed-demo): shifts every timestamp of one complaint back in time
    void backdateComplaint(long long id, int hours);

    // demo dataset only (scms_cli --demo-status / --clear-demo): everything belonging to the "demo.*" citizen accounts
    struct DemoCounts { int complaints = 0, users = 0, total = 0; };
    struct DemoRemoval { int complaints = 0, auditRows = 0, users = 0; bool usersBlocked = false; std::string blockedReason; };
    DemoCounts demoCounts();
    DemoRemoval clearDemoData(bool alsoUsers);   // one transaction: all or nothing

private:
    PgResult exec(const std::string& sql, const std::vector<Param>& params = {});
    std::unique_ptr<Complaint> rowToComplaint(const PgResult& r, int row);
    static std::string scopeClause(const Scope& s, std::vector<Param>& params, const char* alias = "");

    DbConfig cfg_;
    PGconn* conn_ = nullptr;
    std::string lastError_;
    long long queries_ = 0;
};

} // namespace scms
