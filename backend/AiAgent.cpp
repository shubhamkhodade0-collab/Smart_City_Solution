// =====================================================================
// AiAgent.cpp - SmartCity AI assistant (see AiAgent.h for the overview)
//
// Layout of this file
//   1. text utilities          - normalise, hasWord (keyword matching)
//   2. readText()              - understand free text: sub-issue, danger cues, duration, suggested impact/urgency
//   3. slaClock / priority     - how long it has waited and how urgent it is (explainable, 0..100)
//   4. detectIntent()          - what a chat message is asking for
// The rest of the agent lives in AiInsight.cpp (advice for a complaint, triage) and AiChat.cpp (conversation).
// =====================================================================
#include "AiAgent.h"
#include "AiUtil.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

namespace scms {
namespace ai {

using namespace util;

namespace {

int wordCount(const std::string& normalised) {
    int n = 0;
    bool in = false;
    for (char c : normalised) {
        if (c == ' ') in = false;
        else if (!in) { in = true; ++n; }
    }
    return n;
}

// 1 word = 2 points, 2 words = 5, 3+ words = 8: longer phrases are stronger evidence.
// A leading '!' marks a distinctive keyword (a named asset such as "transformer" or "manhole" says more
// than a symptom such as "no power") and doubles its weight.
int phraseWeight(const std::string& keyword) {
    int n = wordCount(AiAgent::normalise(keyword));
    int w = n <= 1 ? 2 : (n == 2 ? 5 : 8);
    return !keyword.empty() && keyword[0] == '!' ? w * 2 : w;
}

bool anyWord(const std::string& text, const std::vector<std::string>& list, std::string* firstMatch = nullptr) {
    for (const auto& k : list)
        if (AiAgent::hasWord(text, k, firstMatch)) return true;
    return false;
}

int countWords(const std::string& text, const std::vector<std::string>& list) {
    int n = 0;
    for (const auto& k : list)
        if (AiAgent::hasWord(text, k)) ++n;
    return n;
}

// category anchors: words that point at a category even when no playbook keyword matches
const std::map<std::string, std::vector<std::string>>& anchors() {
    static const std::map<std::string, std::vector<std::string>> a = {
        {"TRAFFIC", {"traffic", "signal", "junction", "vehicle", "parking", "parked", "jam", "congestion", "warden", "zebra", "red light", "flyover", "constable"}},
        {"GARBAGE", {"garbage", "waste", "trash", "rubbish", "bin", "litter", "dump", "sweep", "collection", "debris", "dead animal", "dead dog", "carcass", "dustbin", "dumpster", "rubble"}},
        {"ELECTRICITY", {"electric*", "power", "wire", "voltage", "transformer", "current", "street light", "streetlight*", "meter", "pole", "shock"}},
        {"WATER", {"water", "tap", "pipe", "pipeline", "tanker", "sewer", "sewage", "drinking"}},
        {"ROADS", {"road", "pothole", "footpath", "pavement", "bridge", "asphalt", "highway", "culvert"}},
        {"DRAINAGE", {"drain", "drainage", "gutter", "manhole", "waterlog*", "flood*", "stagnant", "nala", "rain water", "standing water"}},
    };
    return a;
}

const std::vector<std::string> kVulnerable = {"child*", "kid", "school*", "student*", "elderly", "senior*", "old age", "hospital*", "clinic*", "patient*",
                                              "pregnan*", "disabled", "handicap*", "anganwadi", "infant*", "baby", "babies", "creche"};
const std::vector<std::string> kHazard = {"accident*", "injur*", "died", "death*", "fatal*", "fell", "fall into", "falling", "shock*", "electrocut*", "fire",
                                          "flames", "spark*", "collaps*", "bleeding", "drown*", "trapped", "explosion", "blast"};
const std::vector<std::string> kHealth = {"mosquito*", "dengue", "malaria", "disease*", "illness", "sick", "diarrh*", "cholera", "typhoid", "jaundice",
                                          "infection*", "stench", "rats", "vomit*", "epidemic", "outbreak", "stink*"};
const std::vector<std::string> kScale = {"whole", "entire", "colony", "society", "sector", "locality", "residents", "neighbourhood", "neighborhood",
                                         "hundreds", "thousands", "everyone", "families", "households", "all houses", "many people", "apartment*"};
const std::vector<std::string> kRepeat = {"again", "repeatedly", "repeated", "multiple times", "many times", "already complained", "no action", "ignored",
                                          "still not", "still no", "no response", "nobody came", "reminder", "complained before", "not resolved"};

std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

int numberWord(const std::string& w) {
    static const std::map<std::string, int> m = {{"one", 1}, {"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}, {"six", 6}, {"seven", 7},
                                                 {"eight", 8}, {"nine", 9}, {"ten", 10}, {"eleven", 11}, {"twelve", 12}, {"a", 1}, {"an", 1},
                                                 {"couple", 2}, {"few", 3}, {"several", 4}};
    auto it = m.find(w);
    if (it != m.end()) return it->second;
    if (!w.empty() && std::all_of(w.begin(), w.end(), [](char c) { return std::isdigit(uc(c)) != 0; }) && w.size() <= 4) return std::stoi(w);
    return -1;
}

// "for three days", "since 2 weeks", "since last week", "since yesterday" -> approximate days (0 = unknown)
int parseDurationDays(const std::string& norm) {
    std::vector<std::string> t = splitWords(norm);
    int best = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const std::string& w = t[i];
        double unit = 0;
        if (w.rfind("hour", 0) == 0 || w == "hrs" || w == "hr") unit = 1.0 / 24;
        else if (w == "day" || w == "days") unit = 1;
        else if (w == "week" || w == "weeks") unit = 7;
        else if (w == "month" || w == "months") unit = 30;
        else if (w == "year" || w == "years") unit = 365;
        if (unit > 0 && i >= 1) {
            int n = numberWord(t[i - 1]);
            if (n > 0) best = std::max(best, static_cast<int>(n * unit));
        }
        if (w == "since" && i + 1 < t.size()) {
            if (t[i + 1] == "yesterday") best = std::max(best, 1);
            if (t[i + 1] == "last" && i + 2 < t.size()) {
                if (t[i + 2] == "week") best = std::max(best, 7);
                else if (t[i + 2] == "month") best = std::max(best, 30);
                else if (t[i + 2] == "year") best = std::max(best, 365);
            }
        }
    }
    return best;
}

// "50 families", "20 houses" -> true when the number is large enough to mean "many"
bool bigNumberOfPeople(const std::string& norm) {
    static const std::vector<std::string> units = {"families", "houses", "households", "homes", "residents", "shops", "people", "buildings", "flats"};
    std::vector<std::string> t = splitWords(norm);
    for (std::size_t i = 1; i < t.size(); ++i) {
        if (std::find(units.begin(), units.end(), t[i]) == units.end()) continue;
        int n = numberWord(t[i - 1]);
        if (n >= 10) return true;
    }
    return false;
}

} // namespace

// =====================================================================
// 1. text utilities
// =====================================================================
std::string AiAgent::normalise(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool space = true;
    for (char ch : text) {
        unsigned char c = uc(ch);
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
            space = false;
        } else if (!space) {
            out.push_back(' ');
            space = true;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Keyword matching on already-normalised text.
//  - a keyword must start at a word boundary;
//  - keywords of 5+ letters (or ending in '*') also match longer forms: "pothole" -> "potholes";
//  - short keywords must match the whole word (a trailing s / es is tolerated): "pit" does not match "pitch".
bool AiAgent::hasWord(const std::string& text, const std::string& keyword, std::string* matched) {
    std::string kw = keyword;
    bool prefix = false;
    if (!kw.empty() && kw.back() == '*') {
        prefix = true;
        kw.pop_back();
    }
    kw = normalise(kw);
    if (kw.empty() || text.empty()) return false;
    if (kw.size() >= 5) prefix = true;
    std::size_t pos = 0;
    while ((pos = text.find(kw, pos)) != std::string::npos) {
        bool left = pos == 0 || !std::isalnum(uc(text[pos - 1]));
        std::size_t end = pos + kw.size();
        bool right = prefix || end >= text.size() || !std::isalnum(uc(text[end]));
        if (!right) {   // plural: "dogs", "bins", "wires"
            if (text[end] == 's' && (end + 1 >= text.size() || !std::isalnum(uc(text[end + 1])))) right = true;
            else if (text.compare(end, 2, "es") == 0 && (end + 2 >= text.size() || !std::isalnum(uc(text[end + 2])))) right = true;
        }
        if (left && right) {
            if (matched) {
                *matched = keyword;
                if (!matched->empty() && matched->back() == '*') matched->pop_back();
            }
            return true;
        }
        ++pos;
    }
    return false;
}

std::string AiAgent::formatHours(double hours) {
    if (hours < 0) hours = -hours;
    if (hours < 1.0) {
        int m = std::max(1, static_cast<int>(std::lround(hours * 60)));
        return std::to_string(m) + " min";
    }
    if (hours < 48.0) {
        int h = static_cast<int>(hours);
        int m = static_cast<int>(std::lround((hours - h) * 60));
        if (m == 60) { ++h; m = 0; }
        return m == 0 ? std::to_string(h) + "h" : std::to_string(h) + "h " + std::to_string(m) + "m";
    }
    int d = static_cast<int>(hours / 24);
    int h = static_cast<int>(std::lround(hours - d * 24.0));
    if (h == 24) { ++d; h = 0; }
    return h == 0 ? std::to_string(d) + "d" : std::to_string(d) + "d " + std::to_string(h) + "h";
}

// =====================================================================
// 2. readText - what does this text describe?
// =====================================================================
TextReading AiAgent::readText(const std::string& text, const std::string& categoryCode, const std::string& locality) {
    TextReading r;
    const std::string norm = normalise(text);

    // ---- which playbook? ---------------------------------------------------------------------
    const Playbook* best = nullptr;
    int bestScore = 0, second = 0;
    if (!norm.empty()) {
        const auto& anc = anchors();
        std::map<std::string, int> anchorBonus;
        for (const auto& kv : anc) anchorBonus[kv.first] = std::min(3, countWords(norm, kv.second)) * 2;
        for (const auto& p : playbooks()) {
            if (p.general) continue;
            if (!categoryCode.empty() && p.category != categoryCode) continue;
            // category unknown (Report screen): only consider categories whose anchor words appear in the text
            if (categoryCode.empty() && anchorBonus[p.category] == 0) continue;
            int score = 0;
            for (const auto& k : p.keywords)
                if (hasWord(norm, k)) score += phraseWeight(k);
            if (score > 0) score += anchorBonus[p.category];
            if (score > bestScore) { second = bestScore; bestScore = score; best = &p; }
            else if (score > second) second = score;
        }
    }
    if (best) {
        r.playbook = best;
        r.confidence = std::max(25, std::min(96, 40 + 6 * bestScore - 4 * second));
    } else if (!categoryCode.empty()) {
        r.playbook = generalPlaybook(categoryCode);   // category known, sub-issue unclear -> general guidance
        r.confidence = 0;
    }

    // ---- danger / context cues ----------------------------------------------------------------
    std::string hit;
    bool vulnerable = anyWord(norm, kVulnerable, &hit);
    std::string vulnHit = hit;
    bool hazard = anyWord(norm, kHazard, &hit);
    std::string hazHit = hit;
    bool health = anyWord(norm, kHealth, &hit);
    std::string healthHit = hit;
    bool scale = anyWord(norm, kScale, &hit) || bigNumberOfPeople(norm);
    r.repeatReport = anyWord(norm, kRepeat);
    r.durationDays = parseDurationDays(norm);
    r.hazard = hazard;

    if (vulnerable) r.signals.push_back({"mentions vulnerable people (" + vulnHit + ")", 5});
    if (hazard) r.signals.push_back({"mentions a danger cue (" + hazHit + ")", 6});
    if (health) r.signals.push_back({"mentions a health risk (" + healthHit + ")", 4});
    if (scale) r.signals.push_back({"many people or a whole area are affected", 3});
    if (r.repeatReport) r.signals.push_back({"the citizen says it was reported or ignored before", 3});
    if (r.durationDays >= 30) r.signals.push_back({"unresolved for a month or more", 6});
    else if (r.durationDays >= 7) r.signals.push_back({"unresolved for about a week or more", 4});
    else if (r.durationDays >= 3) r.signals.push_back({"unresolved for " + std::to_string(r.durationDays) + " days", 2});

    // ---- suggested impact / urgency (1..5) ---------------------------------------------------
    if (!norm.empty()) {
        int imp = r.playbook ? r.playbook->defaultImpact : 3;
        int urg = r.playbook ? r.playbook->defaultUrgency : 3;
        if (r.playbook && !r.playbook->general)
            r.impactWhy.push_back("typical for \"" + r.playbook->title + "\" is impact " + std::to_string(r.playbook->defaultImpact));
        if (vulnerable) { ++imp; r.impactWhy.push_back("children, a school, a hospital or elderly people are mentioned"); }
        if (scale) { ++imp; r.impactWhy.push_back("many people or a whole area are affected"); }
        if (r.playbook && !r.playbook->general)
            r.urgencyWhy.push_back("typical for \"" + r.playbook->title + "\" is urgency " + std::to_string(r.playbook->defaultUrgency));
        if (hazard) { ++urg; r.urgencyWhy.push_back("an injury, fire, shock or collapse risk is mentioned"); }
        if (health) { ++urg; r.urgencyWhy.push_back("there is a health risk"); }
        if (r.durationDays >= 3) { ++urg; r.urgencyWhy.push_back("the problem has already lasted about " + std::to_string(r.durationDays) + " days"); }
        r.impact = std::max(1, std::min(5, imp));
        r.urgency = std::max(1, std::min(5, urg));
    }

    // ---- details worth adding -----------------------------------------------------------------
    if (!norm.empty()) {
        if (r.durationDays == 0 && !anyWord(norm, {"since", "yesterday", "today", "morning", "evening", "daily", "every"}))
            r.missing.push_back("Say since when the problem has existed, for example \"since 3 days\".");
        if (!scale && !anyWord(norm, {"my house", "our house", "my home", "my shop", "one house"}))
            r.missing.push_back("Say how many people, houses or shops are affected.");
        if (trimCopy(locality).size() < 6)
            r.missing.push_back("Add a street name or landmark in the Locality box so the crew can find the spot.");
        if (norm.size() < 40)
            r.missing.push_back("Add a little more detail about what exactly is wrong.");
        if (r.missing.size() > 3) r.missing.resize(3);
    }
    return r;
}

Lines AiAgent::draftAdvice(const TextReading& r) {
    Lines out;
    if (r.playbook && !r.playbook->general) {
        for (const auto& s : r.playbook->citizenSafety) out.push_back({LineKind::TIP, s});
    }
    return out;
}

// =====================================================================
// 3. SLA clock and priority
// =====================================================================
SlaClock AiAgent::slaClock(const Complaint& c, std::time_t now) {
    SlaClock k;
    k.slaHours = c.slaHours();
    k.ageHours = std::max(0.0, std::difftime(now, c.createdAt()) / 3600.0);
    if (c.status() == Status::RESOLVED || c.status() == Status::CLOSED) {
        k.state = SlaState::DONE;
        k.usedPct = 0;
        k.hoursLeft = 0;
        k.text = c.status() == Status::CLOSED ? "Closed" : "Completed - waiting for the citizen to confirm";
        return k;
    }
    k.usedPct = k.slaHours > 0 ? k.ageHours / k.slaHours * 100.0 : 0.0;
    k.hoursLeft = k.slaHours - k.ageHours;
    if (k.usedPct >= 100.0) {
        k.state = SlaState::BREACHED;
        k.text = "Overdue by " + formatHours(-k.hoursLeft) + " (target " + std::to_string(k.slaHours) + "h)";
    } else {
        k.state = k.usedPct >= 75.0 ? SlaState::AT_RISK : SlaState::ON_TRACK;
        k.text = formatHours(k.hoursLeft) + " left of " + std::to_string(k.slaHours) + "h target";
    }
    return k;
}

double AiAgent::slaBoost(const SlaClock& sla) {
    if (sla.state == SlaState::DONE) return 0;
    if (sla.usedPct >= 100.0) return 12 + std::min(8.0, (sla.usedPct - 100.0) / 100.0 * 8.0);
    if (sla.usedPct > 50.0) return (sla.usedPct - 50.0) * 0.24;
    return 0;
}

// AI priority = NASM severity score + boosts, so it can never contradict the severity band:
//   + SLA pressure (0..20)  + risk cues in the text (max 15)  + similar open complaints (3 / 6)  +/- the citizen's priority field
int AiAgent::priorityScore(const Complaint& c, const SlaClock& sla, int signalPoints, int similarOpen) {
    if (c.status() == Status::CLOSED) return 0;
    double s = c.severityScore();
    double boost = slaBoost(sla);
    boost += std::min(15, std::max(0, signalPoints));
    if (similarOpen >= 8) boost += 6;
    else if (similarOpen >= 3) boost += 3;
    switch (c.priority()) {
        case Priority::URGENT: boost += 5; break;
        case Priority::HIGH: boost += 3; break;
        case Priority::LOW: boost -= 3; break;
        default: break;
    }
    int p = static_cast<int>(std::lround(s + boost));
    p = std::max(0, std::min(100, p));
    if (c.status() == Status::RESOLVED) p = std::min(p, 25);
    return p;
}

const char* AiAgent::priorityLabel(int score, Status status) {
    if (status == Status::CLOSED) return "Closed";
    if (status == Status::RESOLVED) return "Verify";
    if (score >= 80) return "Critical";
    if (score >= 60) return "Urgent";
    if (score >= 40) return "Elevated";
    return "Routine";
}

// =====================================================================
// 4. intents
// =====================================================================
const char* toString(Intent i) {
    switch (i) {
        case Intent::GREETING: return "greeting";
        case Intent::THANKS: return "thanks";
        case Intent::HELP: return "help";
        case Intent::EMERGENCY: return "emergency";
        case Intent::PRIORITIES: return "priorities";
        case Intent::WORKLOAD: return "workload";
        case Intent::STATUS: return "status";
        case Intent::NEXT_STEP: return "next-step";
        case Intent::TIMELINE: return "timeline";
        case Intent::ESCALATE: return "escalate";
        case Intent::SAFETY: return "safety";
        case Intent::DRAFT_NOTE: return "draft-note";
        case Intent::HOTSPOT: return "hotspot";
        case Intent::CLOSE: return "close";
        case Intent::EXPLAIN_SEVERITY: return "explain-severity";
        case Intent::EXPLAIN_STATUS: return "explain-status";
        case Intent::EXPLAIN_QUEUE: return "explain-queue";
        case Intent::HOW_TO_REPORT: return "how-to-report";
        default: return "unknown";
    }
}

namespace {
struct IntentRule {
    Intent id;
    std::vector<std::string> kws;
};

// order = tie-break order (earlier wins on equal score)
const std::vector<IntentRule>& intentRules() {
    static const std::vector<IntentRule> rules = {
        {Intent::EXPLAIN_SEVERITY, {"severity", "how is severity", "calculated", "calculate", "score", "how does priority", "impact and urgency", "impact", "urgency", "nasm", "assembly"}},
        {Intent::EXPLAIN_STATUS, {"what do the statuses", "statuses mean", "status mean", "what does assigned", "what does pending", "what does in progress", "what does resolved", "what does closed", "lifecycle", "stages", "what do statuses"}},
        {Intent::EXPLAIN_QUEUE, {"queue", "fifo", "first in first out", "how are complaints assigned", "routing", "routed", "assigned to department", "which department", "who handles", "department handle"}},
        {Intent::HOW_TO_REPORT, {"how to report", "how do i report", "how to file", "how do i file", "how to submit", "write a good complaint", "good complaint", "improve my complaint", "what to include", "which category", "what category", "how to describe", "what should i write", "what to write", "report a complaint", "new complaint", "raise a complaint", "lodge"}},
        {Intent::DRAFT_NOTE, {"draft", "note", "write a", "write the", "message", "wording", "compose", "reply to", "text for", "update message", "comment"}},
        {Intent::NEXT_STEP, {"what next", "next step", "next steps", "what should i do", "what do i do", "how to solve", "how do i solve", "solution", "solve", "fix", "how to fix", "how to handle", "how do i handle", "what can i do", "suggest", "advice", "recommend*", "action plan", "plan", "steps", "guidance", "approach", "how to resolve", "next"}},
        {Intent::TIMELINE, {"how long", "when will", "eta", "deadline", "sla", "time left", "how much time", "timeline", "by when", "how soon", "expected time", "time frame", "timeframe", "turnaround", "target time"}},
        {Intent::ESCALATE, {"escalat*", "delay*", "overdue", "no response", "not responding", "ignored", "nobody", "higher authority", "complain further", "still not", "taking too long", "too long", "not fixed", "not resolved", "no action", "helpline", "grievance", "senior officer"}},
        {Intent::SAFETY, {"safe", "safety", "danger*", "risk", "hazard*", "unsafe", "until then", "while waiting", "precaution*", "protect", "stay away", "avoid", "harm"}},
        {Intent::HOTSPOT, {"hotspot*", "similar", "pattern", "repeat*", "other complaints", "same issue", "which city", "which cities", "cities", "city ranking", "most complaints", "trend", "recurring", "cluster", "area wide", "systemic", "common problem", "nearby"}},
        {Intent::CLOSE, {"close", "closing", "confirm", "verify", "satisfied", "mark resolved", "mark as resolved", "finish", "complete", "completed", "sign off", "checklist", "before resolving", "before closing"}},
        {Intent::PRIORITIES, {"priority", "priorities", "handle first", "work on first", "start with", "where to start", "most urgent", "most important", "urgent", "triage", "top complaints", "what should i handle", "which complaint", "which one first", "worst", "critical", "needs attention", "need attention", "attention", "first"}},
        {Intent::WORKLOAD, {"workload", "summary", "summarise", "summarize", "overview", "how many", "statistics", "stats", "my complaints", "all my", "my queue", "pending", "my work", "dashboard", "report"}},
        {Intent::STATUS, {"status", "status of", "what is the status", "progress", "where is my", "any update", "updates", "track", "what is happening", "what happened"}},
        {Intent::HELP, {"help", "what can you do", "how can you help", "capabilit*", "features", "assist", "what do you do", "who are you", "your name", "guide me", "menu", "options", "how can i use", "how do i use", "how to use", "use this ai", "what is this"}},
        {Intent::THANKS, {"thanks", "thank you", "thx", "great", "awesome", "helpful", "cool"}},
        {Intent::GREETING, {"hi", "hello", "hey", "good morning", "good afternoon", "good evening", "namaste", "hii"}},
    };
    return rules;
}

const std::vector<std::string>& emergencyWords() {
    static const std::vector<std::string> w = {"emergency", "injured", "bleeding", "trapped", "drown*", "electrocut*", "on fire", "caught fire", "fire", "explosion",
                                               "unconscious", "heart attack", "fell into", "someone fell", "ambulance", "life threatening", "dying"};
    return w;
}
} // namespace

Intent AiAgent::detectIntent(const std::string& message) {
    const std::string norm = normalise(message);
    if (norm.empty()) return Intent::UNKNOWN;
    if (anyWord(norm, emergencyWords())) return Intent::EMERGENCY;
    Intent best = Intent::UNKNOWN;
    int bestScore = 0;
    for (const auto& rule : intentRules()) {
        int score = 0;
        for (const auto& k : rule.kws)
            if (hasWord(norm, k)) score += phraseWeight(k);
        if (score > bestScore) { bestScore = score; best = rule.id; }
    }
    return best;
}

std::string AiAgent::extractReference(const std::string& message) {
    std::string low = lowerCopy(message);
    std::size_t p = low.find("scms-");
    while (p != std::string::npos) {
        std::size_t i = p + 5, y = i;
        while (i < low.size() && std::isdigit(uc(low[i]))) ++i;
        if (i - y == 4 && i < low.size() && low[i] == '-') {
            std::size_t n = ++i;
            while (i < low.size() && std::isdigit(uc(low[i]))) ++i;
            if (i - n >= 4) {
                std::string ref = message.substr(p, i - p);
                for (auto& ch : ref) ch = static_cast<char>(std::toupper(uc(ch)));
                return ref;
            }
        }
        p = low.find("scms-", p + 1);
    }
    return "";
}

} // namespace ai
} // namespace scms
