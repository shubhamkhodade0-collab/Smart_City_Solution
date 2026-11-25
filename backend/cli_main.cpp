// =====================================================================
// cli_main.cpp - scms_cli: backend utility (no UI)
//
//   scms_cli --check            connect, print DB + Assembly status
//   scms_cli --seed-demo N      create N DEMO complaints through the real pipeline
//   scms_cli --process-queue    run the FIFO assignment processor until empty
//   scms_cli --demo-status      how many demo complaints / demo users exist
//   scms_cli --clear-demo       delete ALL demo complaints (owned by the demo.* accounts); add --users to remove those accounts too
//
//   AI assistant (runs locally, uses the seeded demo passwords):
//   scms_cli --ai-triage USER            rank USER's open complaints by AI priority
//   scms_cli --ai-advise USER REF        full AI advice for one complaint, e.g. citizen SCMS-2026-000012
//   scms_cli --ai-ask USER "question"    chat with the assistant, e.g. water.officer "what should I handle first?"
//   scms_cli --groq-test ["question"]    check the optional Groq connection (key: .env or environment variable)
//   scms_cli --ask-ai "question"         ask the AI directly from the terminal (any topic, needs a Groq key)
// =====================================================================
#include "AiAgent.h"
#include "AsmEngine.h"
#include "ComplaintService.h"
#include "Config.h"
#include "Exceptions.h"
#include <cstdio>
#include <cstring>
#include <random>

using namespace scms;

static DbConfig dbConfig() {
    Config& c = Config::instance();
    return {c.get("DB_HOST", "localhost"), c.get("DB_PORT", "5433"), c.get("DB_NAME", "scms"),
            c.get("DB_USER", "scms_app"), c.get("DB_PASSWORD", "")};
}

// DEMO dataset generator - every complaint goes through ComplaintService exactly like the UI
static int seedDemo(Database& db, int count) {
    ComplaintService svc(db);
    svc.loadReferenceData();
    std::mt19937 rng(20261007);
    auto pick = [&](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };

    std::vector<CityInfo> all = db.getAllCities();
    // weight large urban districts higher so the ranking looks like a real deployment
    const char* hubs[] = {"Pune", "Mumbai", "Mumbai Suburban", "Thane", "Nagpur", "Nashik", "Raigad", "Bengaluru Urban",
                          "Hyderabad", "Chennai", "Kolkata", "Ahmedabad", "Surat", "Jaipur", "Lucknow", "Kanpur Nagar",
                          "Patna", "Bhopal", "Indore", "Ludhiana", "Coimbatore", "Visakhapatnam", "Ernakulam", "Guwahati"};
    std::vector<CityInfo> pool;
    for (const auto& c : all)
        for (const char* h : hubs)
            if (c.city == h) { for (int k = 0; k < 6; ++k) pool.push_back(c); }
    for (int k = 0; k < 120; ++k) pool.push_back(all[static_cast<std::size_t>(pick(static_cast<int>(all.size())))]);

    const std::vector<std::pair<std::string, std::vector<std::string>>> templates = {
        {"TRAFFIC", {"Traffic signal at the junction has not worked for two days causing jams",
                     "Vehicles parked illegally on both sides block the main road every evening",
                     "Heavy congestion near the school gate during morning hours needs a warden"}},
        {"GARBAGE", {"Garbage has not been collected from our lane for five days and it smells badly",
                     "Overflowing community bin attracting stray dogs near the bus stop",
                     "Construction debris dumped on the footpath blocks pedestrians"}},
        {"ELECTRICITY", {"Exposed electric wires hanging low near the children's park are dangerous",
                         "Frequent power cuts every night for the last week in our sector",
                         "Street lights on the main road are not working since last month"}},
        {"WATER", {"No water supply in the building for three days during summer",
                   "Muddy and foul smelling tap water being supplied in the morning",
                   "Main pipeline leaking on the road wasting thousands of litres"}},
        {"ROADS", {"Large potholes on the main road causing two-wheeler accidents",
                   "Road surface washed away after rain and not repaired yet",
                   "Footpath tiles broken and uneven making walking unsafe for seniors"}},
        {"DRAINAGE", {"Drain blocked with plastic causing waterlogging after every rain",
                      "Open drain overflowing into the street near the market",
                      "Manhole cover missing on the main road posing a serious risk"}}};
    const char* localities[] = {"Station Road", "MG Road", "Shivaji Nagar", "Gandhi Chowk", "Market Yard", "Civil Lines",
                                "Sector 12", "Old City", "Ring Road", "Bus Stand Area", "Nehru Colony", "Lake View"};
    const char* priorities[] = {"LOW", "NORMAL", "NORMAL", "HIGH", "URGENT"};

    auto admin = svc.login("admin", "Admin@123");
    std::vector<std::unique_ptr<User>> citizens;
    for (const auto& u : db.getUsers())
        if (u.username.rfind("demo.", 0) == 0) citizens.push_back(svc.login(u.username, "Demo@123"));
    if (citizens.empty()) {
        std::fprintf(stderr, "No demo.* citizens - run database/demo_data.sql first\n");
        return 1;
    }
    std::vector<std::unique_ptr<User>> officers;
    for (const auto& u : db.getUsers())
        if (u.role == "DEPARTMENT") officers.push_back(svc.login(u.username, "Dept@123"));

    // pass 1: every citizen submits through the real pipeline
    std::vector<std::pair<long long, User*>> made;
    int created = 0;
    for (int i = 0; i < count; ++i) {
        User& citizen = *citizens[static_cast<std::size_t>(pick(static_cast<int>(citizens.size())))];
        const auto& tpl = templates[static_cast<std::size_t>(pick(static_cast<int>(templates.size())))];
        const CityInfo& city = pool[static_cast<std::size_t>(pick(static_cast<int>(pool.size())))];
        ComplaintInput in;
        in.citizenName = citizen.fullName();
        in.phone = citizen.phone();
        in.stateId = city.stateId;
        in.districtId = city.districtId;
        in.cityId = city.cityId;
        in.locality = std::string(localities[pick(12)]) + ", " + city.city;
        in.categoryCode = tpl.first;
        in.description = tpl.second[static_cast<std::size_t>(pick(3))] + " (demo #" + std::to_string(i + 1) + ")";
        in.impact = 1 + pick(5);
        in.urgency = 1 + pick(5);
        in.priority = priorities[pick(5)];
        try {
            svc.loadRuntime(citizen);
            SubmitResult res = svc.submitComplaint(citizen, in);
            made.emplace_back(res.id, &citizen);
            ++created;
            if ((i + 1) % 50 == 0) std::printf("  submitted %d / %d\n", i + 1, count);
        } catch (const AppException& e) {
            std::fprintf(stderr, "  skipped #%d: %s\n", i + 1, e.what());
        }
    }

    // pass 2: the FIFO processor assigns the oldest 85% (the newest stay PENDING, as in real life)
    svc.loadRuntime(*admin);
    std::size_t toProcess = made.size() * 85 / 100;
    for (std::size_t k = 0; k < toProcess && !svc.queue().isEmpty(); ++k) svc.processNextInQueue(*admin);

    // pass 3: departments and citizens move assigned complaints along the lifecycle
    for (std::size_t k = 0; k < toProcess; ++k) {
        long long id = made[k].first;
        auto c = db.getComplaintById(id);
        if (!c || c->status() != Status::ASSIGNED) continue;
        User* officer = nullptr;
        for (auto& o : officers)
            if (static_cast<DepartmentUser&>(*o).departmentId() == c->departmentId()) officer = o.get();
        int stage = pick(100);
        try {
            if (officer && stage >= 20) svc.changeStatus(*officer, id, Status::IN_PROGRESS, "Field team dispatched");
            if (officer && stage >= 42) svc.changeStatus(*officer, id, Status::RESOLVED, "Work completed and verified");
            if (officer && stage >= 70) svc.changeStatus(*made[k].second, id, Status::CLOSED, "Confirmed fixed - thank you");
        } catch (const AppException& e) {
            std::fprintf(stderr, "  lifecycle %lld: %s\n", id, e.what());
        }
    }

    // pass 4: spread timestamps over ~8 months in arrival order (keeps FIFO order == created_at order)
    for (std::size_t k = 0; k < made.size(); ++k) {
        int hours = static_cast<int>((made.size() - k) * (24 * 240) / made.size()) + pick(6);
        db.backdateComplaint(made[k].first, hours);
    }
    std::printf("Created %d demo complaints. Assembly calls: %llu\n", created, AsmEngine::totalCalls());
    return 0;
}

// ---- AI assistant commands -------------------------------------------------------------------
// the seeded accounts all use a known password (see database/seed.sql and demo_data.sql)
static bool devPassword(const std::string& user, std::string& pw) {
    if (user == "admin") pw = "Admin@123";
    else if (user == "citizen") pw = "Citizen@123";
    else if (user.size() > 8 && user.compare(user.size() - 8, 8, ".officer") == 0) pw = "Dept@123";
    else if (user.rfind("demo.", 0) == 0) pw = "Demo@123";
    else return false;
    return true;
}

static void printLines(const ai::Lines& lines) {
    int step = 0;
    for (const auto& l : lines) {
        if (l.kind != ai::LineKind::STEP) step = 0;
        switch (l.kind) {
            case ai::LineKind::HEADING: std::printf("\n== %s\n", l.text.c_str()); break;
            case ai::LineKind::TEXT: std::printf("   %s\n", l.text.c_str()); break;
            case ai::LineKind::BULLET: std::printf("   - %s\n", l.text.c_str()); break;
            case ai::LineKind::STEP: std::printf("   %d. %s\n", ++step, l.text.c_str()); break;
            case ai::LineKind::WARNING: std::printf("   [!] %s\n", l.text.c_str()); break;
            case ai::LineKind::TIP: std::printf("   [tip] %s\n", l.text.c_str()); break;
            case ai::LineKind::NOTE: std::printf("   (%s)\n", l.text.c_str()); break;
        }
    }
}

// ---- optional Groq connection --------------------------------------------------------------------------------
static int groqTest(const std::string& question) {
    ai::LlmSettings s = ai::loadLlmSettings();
    std::printf("Groq key   : %s (%s)\nModel      : %s\nAddress    : %s\nMode       : %s\n", s.ready() ? ai::maskKey(s.apiKey).c_str() : "(none)", s.keySource.c_str(),
                s.model.c_str(), s.baseUrl.c_str(), s.mode == 1 ? "every question" : "only questions the built-in assistant cannot answer");
    if (!s.ready()) {
        std::printf("\nNo key. Set GROQ_API_KEY in the project-root .env file or in the process environment.\n");
        return 2;
    }
    ai::ModelList models = ai::groqListModels(s);
    if (!models.ok) { std::printf("\nCould not list models: %s\n", models.error.c_str()); return 3; }
    bool found = false;
    for (const auto& m : models.models) found = found || m == s.model;
    std::printf("\nConnected. %zu chat models available; '%s' is %s.\n", models.models.size(), s.model.c_str(), found ? "available" : "NOT in the list");
    if (question.empty()) return 0;
    ai::LlmRequest req;
    req.settings = s;
    req.messages.push_back({"system", ai::AiAgent::systemPrompt(true)});
    req.messages.push_back({"user", question});
    ai::LlmResult r = ai::groqChat(req);
    if (!r.ok) { std::printf("\nQuestion failed: %s\n", r.error.c_str()); return 4; }
    std::printf("\nAnswer (%s):\n", r.model.c_str());
    printLines(ai::AiAgent::formatLlmAnswer(r.text));
    return 0;
}

static int aiCommand(Database& db, const std::string& cmd, const std::string& user, const std::string& arg) {
    std::string pw;
    if (!devPassword(user, pw)) {
        std::fprintf(stderr, "Unknown demo account '%s' (use admin, citizen, <dept>.officer or demo.<name>)\n", user.c_str());
        return 2;
    }
    ComplaintService svc(db);
    svc.loadReferenceData();
    std::unique_ptr<User> me = svc.login(user, pw);
    ai::AiAgent agent(db, svc);
    if (cmd == "--ai-triage") {
        ai::Triage t = agent.triage(*me);
        std::printf("AI triage for %s: %d open, %d past SLA, %d close to SLA, %d critical  (%s, %llu comparisons)\n", user.c_str(), t.open, t.overdue,
                    t.atRisk, t.critical, t.sort.algorithm.c_str(), static_cast<unsigned long long>(t.sort.comparisons));
        int n = 0;
        for (const auto& it : t.items) {
            std::printf("%3d. %-17s %-11s %-18s %-9s %3d  %s\n", ++n, it.reference.c_str(), it.category.c_str(), it.city.c_str(), it.label.c_str(), it.priority,
                        it.reason.c_str());
            if (n >= 15) break;
        }
        return 0;
    }
    if (cmd == "--ai-advise") {
        std::unique_ptr<Complaint> c = db.getComplaintByReference(arg);
        if (!c || !me->canView(*c)) {
            std::fprintf(stderr, "No complaint %s visible to %s\n", arg.c_str(), user.c_str());
            return 3;
        }
        ai::Insight in = agent.analyse(*me, *c);
        std::printf("%s  [%s]\n   %s\n   AI priority %d/100 (%s)   similar open: %d%s\n", c->referenceCode().c_str(), toString(c->status()), in.headline.c_str(),
                    in.priority, in.priorityLabel.c_str(), in.similarOpen, in.hotspot ? "  HOTSPOT" : "");
        for (const auto& s : in.signals) std::printf("   + %s (+%d)\n", s.text.c_str(), s.points);
        for (const ai::Section* sec : {&in.plan, &in.support, &in.outlook}) {
            std::printf("\n##### %s\n", sec->title.c_str());
            printLines(sec->lines);
        }
        if (!in.suggestedNote.empty()) std::printf("\n%s:\n   \"%s\"\n", in.noteLabel.c_str(), in.suggestedNote.c_str());
        return 0;
    }
    ai::Reply r = agent.chat(*me, arg, -1);
    ai::LlmSettings gs = ai::loadLlmSettings();   // optional: the built-in assistant did not understand -> ask Groq
    if (ai::AiAgent::wantsLlm(r, gs)) {
        ai::LlmRequest rq = agent.llmRequest(*me, arg, r.focusId > 0 ? r.focusId : -1, {}, gs, &r);
        r = ai::AiAgent::withLlmAnswer(r, ai::groqChat(rq), gs);
    }
    std::printf("[intent: %s]\n", r.intent.c_str());
    printLines(r.lines);
    if (!r.followUps.empty()) {
        std::printf("\nSuggestions:");
        for (const auto& f : r.followUps) std::printf("  [%s]", f.c_str());
        std::printf("\n");
    }
    return 0;
}

int main(int argc, char** argv) {
    Config::instance().load("config/app.conf");
    if (argc >= 2 && (std::strcmp(argv[1], "--groq-test") == 0 || std::strcmp(argv[1], "--ask-ai") == 0)) return groqTest(argc >= 3 ? argv[2] : "");
    Database db(dbConfig());
    if (!db.connect()) {
        std::fprintf(stderr, "PostgreSQL connection failed: %s\n", db.lastError().c_str());
        return 2;
    }
    if (argc >= 3 && std::strcmp(argv[1], "--seed-demo") == 0) return seedDemo(db, std::atoi(argv[2]));
    if (argc >= 3 && std::strcmp(argv[1], "--ai-triage") == 0) return aiCommand(db, argv[1], argv[2], "");
    if (argc >= 4 && (std::strcmp(argv[1], "--ai-advise") == 0 || std::strcmp(argv[1], "--ai-ask") == 0)) return aiCommand(db, argv[1], argv[2], argv[3]);
    if (argc >= 2 && std::strcmp(argv[1], "--demo-status") == 0) {
        Database::DemoCounts c = db.demoCounts();
        std::printf("demo complaints: %d   demo accounts: %d   all complaints: %d   (real complaints: %d)\n", c.complaints, c.users, c.total, c.total - c.complaints);
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--clear-demo") == 0) {
        bool users = argc >= 3 && std::strcmp(argv[2], "--users") == 0;
        Database::DemoCounts before = db.demoCounts();
        try {
            Database::DemoRemoval r = db.clearDemoData(users);
            std::printf("Removed %d demo complaints (with their history and notifications) and %d audit rows.\nReal complaints kept: %d\n", r.complaints, r.auditRows,
                        before.total - before.complaints);
            if (users && !r.usersBlocked) std::printf("Removed %d demo accounts. Bring them back with: psql -f database/demo_data.sql\n", r.users);
            else if (users) std::printf("The demo accounts were KEPT: some real record still refers to them.\n  (%s)\n", r.blockedReason.substr(0, 160).c_str());
            else std::printf("The demo accounts were kept, so '--seed-demo 300' can be run again.\n");
        } catch (const AppException& e) {
            std::fprintf(stderr, "Could not clear the demo data: %s\n", e.what());
            return 5;
        }
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--process-queue") == 0) {
        ComplaintService svc(db);
        auto admin = svc.login("admin", "Admin@123");
        svc.loadRuntime(*admin);
        int n = 0;
        while (!svc.queue().isEmpty()) { svc.processNextInQueue(*admin); ++n; }
        std::printf("Processed %d complaints\n", n);
        return 0;
    }
    std::printf("PostgreSQL %s  %s\nAssembly self-test: %s\n", db.serverVersion().c_str(), db.connectionInfo().c_str(),
                AsmEngine::selfTest() ? "ONLINE" : "FAULT");
    Summary s = db.getSummary({});
    std::printf("complaints=%d pending=%d assigned=%d in_progress=%d resolved=%d closed=%d high=%d\n", s.total, s.pending,
                s.assigned, s.inProgress, s.resolved, s.closed, s.high);
    return 0;
}
