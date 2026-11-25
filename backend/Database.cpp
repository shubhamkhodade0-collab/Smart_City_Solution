#include "Database.h"
#include "Exceptions.h"
#include <libpq-fe.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace scms {

// --------------------------------------------------------------------------- PgResult
PgResult::~PgResult() { if (r_) PQclear(r_); }
int PgResult::rows() const { return r_ ? PQntuples(r_) : 0; }
int PgResult::affected() const { return r_ ? std::atoi(PQcmdTuples(r_)) : 0; }
std::string PgResult::str(int row, int col) const { return PQgetvalue(r_, row, col); }
int PgResult::i(int row, int col) const { return isNull(row, col) ? 0 : std::atoi(PQgetvalue(r_, row, col)); }
long long PgResult::ll(int row, int col) const { return isNull(row, col) ? 0 : std::atoll(PQgetvalue(r_, row, col)); }
double PgResult::d(int row, int col) const { return isNull(row, col) ? 0.0 : std::atof(PQgetvalue(r_, row, col)); }
bool PgResult::b(int row, int col) const { return PQgetvalue(r_, row, col)[0] == 't'; }
bool PgResult::isNull(int row, int col) const { return PQgetisnull(r_, row, col) != 0; }

// --------------------------------------------------------------------------- connection
Database::Database(DbConfig cfg) : cfg_(std::move(cfg)) {}
Database::~Database() { disconnect(); }

bool Database::connect() {
    disconnect();
    const char* keys[] = {"host", "port", "dbname", "user", "password", "connect_timeout", "application_name", nullptr};
    const char* vals[] = {cfg_.host.c_str(), cfg_.port.c_str(), cfg_.dbname.c_str(), cfg_.user.c_str(),
                          cfg_.password.c_str(), "3", "SmartCityComplaintSystem", nullptr};
    conn_ = PQconnectdbParams(keys, vals, 0);
    if (PQstatus(conn_) != CONNECTION_OK) {
        lastError_ = PQerrorMessage(conn_);
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }
    lastError_.clear();
    return true;
}

void Database::disconnect() {
    if (conn_) PQfinish(conn_);
    conn_ = nullptr;
}

bool Database::isConnected() const { return conn_ && PQstatus(conn_) == CONNECTION_OK; }

bool Database::ensureConnected() {
    if (isConnected()) return true;
    if (conn_) {
        PQreset(conn_);                       // graceful recovery after a server restart
        if (PQstatus(conn_) == CONNECTION_OK) return true;
        lastError_ = PQerrorMessage(conn_);
    }
    return connect();
}

std::string Database::serverVersion() const {
    if (!isConnected()) return "not connected";
    int v = PQserverVersion(conn_);
    return std::to_string(v / 10000) + "." + std::to_string(v % 100);
}

std::string Database::connectionInfo() const {
    return cfg_.user + "@" + cfg_.host + ":" + cfg_.port + "/" + cfg_.dbname;
}

double Database::pingMs() {
    auto t0 = std::chrono::steady_clock::now();
    try {
        exec("SELECT 1");
    } catch (...) {
        return -1.0;
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

PgResult Database::exec(const std::string& sql, const std::vector<Param>& params) {
    if (!ensureConnected()) throw NotConnectedException();
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& p : params) values.push_back(p.null ? nullptr : p.value.c_str());
    ++queries_;
    PGresult* r = PQexecParams(conn_, sql.c_str(), static_cast<int>(params.size()), nullptr,
                               values.empty() ? nullptr : values.data(), nullptr, nullptr, 0);
    PgResult res(r);
    ExecStatusType st = PQresultStatus(r);
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        std::string msg = PQresultErrorMessage(r);
        const char* state = PQresultErrorField(r, PG_DIAG_SQLSTATE);
        lastError_ = msg;
        if (PQstatus(conn_) != CONNECTION_OK) throw NotConnectedException();
        if (state && std::string(state) == "23505") throw DuplicateException("Duplicate value: " + msg);
        if (state && std::string(state).rfind("23", 0) == 0)
            throw ValidationException("database", "Rejected by database constraint: " + msg);
        throw DatabaseException("Database error: " + msg);
    }
    return res;
}

Database::Transaction::Transaction(Database& db) : db_(db) { db_.exec("BEGIN"); }
Database::Transaction::~Transaction() {
    if (!done_) {
        try { db_.exec("ROLLBACK"); } catch (...) {}
    }
}
void Database::Transaction::commit() {
    db_.exec("COMMIT");
    done_ = true;
}

std::string Database::scopeClause(const Scope& s, std::vector<Param>& params, const char* alias) {
    std::string out;
    std::string a = alias;
    if (s.citizenId > 0) {
        params.emplace_back(s.citizenId);
        out += " AND " + a + "citizen_id = $" + std::to_string(params.size());
    }
    if (s.departmentId > 0) {
        params.emplace_back(s.departmentId);
        out += " AND " + a + "department_id = $" + std::to_string(params.size());
    }
    return out;
}

// --------------------------------------------------------------------------- users
std::unique_ptr<User> Database::authenticateUser(const std::string& username, const std::string& password) {
    // bcrypt comparison happens inside PostgreSQL: crypt(input, stored_hash) == stored_hash
    auto r = exec("SELECT u.id, u.username, u.full_name, u.phone, u.role, COALESCE(u.department_id,0), "
                  "COALESCE(d.name,''), COALESCE(u.city_id,0) FROM users u LEFT JOIN departments d ON d.id = u.department_id "
                  "WHERE u.username = $1 AND u.is_active AND u.password_hash = crypt($2, u.password_hash)",
                  {username, password});
    if (r.rows() != 1) return nullptr;
    auto user = User::create(roleFromString(r.str(0, 4)), r.i(0, 0), r.str(0, 1), r.str(0, 2), r.str(0, 3),
                             r.i(0, 5), r.str(0, 6));
    user->setCityId(r.i(0, 7));
    exec("UPDATE users SET last_login_at = NOW() WHERE id = $1", {user->id()});
    return user;
}

int Database::createUser(const NewUser& u) {
    auto r = exec("INSERT INTO users (username, password_hash, full_name, phone, role, department_id, city_id) "
                  "VALUES ($1, crypt($2, gen_salt('bf', 8)), $3, $4, $5, $6, $7) RETURNING id",
                  {u.username, u.password, u.fullName, u.phone, std::string(toString(u.role)),
                   Param::idOrNull(u.departmentId), Param::idOrNull(u.cityId)});
    return r.i(0, 0);
}

std::vector<UserRow> Database::getUsers() {
    auto r = exec("SELECT u.id, u.username, u.full_name, u.phone, u.role, COALESCE(d.name,''), u.is_active, "
                  "TO_CHAR(u.created_at,'DD Mon YYYY'), COALESCE(TO_CHAR(u.last_login_at,'DD Mon YYYY HH24:MI'),'never') "
                  "FROM users u LEFT JOIN departments d ON d.id = u.department_id ORDER BY u.role, u.username");
    std::vector<UserRow> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.i(i, 0), r.str(i, 1), r.str(i, 2), r.str(i, 3), r.str(i, 4), r.str(i, 5), r.b(i, 6),
                     r.str(i, 7), r.str(i, 8)});
    return v;
}

void Database::setUserActive(int userId, bool active) {
    exec("UPDATE users SET is_active = $2 WHERE id = $1", {userId, active});
}

std::vector<int> Database::getUserIdsByRole(Role role) {
    auto r = exec("SELECT id FROM users WHERE role = $1 AND is_active", {std::string(toString(role))});
    std::vector<int> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back(r.i(i, 0));
    return v;
}

std::vector<int> Database::getDepartmentUserIds(int departmentId) {
    auto r = exec("SELECT id FROM users WHERE role = 'DEPARTMENT' AND department_id = $1 AND is_active", {departmentId});
    std::vector<int> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back(r.i(i, 0));
    return v;
}

// --------------------------------------------------------------------------- reference data
std::vector<IdName> Database::getStates() {
    auto r = exec("SELECT id, name FROM states ORDER BY name");
    std::vector<IdName> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.i(i, 0), r.str(i, 1)});
    return v;
}

std::vector<IdName> Database::getDistricts(int stateId) {
    auto r = exec("SELECT id, name FROM districts WHERE state_id = $1 ORDER BY name", {stateId});
    std::vector<IdName> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.i(i, 0), r.str(i, 1)});
    return v;
}

std::vector<IdName> Database::getCities(int districtId) {
    auto r = exec("SELECT id, name FROM cities WHERE district_id = $1 ORDER BY name", {districtId});
    std::vector<IdName> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.i(i, 0), r.str(i, 1)});
    return v;
}

static const char* CITY_SQL =
    "SELECT ci.id, d.id, s.id, ci.name, d.name, s.name, ci.latitude, ci.longitude FROM cities ci "
    "JOIN districts d ON d.id = ci.district_id JOIN states s ON s.id = d.state_id ";

CityInfo Database::getCity(int cityId) {
    auto r = exec(std::string(CITY_SQL) + "WHERE ci.id = $1", {cityId});
    if (r.rows() != 1) throw NotFoundException("Invalid city (id " + std::to_string(cityId) + ")");
    return {r.i(0, 0), r.i(0, 1), r.i(0, 2), r.str(0, 3), r.str(0, 4), r.str(0, 5), r.d(0, 6), r.d(0, 7)};
}

std::vector<CityInfo> Database::getAllCities() {
    auto r = exec(std::string(CITY_SQL) + "ORDER BY s.name, ci.name");
    std::vector<CityInfo> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.i(i, 0), r.i(i, 1), r.i(i, 2), r.str(i, 3), r.str(i, 4), r.str(i, 5), r.d(i, 6), r.d(i, 7)});
    return v;
}

std::vector<CategoryRow> Database::getCategories() {
    auto r = exec("SELECT id, code, name, asm_code, department_id, sla_hours FROM categories ORDER BY asm_code");
    std::vector<CategoryRow> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.i(i, 0), r.str(i, 1), r.str(i, 2), r.i(i, 3), r.i(i, 4), r.i(i, 5)});
    return v;
}

std::vector<Department> Database::getDepartments() {
    auto r = exec("SELECT d.id, d.code, d.name, d.description, COALESCE(string_agg(c.code, ',' ORDER BY c.asm_code),'') "
                  "FROM departments d LEFT JOIN categories c ON c.department_id = d.id GROUP BY d.id ORDER BY d.id");
    std::vector<Department> v;
    for (int i = 0; i < r.rows(); ++i) {
        Department dep(r.i(i, 0), r.str(i, 1), r.str(i, 2), r.str(i, 3));
        std::string cats = r.str(i, 4);
        std::size_t start = 0;
        while (start < cats.size()) {
            std::size_t comma = cats.find(',', start);
            if (comma == std::string::npos) comma = cats.size();
            dep.addCategory(cats.substr(start, comma - start));
            start = comma + 1;
        }
        v.push_back(std::move(dep));
    }
    return v;
}

// --------------------------------------------------------------------------- complaints
long long Database::reserveComplaintId() {
    return exec("SELECT nextval('complaints_id_seq')").ll(0, 0);
}

std::string Database::makeReference(long long id) {
    auto r = exec("SELECT 'SCMS-' || TO_CHAR(NOW(),'YYYY') || '-' || LPAD($1::text, 6, '0')", {id});
    return r.str(0, 0);
}

bool Database::duplicateExists(int citizenId, int cityId, const std::string& categoryCode, const std::string& desc) {
    auto r = exec("SELECT 1 FROM complaints c JOIN categories cat ON cat.id = c.category_id "
                  "WHERE c.citizen_id = $1 AND c.city_id = $2 AND cat.code = $3 "
                  "AND LOWER(TRIM(c.description)) = LOWER(TRIM($4)) AND c.status <> 'CLOSED' "
                  "AND c.created_at > NOW() - INTERVAL '7 days' LIMIT 1",
                  {citizenId, cityId, categoryCode, desc});
    return r.rows() > 0;
}

void Database::insertComplaint(const Complaint& c, int categoryId) {
    exec("INSERT INTO complaints (id, reference_code, citizen_id, citizen_name, phone, city_id, locality, category_id, "
         "description, impact, urgency, severity_score, severity_level, priority, status, department_id, created_at, updated_at) "
         "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16, TO_TIMESTAMP($17), TO_TIMESTAMP($17))",
         {c.id(), c.referenceCode(), c.citizenId(), c.citizenName(), c.phone(), c.location().cityId,
          c.location().locality, categoryId, c.description(), c.impact(), c.urgency(), c.severityScore(),
          std::string(toString(c.severityLevel())), std::string(toString(c.priority())),
          std::string(toString(c.status())), Param::idOrNull(c.departmentId()),
          static_cast<long long>(c.createdAt())});
    exec("INSERT INTO complaint_status_history (complaint_id, old_status, new_status, changed_by, note, changed_at) "
         "VALUES ($1, NULL, 'PENDING', $2, 'Complaint registered', TO_TIMESTAMP($3))",
         {c.id(), c.citizenId(), static_cast<long long>(c.createdAt())});
}

void Database::updateComplaint(const Complaint& c) {
    auto r = exec("UPDATE complaints SET locality=$2, description=$3, priority=$4, updated_at=NOW() WHERE id=$1",
                  {c.id(), c.location().locality, c.description(), std::string(toString(c.priority()))});
    if (r.affected() != 1) throw NotFoundException("Complaint " + c.referenceCode() + " no longer exists");
}

void Database::updateStatus(long long id, Status from, Status to, int actorId, const std::string& note) {
    // optimistic concurrency: only succeeds if nobody changed the status in between
    auto r = exec("UPDATE complaints SET status = $3::varchar, updated_at = NOW(), "
                  "resolved_at = CASE WHEN $3::varchar = 'RESOLVED' THEN NOW() ELSE resolved_at END, "
                  "closed_at = CASE WHEN $3::varchar = 'CLOSED' THEN NOW() ELSE closed_at END "
                  "WHERE id = $1 AND status = $2",
                  {id, std::string(toString(from)), std::string(toString(to))});
    if (r.affected() != 1)
        throw InvalidTransitionException("The complaint was changed by someone else - refresh and try again.");
    exec("INSERT INTO complaint_status_history (complaint_id, old_status, new_status, changed_by, note) "
         "VALUES ($1,$2,$3,$4,$5)",
         {id, std::string(toString(from)), std::string(toString(to)), Param::idOrNull(actorId), note});
}

void Database::assignDepartment(long long id, int departmentId, int actorId) {
    auto r = exec("UPDATE complaints SET department_id = $2, status = 'ASSIGNED', updated_at = NOW() "
                  "WHERE id = $1 AND status = 'PENDING'", {id, departmentId});
    if (r.affected() != 1) throw InvalidTransitionException("Only PENDING complaints can be assigned.");
    exec("INSERT INTO complaint_status_history (complaint_id, old_status, new_status, changed_by, note) "
         "VALUES ($1,'PENDING','ASSIGNED',$2,'Routed by FIFO processor')", {id, Param::idOrNull(actorId)});
}

static const char* COMPLAINT_COLS =
    "SELECT id, reference_code, citizen_id, citizen_name, phone, city_id, city_name, district_id, district_name, "
    "state_id, state_name, locality, category_code, description, impact, urgency, severity_score, severity_level, "
    "priority, status, COALESCE(department_id,0), COALESCE(department_name,''), "
    "EXTRACT(EPOCH FROM created_at)::bigint, EXTRACT(EPOCH FROM updated_at)::bigint FROM v_complaints ";

std::unique_ptr<Complaint> Database::rowToComplaint(const PgResult& r, int i) {
    Location loc;
    loc.cityId = r.i(i, 5); loc.city = r.str(i, 6);
    loc.districtId = r.i(i, 7); loc.district = r.str(i, 8);
    loc.stateId = r.i(i, 9); loc.state = r.str(i, 10);
    loc.locality = r.str(i, 11);
    auto c = Complaint::create(r.str(i, 12), r.str(i, 3), r.str(i, 4), loc, r.str(i, 13), r.i(i, 14), r.i(i, 15),
                               priorityFromString(r.str(i, 18)));
    // friend access: DB-generated / persisted fields are restored directly
    c->id_ = r.ll(i, 0);
    c->reference_ = r.str(i, 1);
    c->citizenId_ = r.i(i, 2);
    c->severityScore_ = r.i(i, 16);
    c->severityLevel_ = severityFromString(r.str(i, 17));
    c->status_ = statusFromString(r.str(i, 19));
    c->departmentId_ = r.i(i, 20);
    c->departmentName_ = r.str(i, 21);
    c->createdAt_ = static_cast<std::time_t>(r.ll(i, 22));
    c->updatedAt_ = static_cast<std::time_t>(r.ll(i, 23));
    return c;
}

std::unique_ptr<Complaint> Database::getComplaintById(long long id) {
    auto r = exec(std::string(COMPLAINT_COLS) + "WHERE id = $1", {id});
    if (r.rows() != 1) return nullptr;
    return rowToComplaint(r, 0);
}

std::unique_ptr<Complaint> Database::getComplaintByReference(const std::string& ref) {
    auto r = exec(std::string(COMPLAINT_COLS) + "WHERE UPPER(reference_code) = UPPER($1)", {ref});
    if (r.rows() != 1) return nullptr;
    return rowToComplaint(r, 0);
}

static std::string buildWhere(const ComplaintFilter& f, std::vector<Param>& p) {
    std::string w = "WHERE TRUE";
    auto add = [&](const std::string& col, Param v) {
        p.push_back(std::move(v));
        w += " AND " + col + " = $" + std::to_string(p.size());
    };
    if (f.scope.citizenId > 0) add("citizen_id", f.scope.citizenId);
    if (f.scope.departmentId > 0) add("department_id", f.scope.departmentId);
    if (!f.status.empty()) add("status", f.status);
    if (!f.categoryCode.empty()) add("category_code", f.categoryCode);
    if (f.stateId > 0) add("state_id", f.stateId);
    if (f.districtId > 0) add("district_id", f.districtId);
    if (f.cityId > 0) add("city_id", f.cityId);
    if (f.activeOnly) w += " AND status <> 'CLOSED'";
    if (!f.text.empty()) {
        p.emplace_back("%" + f.text + "%");
        std::string n = "$" + std::to_string(p.size());
        w += " AND (reference_code ILIKE " + n + " OR id::text ILIKE " + n + " OR citizen_name ILIKE " + n +
             " OR city_name ILIKE " + n + " OR district_name ILIKE " + n + " OR state_name ILIKE " + n +
             " OR locality ILIKE " + n + " OR category_name ILIKE " + n + " OR status ILIKE " + n +
             " OR COALESCE(department_name,'') ILIKE " + n + " OR description ILIKE " + n +
             " OR severity_level ILIKE " + n + " OR priority ILIKE " + n + ")";
    }
    return w;
}

std::vector<std::unique_ptr<Complaint>> Database::getComplaints(const ComplaintFilter& f) {
    std::vector<Param> p;
    std::string sql = COMPLAINT_COLS + buildWhere(f, p);
    if (f.orderBy == "severity") sql += " ORDER BY severity_score DESC, created_at DESC";
    else if (f.orderBy == "oldest") sql += " ORDER BY created_at ASC";
    else sql += " ORDER BY created_at DESC";
    p.emplace_back(f.limit);
    sql += " LIMIT $" + std::to_string(p.size());
    p.emplace_back(f.offset);
    sql += " OFFSET $" + std::to_string(p.size());
    auto r = exec(sql, p);
    std::vector<std::unique_ptr<Complaint>> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back(rowToComplaint(r, i));
    return v;
}

int Database::countComplaints(const ComplaintFilter& f) {
    std::vector<Param> p;
    auto r = exec("SELECT COUNT(*) FROM v_complaints " + buildWhere(f, p), p);
    return r.i(0, 0);
}

std::vector<std::unique_ptr<Complaint>> Database::searchComplaints(const std::string& text, const Scope& scope, int limit) {
    ComplaintFilter f;
    f.scope = scope;
    f.text = text;
    f.limit = limit;
    return getComplaints(f);
}

std::vector<std::unique_ptr<Complaint>> Database::getComplaintsByCity(int cityId, const Scope& s) {
    ComplaintFilter f; f.scope = s; f.cityId = cityId; return getComplaints(f);
}
std::vector<std::unique_ptr<Complaint>> Database::getComplaintsByDistrict(int districtId, const Scope& s) {
    ComplaintFilter f; f.scope = s; f.districtId = districtId; return getComplaints(f);
}
std::vector<std::unique_ptr<Complaint>> Database::getComplaintsByState(int stateId, const Scope& s) {
    ComplaintFilter f; f.scope = s; f.stateId = stateId; return getComplaints(f);
}
std::vector<std::unique_ptr<Complaint>> Database::getComplaintsByCategory(const std::string& code, const Scope& s) {
    ComplaintFilter f; f.scope = s; f.categoryCode = code; return getComplaints(f);
}
std::vector<std::unique_ptr<Complaint>> Database::getComplaintsByDepartment(int departmentId) {
    ComplaintFilter f; f.scope.departmentId = departmentId; return getComplaints(f);
}

std::vector<std::pair<long long, std::string>> Database::pendingQueueOrder() {
    auto r = exec("SELECT id, reference_code FROM complaints WHERE status = 'PENDING' ORDER BY created_at, id");
    std::vector<std::pair<long long, std::string>> v;
    for (int i = 0; i < r.rows(); ++i) v.emplace_back(r.ll(i, 0), r.str(i, 1));
    return v;
}

// --------------------------------------------------------------------------- history / notifications / audit
void Database::addHistory(long long complaintId, const std::string& type, const std::string& details, int actorId) {
    exec("INSERT INTO complaint_history (complaint_id, event_type, details, actor_id) VALUES ($1,$2,$3,$4)",
         {complaintId, type, details, Param::idOrNull(actorId)});
}

std::vector<HistoryRow> Database::getHistory(long long complaintId) {
    auto r = exec("SELECT TO_CHAR(h.created_at,'DD Mon YYYY, HH24:MI'), h.event_type, h.details, COALESCE(u.full_name,'System'), "
                  "EXTRACT(EPOCH FROM h.created_at)::bigint FROM complaint_history h LEFT JOIN users u ON u.id = h.actor_id "
                  "WHERE h.complaint_id = $1 ORDER BY h.created_at, h.id", {complaintId});
    std::vector<HistoryRow> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.str(i, 0), r.str(i, 1), r.str(i, 2), r.str(i, 3), r.ll(i, 4)});
    return v;
}

std::vector<StatusRow> Database::getStatusHistory(long long complaintId) {
    auto r = exec("SELECT COALESCE(h.old_status,''), h.new_status, h.note, COALESCE(u.full_name,'System'), "
                  "TO_CHAR(h.changed_at,'DD Mon YYYY, HH24:MI') FROM complaint_status_history h "
                  "LEFT JOIN users u ON u.id = h.changed_by WHERE h.complaint_id = $1 ORDER BY h.changed_at, h.id",
                  {complaintId});
    std::vector<StatusRow> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.str(i, 0), r.str(i, 1), r.str(i, 2), r.str(i, 3), r.str(i, 4)});
    return v;
}

void Database::createNotification(int userId, long long complaintId, const std::string& type,
                                  const std::string& title, const std::string& message) {
    exec("INSERT INTO notifications (user_id, complaint_id, type, title, message) VALUES ($1,$2,$3,$4,$5)",
         {userId, Param::idOrNull(complaintId), type, title, message});
}

std::vector<NotificationRow> Database::getNotifications(int userId, int limit) {
    auto r = exec("SELECT id, COALESCE(complaint_id,0), type, title, message, is_read, "
                  "CASE WHEN created_at > NOW() - INTERVAL '1 day' THEN TO_CHAR(created_at,'HH24:MI') "
                  "ELSE TO_CHAR(created_at,'DD Mon') END FROM notifications WHERE user_id = $1 "
                  "ORDER BY created_at DESC, id DESC LIMIT $2", {userId, limit});
    std::vector<NotificationRow> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.ll(i, 0), r.ll(i, 1), r.str(i, 2), r.str(i, 3), r.str(i, 4), r.str(i, 6), r.b(i, 5)});
    return v;
}

int Database::unreadCount(int userId) {
    return exec("SELECT COUNT(*) FROM notifications WHERE user_id = $1 AND NOT is_read", {userId}).i(0, 0);
}

void Database::markNotificationRead(long long id, int userId) {
    exec("UPDATE notifications SET is_read = TRUE WHERE id = $1 AND user_id = $2", {id, userId});
}

void Database::markAllNotificationsRead(int userId) {
    exec("UPDATE notifications SET is_read = TRUE WHERE user_id = $1 AND NOT is_read", {userId});
}

void Database::createAuditLog(int userId, const std::string& action, const std::string& entityType,
                              const std::string& entityId, const std::string& details) {
    exec("INSERT INTO audit_logs (user_id, action, entity_type, entity_id, details) VALUES ($1,$2,$3,$4,$5)",
         {Param::idOrNull(userId), action, entityType, entityId, details});
}

std::vector<AuditRow> Database::getAuditLogs(int limit, const std::string& actionFilter) {
    auto r = exec("SELECT a.id, TO_CHAR(a.created_at,'DD Mon YYYY HH24:MI:SS'), COALESCE(u.username,'-'), a.action, "
                  "a.entity_type, a.entity_id, a.details FROM audit_logs a LEFT JOIN users u ON u.id = a.user_id "
                  "WHERE ($2::text = '' OR a.action = $2::text) ORDER BY a.created_at DESC, a.id DESC LIMIT $1",
                  {limit, actionFilter});
    std::vector<AuditRow> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.ll(i, 0), r.str(i, 1), r.str(i, 2), r.str(i, 3), r.str(i, 4), r.str(i, 5), r.str(i, 6)});
    return v;
}

// --------------------------------------------------------------------------- statistics
Summary Database::getSummary(const Scope& s) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT COUNT(*), COUNT(*) FILTER (WHERE status='PENDING'), COUNT(*) FILTER (WHERE status='ASSIGNED'), "
                  "COUNT(*) FILTER (WHERE status='IN_PROGRESS'), COUNT(*) FILTER (WHERE status='RESOLVED'), "
                  "COUNT(*) FILTER (WHERE status='CLOSED'), COUNT(*) FILTER (WHERE severity_level IN ('HIGH','CRITICAL')), "
                  "COUNT(*) FILTER (WHERE status IN ('PENDING','ASSIGNED','IN_PROGRESS') AND "
                  "created_at < NOW() - make_interval(hours => cat.sla_hours)) "
                  "FROM complaints c JOIN categories cat ON cat.id = c.category_id WHERE TRUE" + w, p);
    return {r.i(0, 0), r.i(0, 1), r.i(0, 2), r.i(0, 3), r.i(0, 4), r.i(0, 5), r.i(0, 6), r.i(0, 7)};
}

std::vector<CountRow> Database::countByCategory(const Scope& s) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT cat.name, COUNT(c.id), COUNT(c.id) FILTER (WHERE c.status IN ('RESOLVED','CLOSED')) "
                  "FROM categories cat LEFT JOIN complaints c ON c.category_id = cat.id" + w +
                  " GROUP BY cat.name, cat.asm_code ORDER BY cat.asm_code", p);
    std::vector<CountRow> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.str(i, 0), r.i(i, 1), r.i(i, 2)});
    return v;
}

std::vector<CountRow> Database::countBySeverity(const Scope& s) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT lv.l, COUNT(c.id) FROM (VALUES ('LOW',1),('MEDIUM',2),('HIGH',3),('CRITICAL',4)) AS lv(l,o) "
                  "LEFT JOIN complaints c ON c.severity_level = lv.l" + w + " GROUP BY lv.l, lv.o ORDER BY lv.o", p);
    std::vector<CountRow> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.str(i, 0), r.i(i, 1), 0});
    return v;
}

std::vector<CountRow> Database::monthlyTrend(const Scope& s, int months) {
    std::vector<Param> p{months - 1};
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT TO_CHAR(m.m,'Mon YY'), COUNT(c.id), COUNT(c.id) FILTER (WHERE c.status IN ('RESOLVED','CLOSED')) "
                  "FROM generate_series(date_trunc('month', NOW()) - make_interval(months => $1), date_trunc('month', NOW()), "
                  "INTERVAL '1 month') AS m(m) LEFT JOIN complaints c ON date_trunc('month', c.created_at) = m.m" + w +
                  " GROUP BY m.m ORDER BY m.m", p);
    std::vector<CountRow> v;
    for (int i = 0; i < r.rows(); ++i) v.push_back({r.str(i, 0), r.i(i, 1), r.i(i, 2)});
    return v;
}

void Database::loadCityStats(CityArray& out, const Scope& s, bool onlyWithComplaints) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT ci.id, d.id, st.id, ci.name, d.name, st.name, COUNT(c.id), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('PENDING','ASSIGNED','IN_PROGRESS')), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('RESOLVED','CLOSED')), "
                  "COUNT(c.id) FILTER (WHERE c.severity_level IN ('HIGH','CRITICAL')) "
                  "FROM cities ci JOIN districts d ON d.id = ci.district_id JOIN states st ON st.id = d.state_id "
                  "LEFT JOIN complaints c ON c.city_id = ci.id" + w + " GROUP BY ci.id, d.id, st.id" +
                  (onlyWithComplaints ? " HAVING COUNT(c.id) > 0" : ""), p);
    out.clear();
    for (int i = 0; i < r.rows(); ++i) {
        CityStat cs;
        cs.cityId = r.i(i, 0); cs.districtId = r.i(i, 1); cs.stateId = r.i(i, 2);
        cs.city = r.str(i, 3); cs.district = r.str(i, 4); cs.state = r.str(i, 5);
        cs.complaints = r.i(i, 6); cs.pending = r.i(i, 7); cs.resolved = r.i(i, 8); cs.highSeverity = r.i(i, 9);
        out.insert(cs);
    }
}

std::vector<RegionStat> Database::stateStats(const Scope& s) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT st.id, st.name, COUNT(c.id), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('PENDING','ASSIGNED','IN_PROGRESS')), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('RESOLVED','CLOSED')), "
                  "COUNT(c.id) FILTER (WHERE c.severity_level IN ('HIGH','CRITICAL')), "
                  "(SELECT COUNT(*) FROM districts dd WHERE dd.state_id = st.id) "
                  "FROM states st LEFT JOIN districts d ON d.state_id = st.id LEFT JOIN cities ci ON ci.district_id = d.id "
                  "LEFT JOIN complaints c ON c.city_id = ci.id" + w + " GROUP BY st.id ORDER BY st.name", p);
    std::vector<RegionStat> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.i(i, 0), r.str(i, 1), r.i(i, 2), r.i(i, 3), r.i(i, 4), r.i(i, 5), r.i(i, 6)});
    return v;
}

std::vector<RegionStat> Database::districtStats(int stateId, const Scope& s) {
    std::vector<Param> p{stateId};
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT d.id, d.name, COUNT(c.id), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('PENDING','ASSIGNED','IN_PROGRESS')), "
                  "COUNT(c.id) FILTER (WHERE c.status IN ('RESOLVED','CLOSED')), "
                  "COUNT(c.id) FILTER (WHERE c.severity_level IN ('HIGH','CRITICAL')), COUNT(DISTINCT ci.id) "
                  "FROM districts d LEFT JOIN cities ci ON ci.district_id = d.id LEFT JOIN complaints c ON c.city_id = ci.id" + w +
                  " WHERE d.state_id = $1 GROUP BY d.id ORDER BY d.name", p);
    std::vector<RegionStat> v;
    for (int i = 0; i < r.rows(); ++i)
        v.push_back({r.i(i, 0), r.str(i, 1), r.i(i, 2), r.i(i, 3), r.i(i, 4), r.i(i, 5), r.i(i, 6)});
    return v;
}

void Database::loadDepartmentWorkload(std::vector<Department>& departments) {
    auto r = exec("SELECT department_id, COUNT(*) FILTER (WHERE status='ASSIGNED'), COUNT(*) FILTER (WHERE status='IN_PROGRESS'), "
                  "COUNT(*) FILTER (WHERE status='RESOLVED'), COUNT(*) FILTER (WHERE status='CLOSED'), "
                  "COUNT(*) FILTER (WHERE severity_level IN ('HIGH','CRITICAL') AND status IN ('ASSIGNED','IN_PROGRESS')) "
                  "FROM complaints WHERE department_id IS NOT NULL GROUP BY department_id");
    for (auto& d : departments) d.workload() = Workload{};
    for (int i = 0; i < r.rows(); ++i)
        for (auto& d : departments)
            if (d.id() == r.i(i, 0)) d.workload() = Workload{r.i(i, 1), r.i(i, 2), r.i(i, 3), r.i(i, 4), r.i(i, 5)};
}

void Database::loadStateCategoryGrid(StatGrid& grid, std::vector<std::string>& states,
                                     std::vector<std::string>& categories, int topStates) {
    grid.clear();
    states.clear();
    categories.clear();
    auto cats = exec("SELECT name FROM categories ORDER BY asm_code");
    for (int i = 0; i < cats.rows(); ++i) categories.push_back(cats.str(i, 0));
    auto top = exec("SELECT st.id, st.name FROM complaints c JOIN cities ci ON ci.id = c.city_id "
                    "JOIN districts d ON d.id = ci.district_id JOIN states st ON st.id = d.state_id "
                    "GROUP BY st.id ORDER BY COUNT(*) DESC, st.name LIMIT $1", {topStates});
    for (int i = 0; i < top.rows(); ++i) {
        states.push_back(top.str(i, 1));
        auto cells = exec("SELECT cat.asm_code, COUNT(c.id) FROM categories cat LEFT JOIN complaints c ON c.category_id = cat.id "
                          "AND c.city_id IN (SELECT ci.id FROM cities ci JOIN districts d ON d.id = ci.district_id WHERE d.state_id = $1) "
                          "GROUP BY cat.asm_code", {top.i(i, 0)});
        for (int k = 0; k < cells.rows(); ++k) grid.set(i, cells.i(k, 0), cells.i(k, 1));
    }
}

double Database::averageResolutionHours(const Scope& s) {
    std::vector<Param> p;
    std::string w = scopeClause(s, p, "c.");
    auto r = exec("SELECT COALESCE(AVG(EXTRACT(EPOCH FROM (resolved_at - created_at)) / 3600.0), 0) FROM complaints c "
                  "WHERE resolved_at IS NOT NULL" + w, p);
    return r.d(0, 0);
}

Database::DemoCounts Database::demoCounts() {
    DemoCounts c;
    PgResult r = exec("SELECT (SELECT COUNT(*) FROM complaints WHERE citizen_id IN (SELECT id FROM users WHERE username LIKE 'demo.%')), "
                      "(SELECT COUNT(*) FROM users WHERE username LIKE 'demo.%'), (SELECT COUNT(*) FROM complaints)");
    if (r.rows() == 1) { c.complaints = r.i(0, 0); c.users = r.i(0, 1); c.total = r.i(0, 2); }
    return c;
}

// Deletes the complaints of the demo accounts. History, status history, notifications and attachments disappear with them
// (ON DELETE CASCADE); the audit entries about those complaints are removed explicitly. Real data is never touched:
// the only selector is "owned by a user whose name starts with demo.".
Database::DemoRemoval Database::clearDemoData(bool alsoUsers) {
    DemoRemoval out;
    static const char* demoUsers = "(SELECT id FROM users WHERE username LIKE 'demo.%')";
    {   // step 1: the complaints - one transaction, all or nothing
        Transaction tx(*this);
        out.auditRows = exec(std::string("DELETE FROM audit_logs WHERE entity_type = 'COMPLAINT' AND entity_id IN "
                                         "(SELECT reference_code FROM complaints WHERE citizen_id IN ") + demoUsers + ")").affected();
        out.complaints = exec(std::string("DELETE FROM complaints WHERE citizen_id IN ") + demoUsers).affected();
        tx.commit();
    }
    if (alsoUsers) {   // step 2: the accounts - separate transaction; refused (and rolled back) if something real still points at them
        try {
            Transaction tx(*this);
            out.auditRows += exec(std::string("DELETE FROM audit_logs WHERE user_id IN ") + demoUsers).affected();
            out.users = exec("DELETE FROM users WHERE username LIKE 'demo.%'").affected();
            tx.commit();
        } catch (const AppException& e) {
            out.users = 0;
            out.usersBlocked = true;
            out.blockedReason = e.what();
        }
    }
    return out;
}

void Database::backdateComplaint(long long id, int hours) {
    Transaction tx(*this);
    exec("UPDATE complaints SET created_at = created_at - make_interval(hours => $2), "
         "updated_at = updated_at - make_interval(hours => $2), resolved_at = resolved_at - make_interval(hours => $2), "
         "closed_at = closed_at - make_interval(hours => $2) WHERE id = $1", {id, hours});
    exec("UPDATE complaint_history SET created_at = created_at - make_interval(hours => $2) WHERE complaint_id = $1", {id, hours});
    exec("UPDATE complaint_status_history SET changed_at = changed_at - make_interval(hours => $2) WHERE complaint_id = $1", {id, hours});
    exec("UPDATE notifications SET created_at = created_at - make_interval(hours => $2) WHERE complaint_id = $1", {id, hours});
    tx.commit();
}

} // namespace scms
