#include "ReportService.h"
#include "Exceptions.h"
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace scms {

std::string percent(long long part, long long whole) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f%%", whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / whole);
    return buf;
}

const std::vector<std::pair<ReportType, std::string>>& ReportService::types() {
    static const std::vector<std::pair<ReportType, std::string>> t{
        {ReportType::SUMMARY, "Summary"},       {ReportType::CITY, "City"},
        {ReportType::DISTRICT, "District"},     {ReportType::STATE, "State"},
        {ReportType::CATEGORY, "Category"},     {ReportType::DEPARTMENT, "Department"},
        {ReportType::SEVERITY, "Severity"},     {ReportType::RESOLUTION, "Resolution"},
        {ReportType::MONTHLY, "Monthly Trend"}};
    return t;
}

Report ReportService::generate(ReportType type, const Scope& scope, int stateId) {
    Report r{type, "", "", {}, {}, {}, {}, ""};
    Summary s = db_.getSummary(scope);
    const long long total = s.total;
    char sub[160];
    std::time_t now = std::time(nullptr);
    std::strftime(sub, sizeof sub, "Generated %d %b %Y %H:%M from live PostgreSQL data", std::localtime(&now));
    r.subtitle = sub;

    switch (type) {
    case ReportType::SUMMARY: {
        r.title = "Complaint Summary";
        r.columns = {"Status", "Complaints", "Share"};
        const std::pair<const char*, int> rows[] = {{"Pending", s.pending}, {"Assigned", s.assigned},
                                                    {"In Progress", s.inProgress}, {"Resolved", s.resolved},
                                                    {"Closed", s.closed}};
        for (auto& [label, v] : rows) {
            r.rows.push_back({label, std::to_string(v), percent(v, total)});
            r.chartLabels.push_back(label);
            r.chartValues.push_back(v);
        }
        r.rows.push_back({"High / critical severity", std::to_string(s.high), percent(s.high, total)});
        r.rows.push_back({"Overdue (past SLA)", std::to_string(s.overdue), percent(s.overdue, total)});
        r.rows.push_back({"Total", std::to_string(total), "100.0%"});
        break;
    }
    case ReportType::CITY: {
        r.title = "City Report";
        CityRanking cr = svc_.cityRanking(true, 0, "", scope);
        r.algorithmNote = "Ranked with " + cr.sort.algorithm + " (" + std::to_string(cr.sort.comparisons) +
                          " comparisons, " + std::to_string(cr.sort.swaps) + " swaps). Source: PostgreSQL -> CityArray";
        r.columns = {"Rank", "City", "District", "State", "Complaints", "Pending", "Resolved", "High", "Share"};
        int rank = 1;
        for (const auto& c : cr.rows) {
            r.rows.push_back({std::to_string(rank++), c.city, c.district, c.state, std::to_string(c.complaints),
                              std::to_string(c.pending), std::to_string(c.resolved), std::to_string(c.highSeverity),
                              percent(c.complaints, cr.total)});
            if (r.chartLabels.size() < 10) {
                r.chartLabels.push_back(c.city);
                r.chartValues.push_back(c.complaints);
            }
        }
        break;
    }
    case ReportType::DISTRICT: {
        r.title = "District Report";
        r.columns = {"State", "District", "Complaints", "Pending", "Resolved", "High", "Share"};
        std::vector<RegionStat> states = db_.stateStats(scope);
        for (const auto& st : states) {
            if (stateId > 0 && st.id != stateId) continue;
            if (st.complaints == 0) continue;
            for (const auto& d : db_.districtStats(st.id, scope)) {
                if (d.complaints == 0) continue;
                r.rows.push_back({st.name, d.name, std::to_string(d.complaints), std::to_string(d.pending),
                                  std::to_string(d.resolved), std::to_string(d.highSeverity), percent(d.complaints, total)});
                if (r.chartLabels.size() < 10) {
                    r.chartLabels.push_back(d.name);
                    r.chartValues.push_back(d.complaints);
                }
            }
        }
        break;
    }
    case ReportType::STATE: {
        r.title = "State Report";
        r.columns = {"State", "Districts", "Complaints", "Pending", "Resolved", "High", "Share"};
        std::vector<RegionStat> states = db_.stateStats(scope);
        quickSort(states.data(), states.size(), [](const RegionStat& a, const RegionStat& b) {
            return a.complaints != b.complaints ? a.complaints > b.complaints : a.name < b.name;
        });
        r.algorithmNote = "Ordered with Quick Sort";
        for (const auto& st : states) {
            r.rows.push_back({st.name, std::to_string(st.subRegions), std::to_string(st.complaints), std::to_string(st.pending),
                              std::to_string(st.resolved), std::to_string(st.highSeverity), percent(st.complaints, total)});
            if (r.chartLabels.size() < 10 && st.complaints > 0) {
                r.chartLabels.push_back(st.name);
                r.chartValues.push_back(st.complaints);
            }
        }
        break;
    }
    case ReportType::CATEGORY: {
        r.title = "Category Report";
        r.columns = {"Category", "Complaints", "Resolved/Closed", "Resolution rate", "Share"};
        for (const auto& c : db_.countByCategory(scope)) {
            r.rows.push_back({c.label, std::to_string(c.count), std::to_string(c.extra), percent(c.extra, c.count),
                              percent(c.count, total)});
            r.chartLabels.push_back(c.label);
            r.chartValues.push_back(c.count);
        }
        break;
    }
    case ReportType::DEPARTMENT: {
        r.title = "Department Report";
        DepartmentRanking dr = svc_.departmentRanking();
        r.algorithmNote = "Ranked by open workload with " + dr.sort.algorithm + " (" +
                          std::to_string(dr.sort.comparisons) + " comparisons, " + std::to_string(dr.sort.swaps) + " swaps)";
        r.columns = {"Department", "Assigned", "In Progress", "Resolved", "Closed", "Open high-severity", "Resolution rate"};
        for (const auto& d : dr.rows) {
            if (scope.departmentId > 0 && d.id() != scope.departmentId) continue;
            const Workload& w = d.workload();
            char rate[16];
            std::snprintf(rate, sizeof rate, "%.1f%%", d.resolutionRate());
            r.rows.push_back({d.name(), std::to_string(w.assigned), std::to_string(w.inProgress), std::to_string(w.resolved),
                              std::to_string(w.closed), std::to_string(w.highSeverity), rate});
            r.chartLabels.push_back(d.code());
            r.chartValues.push_back(w.open());
        }
        break;
    }
    case ReportType::SEVERITY: {
        r.title = "Severity Report";
        r.columns = {"Severity level", "Score range (Assembly)", "Complaints", "Share"};
        const char* ranges[] = {"0 - 29", "30 - 54", "55 - 79", "80 - 100"};
        int i = 0;
        for (const auto& c : db_.countBySeverity(scope)) {
            r.rows.push_back({c.label, ranges[i++ % 4], std::to_string(c.count), percent(c.count, total)});
            r.chartLabels.push_back(c.label);
            r.chartValues.push_back(c.count);
        }
        break;
    }
    case ReportType::RESOLUTION: {
        r.title = "Resolution Report";
        r.columns = {"Metric", "Value"};
        char avg[32];
        std::snprintf(avg, sizeof avg, "%.1f h", db_.averageResolutionHours(scope));
        r.rows = {{"Total complaints", std::to_string(total)},
                  {"Resolved + closed", std::to_string(s.resolved + s.closed)},
                  {"Resolution rate", percent(s.resolved + s.closed, total)},
                  {"Open (pending, assigned, in progress)", std::to_string(s.pending + s.assigned + s.inProgress)},
                  {"Overdue against category SLA", std::to_string(s.overdue)},
                  {"Average time to resolve", avg}};
        r.chartLabels = {"Open", "Resolved", "Closed"};
        r.chartValues = {double(s.pending + s.assigned + s.inProgress), double(s.resolved), double(s.closed)};
        break;
    }
    case ReportType::MONTHLY: {
        r.title = "Monthly Trend";
        r.columns = {"Month", "Created", "Resolved/Closed", "Resolution rate"};
        for (const auto& m : db_.monthlyTrend(scope, 12)) {
            r.rows.push_back({m.label, std::to_string(m.count), std::to_string(m.extra), percent(m.extra, m.count)});
            r.chartLabels.push_back(m.label);
            r.chartValues.push_back(m.count);
        }
        break;
    }
    }
    return r;
}

std::string ReportService::csvEscape(const std::string& s) {
    bool quote = s.find_first_of(",\"\n\r") != std::string::npos;
    if (!quote) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

std::string ReportService::exportCsv(const Report& r, const std::string& directory, int userId) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", std::localtime(&now));
    std::string name = r.title;
    for (char& c : name) c = (c == ' ') ? '_' : c;
    std::string path = directory + "/" + name + "_" + stamp + ".csv";
    std::ofstream out(path, std::ios::binary);
    if (!out) throw AppException("Could not write " + path);
    out << csvEscape(r.title) << "\n" << csvEscape(r.subtitle) << "\n\n";
    for (std::size_t i = 0; i < r.columns.size(); ++i) out << (i ? "," : "") << csvEscape(r.columns[i]);
    out << "\n";
    for (const auto& row : r.rows) {
        for (std::size_t i = 0; i < row.size(); ++i) out << (i ? "," : "") << csvEscape(row[i]);
        out << "\n";
    }
    out.close();
    if (!out) throw AppException("Writing " + path + " failed.");
    db_.createAuditLog(userId, "REPORT_EXPORTED", "REPORT", r.title, path);
    return path;
}

} // namespace scms
