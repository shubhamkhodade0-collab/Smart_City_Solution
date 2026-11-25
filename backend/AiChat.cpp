// =====================================================================
// AiChat.cpp - the conversation layer of the AI assistant
//
//   chat() = (1) find the complaint the user means (a reference in the message, or the one in focus)
//            (2) detect the intent of the message (AiAgent::detectIntent)
//            (3) build a structured reply from live facts + the playbooks
//
// The reply is a list of typed Lines (heading, text, bullet, step, warning, tip) that the OpenGL UI
// draws; it also carries clickable follow-up questions. Everything is computed locally.
// =====================================================================
#include "AiAgent.h"
#include "AiUtil.h"
#include <cmath>

namespace scms {
namespace ai {

using namespace util;

namespace {

void H(Lines& l, const std::string& t) { l.push_back({LineKind::HEADING, t}); }
void T(Lines& l, const std::string& t) { l.push_back({LineKind::TEXT, t}); }
void B(Lines& l, const std::string& t) { l.push_back({LineKind::BULLET, t}); }
void S(Lines& l, const std::string& t) { l.push_back({LineKind::STEP, t}); }
void W(Lines& l, const std::string& t) { l.push_back({LineKind::WARNING, t}); }
void Tip(Lines& l, const std::string& t) { l.push_back({LineKind::TIP, t}); }
void all(Lines& l, const Lines& more) { l.insert(l.end(), more.begin(), more.end()); }

bool isOpenStatus(Status s) { return s == Status::PENDING || s == Status::ASSIGNED || s == Status::IN_PROGRESS; }

const char* niceStatus(Status s) {
    switch (s) {
        case Status::PENDING: return "Pending";
        case Status::ASSIGNED: return "Assigned";
        case Status::IN_PROGRESS: return "In progress";
        case Status::RESOLVED: return "Resolved";
        case Status::CLOSED: return "Closed";
    }
    return "";
}

std::string focusTitle(const Complaint& c) {
    return c.referenceCode() + "  -  " + c.typeName() + ", " + c.location().city + "  (" + niceStatus(c.status()) + ")";
}

std::string firstName(const std::string& full) {
    std::string f = full.substr(0, full.find(' '));
    return f.empty() ? "there" : f;
}

std::string levelName(SeverityLevel l) {
    std::string s = toString(l);
    return capitalise(lowerCopy(s));
}

} // namespace

std::vector<std::string> AiAgent::contextChips(const User& viewer, bool hasFocus) {
    const bool citizen = viewer.role() == Role::CITIZEN;
    if (hasFocus && citizen) return {"What happens next?", "Is it safe?", "How long will it take?", "How do I escalate?"};
    if (hasFocus) return {"What should I do next?", "Draft a note for the citizen", "How much time is left?", "Any similar complaints?"};
    if (citizen) return {"Which of my complaints needs attention?", "How do I write a good complaint?", "What do the statuses mean?", "How is severity calculated?"};
    return {"What should I handle first?", "Summarise my workload", "Which cities have the most complaints?", "How is severity calculated?"};
}

Reply AiAgent::greeting(const User& viewer, int groqMode) {
    Reply r;
    r.intent = "greeting";
    const bool citizen = viewer.role() == Role::CITIZEN;
    T(r.lines, "Hello " + (citizen ? firstName(viewer.fullName()) : viewer.fullName()) + ", I am the SmartCity assistant.");
    if (citizen) {
        B(r.lines, "I can tell you where your complaints stand and what to expect next.");
        B(r.lines, "I can show how to stay safe while you wait and how to escalate if it is late.");
        B(r.lines, "On the Report screen I suggest the category, impact and urgency from your description.");
    } else {
        B(r.lines, "I can rank your open complaints by urgency (AI triage).");
        B(r.lines, "I can suggest a fix plan, the crew and checks needed, and draft the status note for the citizen.");
        B(r.lines, "I can spot repeat problems and the cities that need attention.");
    }
    if (groqMode == 1) Tip(r.lines, "Direct AI is on: ask me anything, not only about complaints. Your messages are sent to Groq with your own API key; complaint facts come from this computer.");
    else if (groqMode == 0) Tip(r.lines, "My built-in answers are worked out on this computer from the complaint data. Questions I cannot answer are sent to Groq with your own API key.");
    else Tip(r.lines, "I run on this computer from the complaint data in PostgreSQL. Nothing is sent over the internet.");
    r.followUps = contextChips(viewer, false);
    return r;
}

Reply AiAgent::chat(const User& viewer, const std::string& message, long long focusId) {
    Reply r;
    const bool citizen = viewer.role() == Role::CITIZEN;
    const std::string msg = trimCopy(message);
    if (msg.empty()) return greeting(viewer);

    // ---- (1) which complaint? ------------------------------------------------------------------
    std::unique_ptr<Complaint> focus;
    const std::string ref = extractReference(msg);
    if (!ref.empty()) {
        std::unique_ptr<Complaint> c;
        try { c = db_.getComplaintByReference(ref); } catch (const std::exception&) {}
        if (!c || !viewer.canView(*c)) {
            r.intent = "not-found";
            W(r.lines, "I could not find " + ref + (citizen ? " among your complaints." : " in your department's queue."));
            T(r.lines, "Check the reference number. It looks like SCMS-2026-000123 and is shown on the complaint.");
            r.followUps = contextChips(viewer, focusId > 0);
            return r;
        }
        focus = std::move(c);
        r.focusId = focus->id();
    } else if (focusId > 0) {
        focus = loadVisible(viewer, focusId);
    }

    // ---- (2) what is being asked? ---------------------------------------------------------------
    const Intent intent = detectIntent(msg);
    r.intent = toString(intent);
    const bool refOnly = !ref.empty() && intent == Intent::UNKNOWN;

    Insight in;
    bool haveInsight = false;
    auto insight = [&]() -> const Insight& {
        if (!haveInsight && focus) { in = analyse(viewer, *focus); haveInsight = true; }
        return in;
    };
    auto askWhich = [&]() {
        T(r.lines, "Which complaint do you mean? Type its reference (for example SCMS-2026-000123) or pick one of these.");
        Triage tri = triage(viewer);
        for (std::size_t i = 0; i < tri.items.size() && i < 3; ++i) r.followUps.push_back(tri.items[i].reference);
        if (tri.items.empty()) T(r.lines, "You have no open complaints at the moment.");
    };

    // ---- (3) the reply -------------------------------------------------------------------------
    if (refOnly) {   // just a reference: give the overview
        const Insight& i = insight();
        H(r.lines, focusTitle(*focus));
        T(r.lines, i.headline);
        if (!i.plan.lines.empty()) {
            H(r.lines, i.plan.title);
            all(r.lines, i.plan.lines);
        }
        r.followUps = contextChips(viewer, true);
        return r;
    }

    switch (intent) {
        case Intent::EMERGENCY: {
            W(r.lines, "If anyone is hurt or in immediate danger, call 112 now (fire: 101). Do not wait for a complaint to be processed.");
            T(r.lines, "Keep well away from fallen or sparking wires, open manholes, flooded roads and damaged structures, and keep children and animals back.");
            if (citizen) T(r.lines, "Once everyone is safe, report the problem here so the right department can act quickly.");
            else T(r.lines, "On site: make the spot safe first (barricade, isolate the utility), then repair.");
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                all(r.lines, citizen ? i.support.lines : i.plan.lines);
            }
            break;
        }
        case Intent::GREETING:
            return greeting(viewer);
        case Intent::THANKS:
            T(r.lines, "You are welcome. Ask me anything about a complaint, or pick a suggestion below.");
            break;
        case Intent::HELP: {
            H(r.lines, "What I can do");
            if (citizen) {
                B(r.lines, "Explain the status of a complaint and what happens next.");
                B(r.lines, "Tell you how long it should take and how to escalate if it is late.");
                B(r.lines, "Give safety advice while you wait.");
                B(r.lines, "Help you write a clearer complaint (use the helper on the Report screen).");
            } else {
                B(r.lines, "Rank your open complaints by urgency (priorities).");
                B(r.lines, "Suggest the fix plan, resources and checks for a complaint.");
                B(r.lines, "Draft the note for the citizen when you change a status.");
                B(r.lines, "Show repeat problems and the cities with the most complaints.");
            }
            Tip(r.lines, "Type a complaint reference such as SCMS-2026-000123 to talk about that complaint.");
            break;
        }
        case Intent::PRIORITIES: {
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                T(r.lines, "AI priority " + std::to_string(i.priority) + "/100 (" + i.priorityLabel + "). The NASM severity is " +
                               std::to_string(focus->severityScore()) + " (" + levelName(focus->severityLevel()) + ").");
                if (i.signals.empty()) T(r.lines, "Nothing in the text or the clock raises it above its severity score.");
                else {
                    H(r.lines, "What raised it");
                    for (const auto& s : i.signals) B(r.lines, s.text + " (+" + std::to_string(s.points) + ")");
                }
                T(r.lines, i.sla.text + ".");
            } else {
                Triage tri = triage(viewer);
                if (tri.items.empty()) {
                    T(r.lines, citizen ? "You have no open complaints right now." : "Nothing needs attention right now. No open complaints in your queue.");
                    break;
                }
                H(r.lines, citizen ? "Your complaints, most urgent first" : "Handle these first");
                for (std::size_t i = 0; i < tri.items.size() && i < 5; ++i) {
                    const TriageItem& t = tri.items[i];
                    S(r.lines, t.reference + "  " + t.category + ", " + t.city + "  -  " + t.label + " " + std::to_string(t.priority) + ": " + t.reason);
                    if (i < 3) r.followUps.push_back(t.reference);
                }
                T(r.lines, std::to_string(tri.open) + " open, " + std::to_string(tri.overdue) + " past the SLA, " + std::to_string(tri.atRisk) + " close to it, " +
                               std::to_string(tri.critical) + " critical.");
                Tip(r.lines, "Ranked by AI priority with Merge Sort (" + std::to_string(tri.sort.comparisons) + " comparisons for " + std::to_string(tri.sort.n) + " complaints).");
                return r;
            }
            break;
        }
        case Intent::WORKLOAD:
        case Intent::STATUS: {
            if (focus && intent == Intent::STATUS) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                double sinceUpdate = std::max(0.0, std::difftime(std::time(nullptr), focus->updatedAt()) / 3600.0);
                T(r.lines, std::string("Status: ") + niceStatus(focus->status()) + ". Last update " + formatHours(sinceUpdate) + " ago." +
                               (focus->departmentName().empty() ? "" : " Handled by " + focus->departmentName() + "."));
                T(r.lines, i.sla.text + ".");
                if (!i.plan.lines.empty()) all(r.lines, i.plan.lines);
                break;
            }
            Summary s = db_.getSummary(svc_.scopeFor(viewer));
            H(r.lines, citizen ? "Where your complaints stand" : "Your workload");
            T(r.lines, std::to_string(s.total) + " complaints: " + std::to_string(s.pending) + " pending, " + std::to_string(s.assigned) + " assigned, " +
                           std::to_string(s.inProgress) + " in progress, " + std::to_string(s.resolved) + " resolved, " + std::to_string(s.closed) + " closed.");
            Triage tri = triage(viewer);
            if (!citizen) B(r.lines, std::to_string(s.high) + " high or critical severity, " + std::to_string(tri.overdue) + " past the SLA, " +
                                         std::to_string(tri.atRisk) + " close to it.");
            else if (s.resolved > 0) Tip(r.lines, std::to_string(s.resolved) + " complaint(s) are marked resolved - please check them and confirm or report again.");
            if (!citizen) {
                CityRanking cr = svc_.cityRanking(true, 3, "", svc_.scopeFor(viewer));
                if (!cr.rows.empty()) {
                    std::string cities;
                    for (std::size_t i = 0; i < cr.rows.size(); ++i)
                        cities += (i ? ", " : "") + cr.rows[i].city + " (" + std::to_string(cr.rows[i].complaints) + ")";
                    B(r.lines, "Busiest cities: " + cities + ".");
                }
            }
            for (std::size_t i = 0; i < tri.items.size() && i < 3; ++i) r.followUps.push_back(tri.items[i].reference);
            if (r.followUps.empty()) r.followUps = contextChips(viewer, false);
            return r;
        }
        case Intent::NEXT_STEP: {
            if (!focus) { askWhich(); return r; }
            const Insight& i = insight();
            H(r.lines, focusTitle(*focus));
            T(r.lines, i.headline);
            H(r.lines, i.plan.title);
            all(r.lines, i.plan.lines);
            break;
        }
        case Intent::TIMELINE: {
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                T(r.lines, "SLA: " + i.sla.text + ".");
                if (i.sla.state == SlaState::BREACHED) T(r.lines, citizen ? "It is overdue. See how to escalate below." : "It is overdue - post an update for the citizen and tell your supervisor.");
                else if (focus->status() == Status::PENDING) T(r.lines, "The clock started when it was reported. Assignment normally happens first, then a crew visit.");
                all(r.lines, i.outlook.lines);
            } else {
                H(r.lines, "Resolution targets (SLA) by category");
                for (const auto& cat : svc_.categories()) B(r.lines, cat.name + ": " + std::to_string(cat.slaHours) + " hours");
                Tip(r.lines, "The clock starts when a complaint is reported and stops when it is marked Resolved.");
            }
            break;
        }
        case Intent::ESCALATE: {
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                if (citizen && i.sla.state != SlaState::BREACHED && isOpenStatus(focus->status()))
                    T(r.lines, "It is not overdue yet (" + i.sla.text + "). Escalating now is unlikely to speed it up - the guidance below applies once the target passes.");
                all(r.lines, i.outlook.lines);
            } else if (citizen) {
                H(r.lines, "If a complaint is late");
                S(r.lines, "Wait until the target time for its category has passed (ask me about the SLA).");
                S(r.lines, "Contact the department's helpline or the ward office and quote the reference number.");
                S(r.lines, "Ask your ward representative or the municipal control room to follow up.");
                S(r.lines, "Use your state's public grievance portal, or CPGRAMS, with the same reference.");
            } else {
                H(r.lines, "When to escalate");
                B(r.lines, "The SLA has been missed or will be missed (check AI priority).");
                B(r.lines, "The problem is a safety risk and the crew or material is blocked.");
                B(r.lines, "Many complaints of one kind pile up in one city (a repeat pattern).");
                Tip(r.lines, "Tell your supervisor, then post a note so the citizen sees what is happening.");
            }
            break;
        }
        case Intent::SAFETY: {
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                if (citizen && !isOpenStatus(focus->status())) {
                    T(r.lines, "This complaint is already marked as fixed, so no special precautions apply. If the problem has come back, report it again and quote " + focus->referenceCode() + ".");
                } else if (citizen) {
                    const Playbook* pb = i.playbook;
                    if (pb && !pb->citizenSafety.empty()) for (const auto& s : pb->citizenSafety) (pb->emergency ? W : Tip)(r.lines, s);
                    else T(r.lines, "Nothing specific for this kind of problem. Take normal care around it.");
                    if (pb && pb->emergency) W(r.lines, "If anyone is in danger, call 112 first.");
                } else {
                    T(r.lines, "Make the area safe before repair work:");
                    B(r.lines, "Barricade the spot and put up warning signs or cones.");
                    B(r.lines, "Isolate or switch off the utility involved (power feeder, water valve) before touching it.");
                    B(r.lines, "Use the right protective gear; insulated tools for any electrical work.");
                    B(r.lines, "Keep the public away until the check is complete.");
                    if (i.playbook && !i.playbook->immediate.empty()) Tip(r.lines, i.playbook->immediate[0]);
                }
            } else {
                W(r.lines, "In any emergency call 112 (fire: 101).");
                B(r.lines, "Stay at least 10 metres from fallen or sparking wires; do not touch anything in contact with them.");
                B(r.lines, "Never walk or drive through floodwater: it can hide open manholes and live wires.");
                B(r.lines, "Keep children and pets away from open drains, dumps and standing water.");
                Tip(r.lines, "Open a complaint (or type its reference) and I will give advice specific to that problem.");
            }
            break;
        }
        case Intent::DRAFT_NOTE: {
            if (!focus) { askWhich(); return r; }
            const Insight& i = insight();
            H(r.lines, focusTitle(*focus));
            if (i.suggestedNote.empty()) {
                T(r.lines, std::string("There is no status change to write a note for right now (status: ") + niceStatus(focus->status()) + ").");
            } else {
                T(r.lines, i.noteLabel + ":");
                T(r.lines, "\"" + i.suggestedNote + "\"");
                Tip(r.lines, "On the complaint screen the same text is offered as a one-click \"Use this note\" button.");
            }
            break;
        }
        case Intent::HOTSPOT: {
            if (focus) {
                const Insight& i = insight();
                H(r.lines, focusTitle(*focus));
                if (i.similarOpen == 0) T(r.lines, std::string("No other open ") + focus->typeName() + " complaints in " + focus->location().city + ". This looks like a one-off.");
                else if (i.hotspot) {
                    W(r.lines, std::to_string(i.similarOpen) + " other open " + focus->typeName() + " complaints in " + focus->location().city + ": a repeat pattern.");
                    if (!citizen) Tip(r.lines, "One coordinated drive in the area can close several at once.");
                } else T(r.lines, std::to_string(i.similarOpen) + " other open " + focus->typeName() + " complaint(s) in " + focus->location().city + ". Not a pattern yet.");
            } else {
                Scope sc = citizen ? Scope{} : svc_.scopeFor(viewer);
                CityRanking cr = svc_.cityRanking(true, 5, "", sc);
                if (cr.rows.empty()) { T(r.lines, "There are no complaints to compare yet."); break; }
                H(r.lines, citizen ? "Cities with the most complaints (all India)" : "Cities with the most complaints in your scope");
                for (std::size_t i = 0; i < cr.rows.size(); ++i)
                    S(r.lines, cr.rows[i].city + ", " + cr.rows[i].state + "  -  " + std::to_string(cr.rows[i].complaints) + " complaints (" +
                                   std::to_string(cr.rows[i].pending) + " pending, " + std::to_string(cr.rows[i].highSeverity) + " high severity)");
                Tip(r.lines, "Ranked with Quick Sort (" + std::to_string(cr.sort.comparisons) + " comparisons). The full list is on the City Ranking screen.");
            }
            break;
        }
        case Intent::CLOSE: {
            if (!focus) { askWhich(); return r; }
            const Insight& i = insight();
            H(r.lines, focusTitle(*focus));
            if (citizen) {
                if (focus->status() == Status::RESOLVED) {
                    T(r.lines, "The department marked it resolved. Before you confirm:");
                    B(r.lines, "Look at the spot - is the problem you described really gone?");
                    B(r.lines, "Is the work area clean and safe?");
                    T(r.lines, "If yes, confirm and close it. If not, do not close it: report it again and quote " + focus->referenceCode() + ".");
                    if (!i.suggestedNote.empty()) Tip(r.lines, "Suggested note: \"" + i.suggestedNote + "\"");
                } else if (focus->status() == Status::CLOSED) T(r.lines, "It is already closed.");
                else T(r.lines, "You can confirm and close it only after the department marks it Resolved.");
            } else {
                if (focus->status() == Status::RESOLVED || focus->status() == Status::CLOSED) T(r.lines, "It is already marked resolved. The citizen confirms and closes it.");
                else if (i.playbook) {
                    T(r.lines, "Before you mark it Resolved:");
                    for (const auto& s : i.playbook->verify) S(r.lines, s);
                    if (!i.suggestedNote.empty() && focus->status() == Status::IN_PROGRESS) Tip(r.lines, "Suggested note: \"" + i.suggestedNote + "\"");
                }
            }
            break;
        }
        case Intent::EXPLAIN_SEVERITY: {
            H(r.lines, "How severity and priority work");
            T(r.lines, "Severity is calculated by the NASM routine: score = category weight + 3 x impact x urgency, capped at 100.");
            B(r.lines, "Category weights: Electricity 22, Water 18, Drainage 16, Traffic 14, Roads 12, Garbage 10.");
            B(r.lines, "Bands: Low below 30, Medium 30-54, High 55-79, Critical 80 and above.");
            T(r.lines, "The AI priority starts from that score and adds the time used on the SLA, danger cues in the description (children, injury, health), repeat complaints and similar open complaints nearby.");
            if (focus) T(r.lines, focus->referenceCode() + ": impact " + std::to_string(focus->impact()) + ", urgency " + std::to_string(focus->urgency()) +
                                      " gives severity " + std::to_string(focus->severityScore()) + " (" + levelName(focus->severityLevel()) + ").");
            break;
        }
        case Intent::EXPLAIN_STATUS: {
            H(r.lines, "The life of a complaint");
            B(r.lines, "Pending: waiting in the first-in-first-out queue.");
            B(r.lines, "Assigned: routed to the department that handles its category.");
            B(r.lines, "In progress: a crew is working on it.");
            B(r.lines, "Resolved: the department says it is fixed and waits for the citizen to confirm.");
            B(r.lines, "Closed: the citizen confirmed. Every change is saved in the history.");
            break;
        }
        case Intent::EXPLAIN_QUEUE: {
            H(r.lines, "How complaints are routed");
            T(r.lines, "New complaints join a first-in-first-out queue. The processor takes the oldest one and assigns it to the department that owns its category (for example Electricity goes to the power department).");
            T(r.lines, "From then on only that department and the citizen who reported it can see and update it; the administrator sees everything.");
            break;
        }
        case Intent::HOW_TO_REPORT: {
            H(r.lines, "How to write a complaint that gets fixed faster");
            S(r.lines, "Pick the category that matches the problem; the helper on the Report screen suggests one.");
            S(r.lines, "Say what is wrong, where (street and a landmark) and since when.");
            S(r.lines, "Say how many people, houses or shops are affected, and mention children, a school or a hospital nearby.");
            S(r.lines, "Choose impact and urgency honestly; they set the severity and the target time.");
            Tip(r.lines, "If someone is in danger, call 112 first and then report it here.");
            break;
        }
        case Intent::UNKNOWN:
        default: {
            if (focus) {
                const Insight& i = insight();
                T(r.lines, "I am not sure what you would like to know about it, but here is where it stands:");
                H(r.lines, focusTitle(*focus));
                T(r.lines, i.headline);
            } else {
                T(r.lines, "I did not quite understand that. I can help with the priority, status, next steps, timing, safety and escalation of a complaint.");
                T(r.lines, "Try one of the questions below, or type a complaint reference such as SCMS-2026-000123.");
            }
            break;
        }
    }
    if (r.followUps.empty()) r.followUps = contextChips(viewer, focus != nullptr);
    return r;
}

} // namespace ai
} // namespace scms
