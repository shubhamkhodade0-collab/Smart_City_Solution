// =====================================================================
// App.h - the OpenGL application: session, routing, cached data, screens
// Data is cached per screen and refreshed only after an action or on a
// timer (never queried every frame).
// =====================================================================
#pragma once
#include "../../../backend/AiAgent.h"
#include "../../../backend/ComplaintService.h"
#include "../../../backend/ReportService.h"
#include "../Charts/Charts.h"
#include "../Map/IndiaMap.h"
#include "../UI/Widgets.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class Screen {
    LOGIN, REGISTER, DASHBOARD, REPORT, COMPLAINTS, DETAIL, NOTIFICATIONS, PROFILE, SETTINGS,
    QUEUE, MAP, RANKING, DEPARTMENTS, REPORTS, USERS, AUDIT, DSA, ASSEMBLY, ANALYTICS3D, SEARCH, ASSISTANT
};

struct NavItem { Screen screen; const char* label; const char* icon; const char* section; };

class App {
public:
    App();
    bool init();
    void display();
    void reshape(int w, int h);
    void setPixelScale(float scale) { pixelScale_ = scale > 1.0f ? scale : 1.0f; }
    void mouseButton(int button, int state, int x, int y);
    void mouseMove(int x, int y);
    void wheel(int dir, int x, int y);
    void keyChar(unsigned char c);
    void keySpecial(int key);
    void tick();     // timer: periodic refresh of notifications / DB health

    static constexpr int MIN_W = 1280, MIN_H = 760;

private:
    // ---------- infrastructure
    bool guarded(const std::function<void()>& f);   // runs f, turns any AppException into an error toast
    void go(Screen s);
    void logout();
    std::vector<NavItem> navItems() const;
    void drawShell();
    void drawSidebar();
    void drawTopbar();
    ui::Rect content() const;
    std::string screenTitle(Screen s) const;
    void refreshNotifications();
    void markDirty();

    // ---------- screens
    void screenLogin();
    void screenRegister();
    void screenDashboard();
    void dashboardAdmin(const ui::Rect& r);
    void dashboardDepartment(const ui::Rect& r);
    void dashboardCitizen(const ui::Rect& r);
    void screenReport();
    void screenComplaints();
    void screenDetail();
    void screenNotifications();
    void screenProfile();
    void screenSettings();
    void screenQueue();
    void screenMap();
    void screenRanking();
    void screenDepartments();
    void screenReports();
    void screenUsers();
    void screenAudit();
    void screenDsa();
    void screenAssembly();
    void screen3D();
    void screenSearch();
    void screenAssistant();

    // ---------- helpers
    void loadDashboard();
    void loadList();
    void openDetail(long long id);
    void loadMap();
    void loadRanking();
    void loadReport();
    void loadStatesInto(ui::Dropdown& dd, std::vector<scms::IdName>& store);
    void complaintTable(const ui::Rect& r, std::vector<std::unique_ptr<scms::Complaint>>& rows, ui::Table& table,
                        bool showDepartment = true);
    std::vector<ui::Cell> complaintRow(const scms::Complaint& c, bool showDepartment);
    void statusStepper(const ui::Rect& r, scms::Status s);
    void runSearch();

    // ---------- AI assistant (ScreensAI.cpp)
    float aiLines(const scms::ai::Lines& lines, float x, float y, float w, bool draw);
    float aiColumn(const std::string& title, const scms::ai::Lines& lines, float x, float y, float w, bool draw);
    float aiDetailHeight(float width, bool hasNote);
    void aiDetailCard(const ui::Rect& r, bool canUseNote);
    void aiReportPanel(const ui::Rect& r);
    void aiLoadInsight();
    void aiSend(const std::string& text);
    void aiAsk(long long complaintId, const std::string& question);
    void aiReset();
    void loadDepartments();
    void deptDetailDialog();
    void exportDepartmentsCsv();
    void aiPoll();
    void scopeToggle(const ui::Rect& r);
    scms::Scope geoScope() const;

public:
    // shared visual helpers (also used by free functions)
    static ui::Color statusColor(scms::Status s, bool soft);
    static ui::Color severityColor(scms::SeverityLevel l, bool soft);
    static std::string fmtTime(std::time_t t, bool withTime = true);

private:
    // ---------- services
    scms::Database db_;
    scms::ComplaintService svc_;
    scms::ReportService reports_;
    scms::ai::AiAgent agent_;
    map::IndiaMap india_;
    std::string mapError_;
    std::unique_ptr<scms::User> user_;
    bool dbOnline_ = false;
    std::string dbError_;
    bool asmOnline_ = false;

    Screen screen_ = Screen::LOGIN;
    int width_ = 1440, height_ = 900;
    int winH_ = 0;
    float pixelScale_ = 1.0f;
    double lastTick_ = 0;

    // ---------- auth
    ui::TextInput lUser_{40}, lPass_{64, false, true};
    std::string loginError_;
    ui::TextInput gName_{60}, gUser_{40}, gPhone_{10, true}, gPass_{64, false, true}, gConfirm_{64, false, true};
    ui::Dropdown gState_, gDistrict_, gCity_;
    std::vector<scms::IdName> gStates_, gDistricts_, gCities_;
    long long publicTotal_ = -1;
    int publicStates_ = 0;

    // ---------- dashboard cache
    bool dashLoaded_ = false;
    bool dsaLoaded_ = false;
    scms::Summary sum_;
    std::vector<scms::CountRow> byCat_, bySev_, monthly_;
    std::vector<scms::Department> deptRank_;
    scms::SortMetrics deptSort_;
    std::vector<std::unique_ptr<scms::Complaint>> recent_;
    double avgRes_ = 0;
    ui::Table recentTable_;
    ui::ScrollPanel pageScroll_;

    // ---------- notifications
    std::vector<scms::NotificationRow> notes_;
    int unread_ = 0;
    ui::ScrollPanel notesScroll_;

    // ---------- complaint list
    bool listDirty_ = true;
    std::vector<std::unique_ptr<scms::Complaint>> list_;
    int listTotal_ = 0, listPage_ = 0;
    static constexpr int PAGE = 12;
    ui::Table listTable_;
    ui::TextInput listSearch_{80};
    ui::Dropdown fStatus_, fCategory_;
    int listSort_ = 0;
    std::string appliedSearch_;

    // ---------- detail
    std::unique_ptr<scms::Complaint> detail_;
    std::vector<scms::HistoryRow> history_;
    std::vector<scms::StatusRow> statusRows_;
    ui::TextInput note_{200};
    bool confirmOpen_ = false;
    scms::Status confirmTo_ = scms::Status::PENDING;
    ui::ScrollPanel detailScroll_;
    Screen backFromDetail_ = Screen::COMPLAINTS;

    // ---------- report form
    ui::TextInput rName_{60}, rPhone_{10, true}, rLocality_{120}, rDesc_{1000};
    ui::Dropdown rState_, rDistrict_, rCity_, rCategory_, rPriority_;
    std::vector<scms::IdName> rStates_, rDistricts_, rCities_;
    int rImpact_ = 0, rUrgency_ = 0;
    std::string rImpactErr_, rUrgencyErr_;
    bool submitOpen_ = false;
    scms::SubmitResult lastSubmit_;

    // ---------- global search
    ui::TextInput topSearch_{80};
    scms::SearchOutcome search_;
    std::string searchedFor_;
    ui::Table searchTable_;

    // ---------- queue
    scms::ProcessResult lastProcess_;
    bool processedOnce_ = false;

    // ---------- map
    bool mapLoaded_ = false;
    scms::CityArray mapMarkers_;                   // city bubbles (reloaded together with the state / district counts)
    int viewScope_ = 0;                            // citizens / departments: 0 = all India, 1 = only my own complaints / my department
    std::vector<scms::RegionStat> mapStates_;     // indexed by map state index
    std::vector<scms::RegionStat> mapDistricts_;
    std::vector<int> stateCount_;                  // complaints per map state index
    std::vector<int> districtCount_;               // per map district index (selected state only)
    int t1_ = 1, t2_ = 2;                          // legend thresholds (from data)
    int hoverDistrict_ = -1, selState_ = -1, selDistrict_ = -1;
    bool dragging_ = false;
    float dragX_ = 0, dragY_ = 0, dragDist_ = 0;
    ui::TextInput mapSearch_{60};
    ui::ScrollPanel mapListScroll_;
    std::vector<scms::CityInfo> allCities_;
    scms::SortMetrics districtSort_;

    // ---------- ranking
    bool rankLoaded_ = false;
    scms::CityRanking ranking_;
    int rankTop_ = 0, rankOrder_ = 0;
    ui::TextInput rankSearch_{60};
    ui::Table rankTable_;

    // ---------- reports
    int reportSel_ = 0;
    bool reportLoaded_ = false;
    scms::Report report_;
    ui::Table reportTable_;

    // ---------- users / audit
    std::vector<scms::UserRow> users_;
    ui::Table usersTable_;
    bool userModal_ = false;
    ui::TextInput uName_{60}, uUser_{40}, uPhone_{10, true}, uPass_{64, false, true}, uConfirm_{64, false, true};
    ui::Dropdown uRole_, uDept_;
    std::vector<scms::AuditRow> audit_;
    ui::Table auditTable_;
    ui::Dropdown auditFilter_;
    bool usersLoaded_ = false, auditLoaded_ = false;

    // ---------- DSA lab
    ui::TextInput dsaKey_{60};
    std::string dsaResult_;
    std::vector<scms::SortMetrics> sortBench_;
    scms::SearchMetrics dsaLinear_, dsaBinary_;
    bool dsaSearched_ = false;

    // ---------- assembly
    ui::Dropdown asmCat_;
    int asmImpact_ = 2, asmUrgency_ = 3;   // segmented index (value-1)
    ui::TextInput mulA_{9, true}, mulB_{9, true};

    // ---------- Departments screen (ScreensDepartments.cpp)
    struct DeptInfo {
        int id = 0, slaHours = 0;
        std::string name, code, categoryCode, categoryName;
        int assigned = 0, inProgress = 0, resolved = 0, closed = 0, highOpen = 0;
        double resolutionRate = 0, avgAgeHours = 0, oldestAgeHours = 0;
        std::string oldestRef;
        scms::ai::Triage tri;                       // this department's open complaints, ranked by AI priority
        std::vector<scms::UserRow> officers;
        scms::CityRanking cities;                   // busiest cities (loaded when the detail dialog opens)
        bool citiesLoaded = false;
    };
    std::vector<DeptInfo> deptInfo_;
    scms::SortMetrics deptSortMetrics_;
    bool deptLoaded_ = false, deptDialog_ = false;
    int deptSortSel_ = 0, deptOpenIdx_ = -1;
    scms::ai::Lines deptInsight(const DeptInfo& d) const;

    // ---------- AI assistant
    struct AiMessage {
        bool fromUser = false;
        scms::ai::Lines lines;
        std::vector<std::string> chips;
        bool pending = false;                      // "Asking GPT-4..." bubble waiting for the answer
        float h = -1, hw = 0;                      // cached height for the width it was measured at
    };
    // ---- optional Groq connection (configured through .env/environment)
    scms::ai::LlmSettings groq_;
    struct PendingLlm {
        std::shared_ptr<scms::ai::Async<scms::ai::LlmResult>> job;
        std::size_t msgIndex = 0;
        scms::ai::Reply builtIn;                   // the built-in answer: shown instead if Groq fails
    } aiPending_;
    bool aiOpen_ = true;                           // AI card on the complaint screen expanded?
    std::unique_ptr<scms::ai::Insight> aiInsight_; // analysis of the complaint on screen
    std::vector<AiMessage> aiChat_;
    ui::TextInput aiInput_{300};
    ui::ScrollPanel aiScroll_, aiSideScroll_;
    bool aiStick_ = true;                          // scroll the chat to the newest message
    long long aiFocusId_ = 0;                      // complaint the chat is talking about (0 = none)
    std::string aiFocusRef_, aiFocusTitle_;
    scms::ai::Triage aiTriage_;
    bool aiTriageLoaded_ = false;
    std::string aiDraftSeen_;                      // report form: text the helper last analysed
    scms::ai::TextReading aiDraft_;

    // ---------- 3D
    charts::Chart3D chart3d_;
    scms::StatGrid grid_;
    std::vector<std::string> gridStates_, gridCats_;
    bool gridLoaded_ = false;
};
