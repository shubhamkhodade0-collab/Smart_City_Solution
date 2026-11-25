// =====================================================================
// AiAgent.h - the SmartCity AI assistant (local, rule-based expert system)
//
// What it does
//   * reads a complaint (or free text typed by a citizen) and recognises WHAT kind of
//     problem it is (AiKnowledge playbooks), how DANGEROUS it is, and how LONG it has waited;
//   * scores every complaint with an explainable AI priority (0..100);
//   * suggests a solution plan for the department and a "what happens next / stay safe"
//     guide for the citizen;
//   * ranks a user's whole workload (triage) with the project's own Merge Sort;
//   * answers questions in a chat (intent detection + context of the open complaint).
//
// How it works - important for honesty
//   It is NOT a large language model. It is a deterministic expert system: keyword
//   scoring + the playbooks in AiKnowledge.cpp + live facts from PostgreSQL (status, SLA,
//   similar open complaints). It needs no internet, no API key and never sends data anywhere.
//
// Access rules
//   The agent only reads what the signed-in user may see (User::canView). Counts of
//   similar complaints in a city are aggregate numbers - no other citizen's details.
// =====================================================================
#pragma once
#include "AiKnowledge.h"
#include "GroqClient.h"
#include "ComplaintService.h"
#include "../dsa/Sorting.h"
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace scms {
namespace ai {

// ---------------------------------------------------------------- rich text returned to the UI
enum class LineKind { HEADING, TEXT, BULLET, STEP, WARNING, TIP, NOTE };   // NOTE = small grey footnote
struct Line {
    LineKind kind = LineKind::TEXT;
    std::string text;
};
typedef std::vector<Line> Lines;
struct Section {
    std::string title;
    Lines lines;
};

// ---------------------------------------------------------------- analysis results
enum class SlaState { ON_TRACK, AT_RISK, BREACHED, DONE };

struct SlaClock {
    double ageHours = 0;
    int slaHours = 0;
    double usedPct = 0;       // age / sla * 100 (open complaints only)
    double hoursLeft = 0;     // negative when overdue
    SlaState state = SlaState::ON_TRACK;
    std::string text;         // "8h 20m left of 24h target" / "Overdue by 3d 4h (target 24h)"
};

struct Signal {
    std::string text;         // human-readable evidence, e.g. "mentions children or a school"
    int points = 0;           // contribution to the AI priority
};

// what the agent understood from free text
struct TextReading {
    const Playbook* playbook = nullptr;   // never null when a category is known; may be null for unclear text
    int confidence = 0;                   // 0..100 (0 = fell back to the general playbook)
    int impact = 0, urgency = 0;          // suggested 1..5 (0 = nothing to suggest)
    std::vector<std::string> impactWhy, urgencyWhy;
    std::vector<Signal> signals;
    int durationDays = 0;
    bool hazard = false;                  // injury / fire / shock / collapse words found
    bool repeatReport = false;            // "again", "no action", "ignored" ...
    std::vector<std::string> missing;     // details worth adding (max 3)
};

struct Insight {
    const Playbook* playbook = nullptr;
    int confidence = 0;
    SlaClock sla;
    int priority = 0;
    std::string priorityLabel;            // Critical / Urgent / Elevated / Routine / Verify / Closed
    std::vector<Signal> signals;          // everything that moved the priority above the severity score
    int similarOpen = 0;                  // other open complaints, same city and category
    bool hotspot = false;
    std::string headline;
    Section plan, support, outlook;       // meaning depends on the audience (see AiAgent.cpp)
    std::string suggestedNote;            // ready-to-use note for the next status change (<= 200 chars)
    std::string noteLabel;                // what the note is for, e.g. "Note for the citizen"
};

struct TriageItem {
    long long id = 0;
    std::string reference, category, city, locality, reason;
    Status status = Status::PENDING;
    SeverityLevel severity = SeverityLevel::LOW;
    int severityScore = 0, priority = 0;
    std::string label;
    SlaState sla = SlaState::ON_TRACK;
    double hoursLeft = 0, ageHours = 0;
};

struct Triage {
    std::vector<TriageItem> items;        // highest priority first (Merge Sort)
    SortMetrics sort;
    int open = 0, overdue = 0, atRisk = 0, critical = 0;
};

struct Reply {
    Lines lines;
    std::vector<std::string> followUps;   // suggested next questions (clickable chips)
    long long focusId = -1;               // -1 keep the current focus, 0 clear it, >0 focus this complaint
    std::string intent;                   // for the tests / the UI badge
};

enum class Intent {
    UNKNOWN, GREETING, THANKS, HELP, EMERGENCY, PRIORITIES, WORKLOAD, STATUS, NEXT_STEP, TIMELINE, ESCALATE, SAFETY,
    DRAFT_NOTE, HOTSPOT, CLOSE, EXPLAIN_SEVERITY, EXPLAIN_STATUS, EXPLAIN_QUEUE, HOW_TO_REPORT
};
const char* toString(Intent i);

// ---------------------------------------------------------------- the agent
class AiAgent {
public:
    AiAgent(Database& db, ComplaintService& svc) : db_(db), svc_(svc) {}

    // ---- pure functions (no database): deterministic, unit-tested ----------------------------
    static std::string normalise(const std::string& text);        // lower-case, punctuation -> single spaces
    static bool hasWord(const std::string& normalisedText, const std::string& keyword, std::string* matched = nullptr);
    static TextReading readText(const std::string& text, const std::string& categoryCode = "",
                                const std::string& locality = "");
    static SlaClock slaClock(const Complaint& c, std::time_t now);
    static double slaBoost(const SlaClock& sla);                        // 0..20 points added to the priority
    static int priorityScore(const Complaint& c, const SlaClock& sla, int signalPoints, int similarOpen);
    static const char* priorityLabel(int score, Status status);
    static Intent detectIntent(const std::string& message);
    static std::string extractReference(const std::string& message);   // "SCMS-2026-000123" or ""
    static std::string formatHours(double hours);                       // "45 min", "8h 20m", "3d 4h"
    static Insight buildInsight(const User& viewer, const Complaint& c, int similarOpen, std::time_t now);
    // advice shown next to the "Report a complaint" form while the citizen types
    static Lines draftAdvice(const TextReading& r);
    // turns an answer written by the optional Groq model into typed lines (plain ASCII, simple markdown understood)
    static Lines formatLlmAnswer(const std::string& text);
    static std::string systemPrompt(bool citizen);
    static bool projectRelated(const std::string& message);   // about complaints / this application?
    // should Groq be asked? (key present AND (mode "every question" OR the built-in assistant did not understand))
    static bool wantsLlm(const Reply& builtIn, const LlmSettings& settings);
    // the reply to show once Groq has answered (or failed: then the built-in reply plus the reason)
    static Reply withLlmAnswer(const Reply& builtIn, const LlmResult& result, const LlmSettings& settings);

    // ---- database-aware ------------------------------------------------------------------------
    Insight analyse(const User& viewer, const Complaint& c);
    Triage triage(const User& viewer);
    // open complaints of any scope (e.g. one department), ranked by AI priority; includeResolved adds the ones waiting for the citizen
    Triage triageScope(const Scope& scope, bool includeResolved);
    Reply greeting(const User& viewer, int groqMode = -1);   // -1 = Groq off, 0 = built-in first, 1 = direct AI
    Reply chat(const User& viewer, const std::string& message, long long focusId);
    // Builds the request for the optional Groq model (reads the database; the network call itself is made elsewhere,
    // on a worker thread). Names, phone numbers and user names are never included.
    LlmRequest llmRequest(const User& viewer, const std::string& message, long long focusId, const std::vector<LlmMessage>& history,
                          const LlmSettings& settings, const Reply* builtIn);

private:
    std::unique_ptr<Complaint> loadVisible(const User& viewer, long long id);
    int similarOpenCount(const Complaint& c);
    std::vector<std::string> contextChips(const User& viewer, bool hasFocus);

    Database& db_;
    ComplaintService& svc_;
};

} // namespace ai
} // namespace scms
