// =====================================================================
// ReportService.h - report generation (tables + chart series) and CSV export
// All numbers come from PostgreSQL aggregate queries at generation time.
// =====================================================================
#pragma once
#include "ComplaintService.h"
#include <string>
#include <vector>

namespace scms {

enum class ReportType { SUMMARY, CITY, DISTRICT, STATE, CATEGORY, DEPARTMENT, SEVERITY, RESOLUTION, MONTHLY };

struct Report {
    ReportType type;
    std::string title, subtitle;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> chartLabels;
    std::vector<double> chartValues;
    std::string algorithmNote;     // e.g. "Ranked with Quick Sort"
};

class ReportService {
public:
    ReportService(Database& db, ComplaintService& svc) : db_(db), svc_(svc) {}
    static const std::vector<std::pair<ReportType, std::string>>& types();
    Report generate(ReportType type, const Scope& scope, int stateIdForDistricts = 0);
    std::string exportCsv(const Report& r, const std::string& directory, int userId);   // returns file path
    static std::string csvEscape(const std::string& s);

private:
    Database& db_;
    ComplaintService& svc_;
};

std::string percent(long long part, long long whole);

} // namespace scms
