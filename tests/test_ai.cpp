// =====================================================================
// test_ai.cpp - unit tests of the AI assistant (no database needed)
//   build + run:  make test-ai
// =====================================================================
#include "backend/AiAgent.h"
#include "backend/AsmEngine.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

using namespace scms;
using namespace scms::ai;

static int g_checks = 0, g_failed = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) {                                                                    \
            ++g_failed;                                                                   \
            std::printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);                \
        }                                                                                 \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                                \
    do {                                                                                  \
        ++g_checks;                                                                       \
        std::string _a = (a), _b = (b);                                                   \
        if (_a != _b) {                                                                   \
            ++g_failed;                                                                   \
            std::printf("  FAIL  %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a.c_str(), _b.c_str()); \
        }                                                                                 \
    } while (0)

static std::unique_ptr<Complaint> makeComplaint(const std::string& cat, const std::string& text, int impact, int urgency,
                                                Priority pr = Priority::NORMAL, const std::string& city = "Pune") {
    Location loc;
    loc.cityId = 1; loc.districtId = 1; loc.stateId = 1;
    loc.city = city; loc.district = "Pune"; loc.state = "Maharashtra"; loc.locality = "Shivaji Nagar, near the market";
    std::unique_ptr<Complaint> c = Complaint::create(cat, "Test Citizen", "9876543210", loc, text, impact, urgency, pr);
    c->computeSeverity();
    return c;
}

static std::unique_ptr<User> makeUser(Role r) {
    return User::create(r, 7, r == Role::CITIZEN ? "citizen" : "water.officer", r == Role::CITIZEN ? "Test Citizen" : "Water Officer", "9000000000",
                        r == Role::CITIZEN ? 0 : 1, r == Role::CITIZEN ? "" : "Water");
}

static void testMatching() {
    std::printf("matching rules\n");
    const std::string t = AiAgent::normalise("Two-wheeler's PITCH!  Potholes, dogs; wires & fire-wood");
    CHECK_EQ_STR(t, "two wheeler s pitch potholes dogs wires fire wood");
    CHECK(AiAgent::hasWord(t, "pothole"));          // long keyword matches an inflection
    CHECK(AiAgent::hasWord(t, "dogs"));
    CHECK(AiAgent::hasWord(t, "wire"));             // plural tolerated for short keywords
    CHECK(!AiAgent::hasWord(t, "pit"));             // "pit" is not "pitch"
    CHECK(AiAgent::hasWord(t, "fire"));             // whole word (from "fire-wood")
    CHECK(!AiAgent::hasWord(AiAgent::normalise("firewood stored"), "fire"));
    CHECK(AiAgent::hasWord(AiAgent::normalise("someone was electrocuted"), "electrocut*"));
    CHECK(AiAgent::hasWord(AiAgent::normalise("no water supply since morning"), "no water"));
    CHECK(!AiAgent::hasWord(AiAgent::normalise("no supply of water"), "no water"));
    std::string m;
    CHECK(AiAgent::hasWord(AiAgent::normalise("school gate is blocked"), "school*", &m));
    CHECK_EQ_STR(m, "school");
}

struct Sample { const char* text; const char* cat; const char* expect; };

static void testClassification() {
    std::printf("classification of realistic complaints\n");
    // the 18 sentences used by 'scms_cli --seed-demo' plus a few more
    const Sample samples[] = {
        {"Traffic signal at the junction has not worked for two days causing jams", "TRAFFIC", "TRAFFIC_SIGNAL"},
        {"Vehicles parked illegally on both sides block the main road every evening", "TRAFFIC", "TRAFFIC_PARKING"},
        {"Heavy congestion near the school gate during morning hours needs a warden", "TRAFFIC", "TRAFFIC_CONGESTION"},
        {"Garbage has not been collected from our lane for five days and it smells badly", "GARBAGE", "GARBAGE_MISSED"},
        {"Overflowing community bin attracting stray dogs near the bus stop", "GARBAGE", "GARBAGE_OVERFLOW"},
        {"Construction debris dumped on the footpath blocks pedestrians", "GARBAGE", "GARBAGE_DEBRIS"},
        {"Exposed electric wires hanging low near the children's park are dangerous", "ELECTRICITY", "ELEC_EXPOSED"},
        {"Frequent power cuts every night for the last week in our sector", "ELECTRICITY", "ELEC_OUTAGE"},
        {"Street lights on the main road are not working since last month", "ELECTRICITY", "ELEC_STREETLIGHT"},
        {"No water supply in the building for three days during summer", "WATER", "WATER_NO_SUPPLY"},
        {"Muddy and foul smelling tap water being supplied in the morning", "WATER", "WATER_CONTAMINATION"},
        {"Main pipeline leaking on the road wasting thousands of litres", "WATER", "WATER_LEAK"},
        {"Large potholes on the main road causing two-wheeler accidents", "ROADS", "ROADS_POTHOLE"},
        {"Road surface washed away after rain and not repaired yet", "ROADS", "ROADS_DAMAGE"},
        {"Footpath tiles broken and uneven making walking unsafe for seniors", "ROADS", "ROADS_FOOTPATH"},
        {"Drain blocked with plastic causing waterlogging after every rain", "DRAINAGE", "DRAIN_BLOCKED"},
        {"Open drain overflowing into the street near the market", "DRAINAGE", "DRAIN_OPEN"},
        {"Manhole cover missing on the main road posing a serious risk", "DRAINAGE", "DRAIN_OPEN"},
        {"Transformer on our street blasted last night and the whole colony has no power", "ELECTRICITY", "ELEC_TRANSFORMER"},
        {"Sewage is overflowing from the manhole and the stench is unbearable", "WATER", "WATER_SEWER"},
        {"A dead dog has been lying near the school for two days", "GARBAGE", "GARBAGE_HAZARD"},
        {"The bridge railing is broken and bent near the river", "ROADS", "ROADS_STRUCTURE"},
        {"Several accidents happened because there is no zebra crossing near the market", "TRAFFIC", "TRAFFIC_SAFETY"},
    };
    int okWith = 0, okWithout = 0, n = 0;
    for (const auto& s : samples) {
        ++n;
        TextReading a = AiAgent::readText(s.text, s.cat);
        TextReading b = AiAgent::readText(s.text);   // category unknown - the Report screen case
        bool A = a.playbook && a.playbook->id == s.expect;
        bool B = b.playbook && b.playbook->id == s.expect;
        okWith += A;
        okWithout += B;
        if (!A) std::printf("  miss (category known): %s -> %s\n", s.text, a.playbook ? a.playbook->id.c_str() : "none");
        if (!B) std::printf("  miss (category unknown): %s -> %s\n", s.text, b.playbook ? b.playbook->id.c_str() : "none");
    }
    CHECK(okWith == n);          // with the category the sub-issue must always be right
    CHECK(okWithout >= n - 1);   // without it at most one miss is tolerated
    std::printf("  category known %d/%d, unknown %d/%d\n", okWith, n, okWithout, n);

    // gibberish falls back to the general playbook of the category, with zero confidence
    TextReading g = AiAgent::readText("xyzzy plugh", "WATER");
    CHECK(g.playbook && g.playbook->general && g.confidence == 0);
    TextReading none = AiAgent::readText("xyzzy plugh");
    CHECK(none.playbook == nullptr);
    CHECK(AiAgent::readText("").impact == 0);

    // the category given by the citizen is respected even when the words point elsewhere
    TextReading w = AiAgent::readText("pothole on the road", "ROADS");
    CHECK(w.playbook && w.playbook->category == "ROADS");
}


static void testHeldOut() {
    std::printf("held-out sentences (different wording from the tuning set)\n");
    const Sample held[] = {
        {"Red light at Gandhi Chowk is dead and cars are crossing without any control", "TRAFFIC", "TRAFFIC_SIGNAL"},
        {"Bikes are double parked outside the coaching centre and the lane is choked every afternoon", "TRAFFIC", "TRAFFIC_PARKING"},
        {"Our dustbin has not been emptied for a week and there are flies everywhere", "GARBAGE", "GARBAGE_MISSED|GARBAGE_OVERFLOW"},
        {"Cable is lying on the road after the storm and a pole is leaning dangerously", "ELECTRICITY", "ELEC_EXPOSED"},
        {"There is no electricity in our flats since morning", "ELECTRICITY", "ELEC_OUTAGE"},
        {"Water from the tap is yellow and smells like sewage, children are getting stomach pain", "WATER", "WATER_CONTAMINATION"},
        {"A water pipe has burst near the temple and the road is flooded", "WATER", "WATER_LEAK"},
        {"Deep pits on the highway near the toll plaza, a biker fell yesterday", "ROADS", "ROADS_POTHOLE"},
        {"The road has caved in near the nala after heavy rain", "ROADS", "ROADS_DAMAGE"},
        {"Footpath outside the school has broken tiles and children trip over them", "ROADS", "ROADS_FOOTPATH"},
        {"Rain water is standing in our street knee deep and mosquitoes are breeding", "DRAINAGE", "DRAIN_WATERLOG"},
        {"The gutter in front of my shop is open and a child almost fell in", "DRAINAGE", "DRAIN_OPEN"},
        {"Streetlights near the park are off every night so women feel unsafe", "ELECTRICITY", "ELEC_STREETLIGHT"},
    };
    int okWith = 0, okWithout = 0, n = 0;
    for (const auto& s : held) {
        ++n;
        TextReading a = AiAgent::readText(s.text, s.cat);
        TextReading b = AiAgent::readText(s.text);
        std::string e = std::string("|") + s.expect + "|";
        bool A = a.playbook && e.find("|" + a.playbook->id + "|") != std::string::npos;
        bool B = b.playbook && e.find("|" + b.playbook->id + "|") != std::string::npos;
        okWith += A;
        okWithout += B;
        if (!A) std::printf("  miss (category known): %s -> %s\n", s.text, a.playbook ? a.playbook->id.c_str() : "none");
        if (!B) std::printf("  miss (category unknown): %s -> %s\n", s.text, b.playbook ? b.playbook->id.c_str() : "none");
    }
    std::printf("  category known %d/%d, unknown %d/%d\n", okWith, n, okWithout, n);
    CHECK(okWith >= n - 1);
    CHECK(okWithout >= n - 1);
}


// sentences written AFTER the keyword lists were tuned - an honest estimate of how well the reader generalises
static void testFresh() {
    std::printf("fresh sentences (never used to tune the keywords)\n");
    const Sample fresh[] = {
        {"Garbage truck skipped our street all this week and the heap is growing", "GARBAGE", "GARBAGE_MISSED|GARBAGE_OVERFLOW"},
        {"The traffic constable is missing and vehicles are jammed at the flyover exit every evening", "TRAFFIC", "TRAFFIC_CONGESTION"},
        {"Electric pole in front of our house is tilted and wires are touching the tree", "ELECTRICITY", "ELEC_EXPOSED"},
        {"Drinking water supply has been irregular and the pressure is very low in our building", "WATER", "WATER_NO_SUPPLY"},
        {"Sewer line is choked and dirty water is coming out on the road near the school", "WATER", "WATER_SEWER"},
        {"Crack has developed in the culvert wall and the road above looks unsafe", "ROADS", "ROADS_STRUCTURE"},
        {"Pedestrians cannot walk because hawkers have occupied the entire footpath", "ROADS", "TRAFFIC_PARKING|ROADS_FOOTPATH"},
        {"Nala is full of silt and plastic bottles and has not been cleaned since last year", "DRAINAGE", "DRAIN_BLOCKED"},
        {"Transformer near the park makes a loud humming noise and gives off a burning smell", "ELECTRICITY", "ELEC_TRANSFORMER"},
        {"People are throwing building rubble at the vacant plot behind our society", "GARBAGE", "GARBAGE_DEBRIS"},
    };
    int okWith = 0, okWithout = 0, n = 0;
    for (const auto& s : fresh) {
        ++n;
        TextReading a = AiAgent::readText(s.text, s.cat);
        TextReading b = AiAgent::readText(s.text);
        std::string e = std::string("|") + s.expect + "|";
        bool A = a.playbook && e.find("|" + a.playbook->id + "|") != std::string::npos;
        bool B = b.playbook && e.find("|" + b.playbook->id + "|") != std::string::npos;
        okWith += A;
        okWithout += B;
        if (!A) std::printf("  miss (category known): %s -> %s\n", s.text, a.playbook ? a.playbook->id.c_str() : "none");
        if (!B) std::printf("  miss (category unknown): %s -> %s\n", s.text, b.playbook ? b.playbook->id.c_str() : "none");
    }
    std::printf("  category known %d/%d, unknown %d/%d\n", okWith, n, okWithout, n);
    CHECK(okWith >= 7);       // thresholds set from the measured result - they guard against regressions
    CHECK(okWithout >= 5);
}

static bool hasSignal(const TextReading& r, const std::string& needle) {
    for (const auto& s : r.signals)
        if (s.text.find(needle) != std::string::npos) return true;
    return false;
}

static void testCues() {
    std::printf("danger cues, duration, suggestions\n");
    CHECK(AiAgent::readText("no water for three days", "WATER").durationDays == 3);
    CHECK(AiAgent::readText("not working since last week").durationDays == 7);
    CHECK(AiAgent::readText("broken since last month").durationDays == 30);
    CHECK(AiAgent::readText("garbage for 2 weeks").durationDays == 14);
    CHECK(AiAgent::readText("for a few days").durationDays == 3);
    CHECK(AiAgent::readText("since yesterday evening").durationDays == 1);
    CHECK(AiAgent::readText("the drain smells").durationDays == 0);

    TextReading wires = AiAgent::readText("Live wire sparking near the children's school", "ELECTRICITY");
    CHECK(wires.hazard);
    CHECK(hasSignal(wires, "vulnerable"));
    CHECK(wires.impact == 5 && wires.urgency == 5);

    TextReading many = AiAgent::readText("About 50 families in our colony have no water", "WATER");
    CHECK(hasSignal(many, "many people"));
    TextReading repeat = AiAgent::readText("I already complained twice and there was no action", "WATER");
    CHECK(repeat.repeatReport);

    // suggestions stay in range for any text
    const char* texts[] = {"a", "water leaking everywhere children school hospital fire accident dengue 100 families for 3 months again", "zzz"};
    for (const char* t : texts) {
        TextReading r = AiAgent::readText(t);
        if (r.impact != 0) CHECK(r.impact >= 1 && r.impact <= 5 && r.urgency >= 1 && r.urgency <= 5);
        CHECK(r.missing.size() <= 3);
    }
    // a thin description gets advice on what to add
    TextReading thin = AiAgent::readText("tap dry", "WATER", "");
    CHECK(!thin.missing.empty());
    // a good description needs less
    TextReading full = AiAgent::readText("No water supply in all 40 houses of our lane for three days, tankers are not coming", "WATER", "Rose Lane near the temple");
    CHECK(full.missing.empty());
}

static void testClockAndPriority() {
    std::printf("SLA clock and priority\n");
    CHECK_EQ_STR(AiAgent::formatHours(0.5), "30 min");
    CHECK_EQ_STR(AiAgent::formatHours(8.0 + 20.0 / 60), "8h 20m");
    CHECK_EQ_STR(AiAgent::formatHours(24), "24h");
    CHECK_EQ_STR(AiAgent::formatHours(49), "2d 1h");
    CHECK_EQ_STR(AiAgent::formatHours(-3), "3h");

    auto c = makeComplaint("ELECTRICITY", "Exposed wire hanging near the road", 4, 4);   // SLA 12h
    const std::time_t t0 = c->createdAt();
    SlaClock s1 = AiAgent::slaClock(*c, t0 + 6 * 3600);
    CHECK(s1.state == SlaState::ON_TRACK && s1.slaHours == 12 && s1.usedPct > 49.9 && s1.usedPct < 50.1);
    SlaClock s2 = AiAgent::slaClock(*c, t0 + 10 * 3600);
    CHECK(s2.state == SlaState::AT_RISK);
    SlaClock s3 = AiAgent::slaClock(*c, t0 + 13 * 3600);
    CHECK(s3.state == SlaState::BREACHED);
    CHECK_EQ_STR(s3.text, "Overdue by 1h (target 12h)");

    // priority never goes below the severity score and grows with waiting time
    int p0 = AiAgent::priorityScore(*c, s1, 0, 0);
    int p1 = AiAgent::priorityScore(*c, s3, 0, 0);
    CHECK(p0 >= c->severityScore());
    CHECK(p1 > p0);
    CHECK(AiAgent::priorityScore(*c, s1, 100, 0) <= 100);                       // capped
    CHECK(AiAgent::priorityScore(*c, s1, 10, 8) > AiAgent::priorityScore(*c, s1, 10, 0));   // hotspot raises it
    auto urgent = makeComplaint("ELECTRICITY", "Exposed wire hanging near the road", 4, 4, Priority::URGENT);
    CHECK(AiAgent::priorityScore(*urgent, s1, 0, 0) > p0);

    // labels follow the same bands as the severity levels
    CHECK_EQ_STR(AiAgent::priorityLabel(85, Status::PENDING), "Critical");
    CHECK_EQ_STR(AiAgent::priorityLabel(65, Status::ASSIGNED), "Urgent");
    CHECK_EQ_STR(AiAgent::priorityLabel(45, Status::IN_PROGRESS), "Elevated");
    CHECK_EQ_STR(AiAgent::priorityLabel(10, Status::PENDING), "Routine");
    CHECK_EQ_STR(AiAgent::priorityLabel(99, Status::RESOLVED), "Verify");
    CHECK_EQ_STR(AiAgent::priorityLabel(99, Status::CLOSED), "Closed");

    // a CRITICAL severity complaint never reads as Routine
    auto crit = makeComplaint("ELECTRICITY", "Sparking transformer", 5, 5);
    CHECK(crit->severityLevel() == SeverityLevel::CRITICAL);
    CHECK_EQ_STR(AiAgent::priorityLabel(AiAgent::priorityScore(*crit, AiAgent::slaClock(*crit, crit->createdAt()), 0, 0), crit->status()), "Critical");

    // lifecycle: resolved / closed
    c->assignDepartment(1, "Power");
    c->transitionTo(Status::IN_PROGRESS);
    c->transitionTo(Status::RESOLVED);
    SlaClock done = AiAgent::slaClock(*c, t0 + 100 * 3600);
    CHECK(done.state == SlaState::DONE);
    CHECK(AiAgent::priorityScore(*c, done, 50, 9) <= 25);
    c->transitionTo(Status::CLOSED);
    CHECK(AiAgent::priorityScore(*c, AiAgent::slaClock(*c, t0), 50, 9) == 0);
}

static void testIntents() {
    std::printf("intents\n");
    struct I { const char* msg; Intent want; };
    const I cases[] = {
        {"how can i use this ai", Intent::HELP}, {"how to use this", Intent::HELP}, {"hi", Intent::GREETING}, {"Hello there", Intent::GREETING}, {"thanks a lot", Intent::THANKS}, {"what can you do?", Intent::HELP},
        {"What should I handle first?", Intent::PRIORITIES}, {"Which of my complaints needs attention?", Intent::PRIORITIES},
        {"Summarise my workload", Intent::WORKLOAD}, {"What should I do next?", Intent::NEXT_STEP}, {"What happens next?", Intent::NEXT_STEP},
        {"how to fix this", Intent::NEXT_STEP}, {"How long will it take?", Intent::TIMELINE}, {"How much time is left?", Intent::TIMELINE},
        {"How do I escalate?", Intent::ESCALATE}, {"nobody has responded, it is overdue", Intent::ESCALATE}, {"Is it safe?", Intent::SAFETY},
        {"Draft a note for the citizen", Intent::DRAFT_NOTE}, {"Any similar complaints?", Intent::HOTSPOT},
        {"Which cities have the most complaints?", Intent::HOTSPOT}, {"How is severity calculated?", Intent::EXPLAIN_SEVERITY},
        {"What do the statuses mean?", Intent::EXPLAIN_STATUS}, {"How do I write a good complaint?", Intent::HOW_TO_REPORT},
        {"how are complaints assigned to a department", Intent::EXPLAIN_QUEUE}, {"Can I close it now?", Intent::CLOSE},
        {"someone is injured, help", Intent::EMERGENCY}, {"there is a fire near the transformer", Intent::EMERGENCY},
        {"status of my complaint", Intent::STATUS}, {"purple monkey dishwasher", Intent::UNKNOWN}, {"", Intent::UNKNOWN},
    };
    for (const auto& c : cases) {
        Intent got = AiAgent::detectIntent(c.msg);
        if (got != c.want) std::printf("  \"%s\": got %s, wanted %s\n", c.msg, toString(got), toString(c.want));
        CHECK(got == c.want);
    }

    // every suggestion chip offered by the assistant must be understood when the user clicks it
    const char* chipText[] = {"What happens next?", "Is it safe?", "How long will it take?", "How do I escalate?", "What should I do next?",
                              "Draft a note for the citizen", "How much time is left?", "Any similar complaints?",
                              "Which of my complaints needs attention?", "How do I write a good complaint?", "What do the statuses mean?",
                              "How is severity calculated?", "What should I handle first?", "Summarise my workload", "Which cities have the most complaints?"};
    for (const char* t : chipText) CHECK(AiAgent::detectIntent(t) != Intent::UNKNOWN);

    CHECK_EQ_STR(AiAgent::extractReference("what is the status of scms-2026-000012 please"), "SCMS-2026-000012");
    CHECK_EQ_STR(AiAgent::extractReference("SCMS-2026-1234567"), "SCMS-2026-1234567");
    CHECK_EQ_STR(AiAgent::extractReference("SCMS-26-12"), "");
    CHECK_EQ_STR(AiAgent::extractReference("no reference here"), "");
}

static bool anyLine(const Lines& l, LineKind k, const std::string& needle = "") {
    for (const auto& x : l)
        if (x.kind == k && x.text.find(needle) != std::string::npos) return true;
    return false;
}

static void testInsights() {
    std::printf("insights (department and citizen view)\n");
    std::unique_ptr<User> dept = makeUser(Role::DEPARTMENT), cit = makeUser(Role::CITIZEN);
    auto c = makeComplaint("WATER", "No water supply in the whole colony for three days, children and elderly are suffering", 4, 4);
    const std::time_t now = c->createdAt() + 30 * 3600;   // 30h old, SLA 24h -> breached
    c->assignDepartment(1, "Water Supply");

    Insight d = AiAgent::buildInsight(*dept, *c, 5, now);
    CHECK(d.playbook && d.playbook->id == "WATER_NO_SUPPLY");
    CHECK(d.sla.state == SlaState::BREACHED);
    CHECK(d.hotspot && d.similarOpen == 5);
    CHECK(d.priority >= 60);                                   // breached + vulnerable + hotspot
    CHECK(anyLine(d.plan.lines, LineKind::STEP));
    CHECK(anyLine(d.plan.lines, LineKind::HEADING, "Start now"));
    CHECK(anyLine(d.outlook.lines, LineKind::WARNING, "SLA missed"));
    CHECK(anyLine(d.outlook.lines, LineKind::WARNING, "5 other open Water"));
    CHECK(!d.suggestedNote.empty() && d.suggestedNote.size() <= 200);

    c->transitionTo(Status::IN_PROGRESS);
    Insight d2 = AiAgent::buildInsight(*dept, *c, 0, now);
    CHECK(anyLine(d2.plan.lines, LineKind::HEADING, "Before you mark it Resolved"));
    CHECK(d2.suggestedNote.find("Please check on your side") != std::string::npos && d2.suggestedNote.size() <= 200);

    Insight ci = AiAgent::buildInsight(*cit, *c, 5, now);
    CHECK(ci.plan.title == "What happens next");
    CHECK(anyLine(ci.outlook.lines, LineKind::WARNING, "-hour target passed"));
    CHECK(anyLine(ci.outlook.lines, LineKind::BULLET, c->referenceCode()) || anyLine(ci.outlook.lines, LineKind::BULLET, "reference"));
    CHECK(ci.suggestedNote.empty());                           // nothing to confirm yet
    CHECK(ci.headline.find("This looks like") == 0);

    c->transitionTo(Status::RESOLVED);
    Insight cr = AiAgent::buildInsight(*cit, *c, 0, now);
    CHECK(!cr.suggestedNote.empty());                          // citizen can confirm
    CHECK(cr.priorityLabel == "Verify" && cr.priority <= 25);
    CHECK(cr.similarOpen == 0 && !cr.hotspot);                 // closed work is never a hotspot

    // emergency playbooks warn the citizen
    auto wire = makeComplaint("ELECTRICITY", "Exposed live wire hanging low", 5, 5);
    Insight w = AiAgent::buildInsight(*cit, *wire, 0, wire->createdAt());
    CHECK(anyLine(w.support.lines, LineKind::WARNING, "112"));
    Insight wd = AiAgent::buildInsight(*dept, *wire, 0, wire->createdAt());
    CHECK(anyLine(wd.outlook.lines, LineKind::WARNING, "Safety-critical"));

    // every category yields a complete insight even for unclear text
    const char* cats[] = {"TRAFFIC", "GARBAGE", "ELECTRICITY", "WATER", "ROADS", "DRAINAGE"};
    for (const char* cat : cats) {
        auto g = makeComplaint(cat, "something is wrong here please help", 3, 3);
        Insight i = AiAgent::buildInsight(*dept, *g, 0, g->createdAt() + 3600);
        CHECK(i.playbook && i.playbook->general);
        CHECK(!i.plan.lines.empty() && !i.support.lines.empty());
        Insight ic = AiAgent::buildInsight(*cit, *g, 0, g->createdAt() + 3600);
        CHECK(!ic.plan.lines.empty());
    }
}

static void testKnowledgeBase() {
    std::printf("knowledge base integrity\n");
    const char* cats[] = {"TRAFFIC", "GARBAGE", "ELECTRICITY", "WATER", "ROADS", "DRAINAGE"};
    for (const char* cat : cats) CHECK(generalPlaybook(cat) != nullptr);
    CHECK(playbooks().size() >= 29);
    for (const auto& p : playbooks()) {
        CHECK(!p.id.empty() && !p.title.empty() && !p.explanation.empty());
        CHECK(!p.immediate.empty() && !p.fieldWork.empty() && !p.verify.empty() && !p.resources.empty());
        CHECK(!p.citizenSafety.empty() && !p.citizenTips.empty());
        CHECK(!p.resolvedNote.empty() && p.resolvedNote.size() <= 120);   // leaves room for "Please check on your side and confirm."
        CHECK(p.defaultImpact >= 1 && p.defaultImpact <= 5 && p.defaultUrgency >= 1 && p.defaultUrgency <= 5);
        if (!p.general) CHECK(p.keywords.size() >= 5);
        for (const auto& k : p.keywords) {   // lower-case words; only '-', '*' (prefix) and a leading '!' (distinctive) allowed
            bool ok = !k.empty();
            for (char ch : k) ok = ok && (std::islower(static_cast<unsigned char>(ch)) || std::isdigit(static_cast<unsigned char>(ch)) || ch == ' ' || ch == '-' || ch == '*' || ch == '!');
            CHECK(ok);
        }
        CHECK(playbookById(p.id) == &p);
    }
}

int main() {
    std::printf("SmartCity AI assistant - unit tests (assembly engine: %s)\n\n", AsmEngine::selfTest() ? "online" : "FAULT");
    testMatching();
    testClassification();
    testHeldOut();
    testFresh();
    testCues();
    testClockAndPriority();
    testIntents();
    testInsights();
    testKnowledgeBase();
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
