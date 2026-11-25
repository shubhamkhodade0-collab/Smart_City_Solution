// =====================================================================
// AiInsight.cpp - the advice the assistant shows on ONE complaint, and the triage of a whole workload
//
//   buildInsight()  pure: complaint + number of similar open complaints + clock -> Insight
//                   (department view for DEPARTMENT / ADMIN users, citizen view for citizens)
//   analyse()       asks PostgreSQL for the "similar open complaints" count, then buildInsight()
//   triage()        ranks every open complaint of the signed-in user with the project's Merge Sort
// =====================================================================
#include "AiAgent.h"
#include "AiUtil.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace scms {
namespace ai {

using namespace util;

namespace {

void add(Lines& out, LineKind k, const std::string& text) { out.push_back({k, text}); }

// the playbook lines are written as sentences; the UI shows them as list items
std::string stripDot(std::string s) {
    while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
    return s;
}

// typical time until the first crew visit
int firstResponseHours(const Complaint& c, const Playbook* pb) {
    if ((pb && pb->emergency) || c.severityLevel() == SeverityLevel::CRITICAL) return 2;
    if (c.severityLevel() == SeverityLevel::HIGH) return std::max(2, std::min(6, c.slaHours() / 2));
    return std::max(4, c.slaHours() / 3);
}

int updateIntervalHours(const Complaint& c, const Playbook* pb) {
    if ((pb && pb->emergency) || c.severityLevel() == SeverityLevel::CRITICAL) return 2;
    return std::max(4, std::min(12, c.slaHours() / 4));
}

bool isOpen(Status s) { return s == Status::PENDING || s == Status::ASSIGNED || s == Status::IN_PROGRESS; }

std::string place(const Complaint& c) {
    const Location& l = c.location();
    if (l.locality.empty()) return l.city;
    // the locality often already ends with the city ("Market Yard, Yavatmal") - do not repeat it
    if (lowerCopy(l.locality).find(lowerCopy(l.city)) != std::string::npos) return l.locality;
    return l.locality + ", " + l.city;
}

// ------------------------------------------------------------------ department view
void departmentView(Insight& in, const Complaint& c, const Playbook& pb, const TextReading& rd) {
    in.plan.title = "Recommended solution";
    Lines& p = in.plan.lines;
    switch (c.status()) {
        case Status::PENDING:
            add(p, LineKind::TEXT, "Not assigned yet. The processing queue routes it first-in-first-out to the department that handles " +
                                       std::string(c.typeName()) + " complaints.");
            add(p, LineKind::HEADING, "Get ready");
            for (std::size_t i = 0; i < pb.immediate.size() && i < 2; ++i) add(p, LineKind::BULLET, pb.immediate[i]);
            break;
        case Status::ASSIGNED:
            add(p, LineKind::HEADING, "Start now (first " + std::to_string(firstResponseHours(c, &pb)) + " hours)");
            for (const auto& s : pb.immediate) add(p, LineKind::STEP, s);
            add(p, LineKind::HEADING, "Then fix it");
            for (const auto& s : pb.fieldWork) add(p, LineKind::STEP, s);
            add(p, LineKind::TIP, "Move the complaint to In Progress as soon as the crew is on site.");
            break;
        case Status::IN_PROGRESS:
            add(p, LineKind::HEADING, "Continue the fix");
            for (const auto& s : pb.fieldWork) add(p, LineKind::STEP, s);
            add(p, LineKind::HEADING, "Before you mark it Resolved");
            for (const auto& s : pb.verify) add(p, LineKind::STEP, s);
            break;
        case Status::RESOLVED:
            add(p, LineKind::TEXT, "Marked resolved. The citizen can now confirm and close the complaint.");
            add(p, LineKind::HEADING, "Follow-up");
            for (const auto& s : pb.prevention) add(p, LineKind::BULLET, s);
            break;
        case Status::CLOSED:
            add(p, LineKind::TEXT, "Closed. Nothing more to do on this complaint.");
            add(p, LineKind::HEADING, "Prevention ideas");
            for (const auto& s : pb.prevention) add(p, LineKind::BULLET, s);
            break;
    }

    in.support.title = "Resources and likely causes";
    Lines& s = in.support.lines;
    add(s, LineKind::HEADING, "Line up");
    for (const auto& r : pb.resources) add(s, LineKind::BULLET, r);
    if (!pb.causes.empty()) {
        add(s, LineKind::HEADING, pb.general ? "Check first" : "Likely causes");
        for (const auto& r : pb.causes) add(s, LineKind::BULLET, r);
    }

    in.outlook.title = "Pattern and escalation";
    Lines& o = in.outlook.lines;
    if (isOpen(c.status()) && pb.emergency)
        add(o, LineKind::WARNING, "Safety-critical problem: make the spot safe first, then do the repair.");
    if (in.hotspot) {
        add(o, LineKind::WARNING, std::to_string(in.similarOpen) + " other open " + c.typeName() + " complaints in " + c.location().city +
                                      ". Treat this as an area-wide problem: one coordinated drive can close them together.");
        if (in.similarOpen >= 8)
            add(o, LineKind::TIP, "That many repeats suggests a cause upstream (capacity, route or asset age). Raise it with the zone head for a permanent fix.");
    }
    switch (in.sla.state) {
        case SlaState::BREACHED:
            add(o, LineKind::WARNING, "SLA missed - " + in.sla.text + ". Tell your supervisor, add crew if needed and post a note so the citizen knows what is happening.");
            break;
        case SlaState::AT_RISK:
            add(o, LineKind::TIP, std::to_string(static_cast<int>(in.sla.usedPct)) + "% of the SLA is used. Finish the work or post an update soon.");
            break;
        case SlaState::ON_TRACK:
            add(o, LineKind::TEXT, "On track: " + in.sla.text + ".");
            break;
        default: break;
    }
    if (rd.repeatReport)
        add(o, LineKind::TIP, "The citizen says this was reported before. Check the history for an earlier attempt and say what is different this time.");
    if (c.priority() == Priority::URGENT) add(o, LineKind::TIP, "The citizen marked this complaint Urgent.");
    if (isOpen(c.status()) && !pb.prevention.empty()) add(o, LineKind::TIP, "To stop repeats: " + stripDot(pb.prevention[0]) + ".");

    // a note ready to paste for the next status change
    if (c.status() == Status::ASSIGNED) {
        in.noteLabel = "Note for the citizen - when you start work";
        in.suggestedNote = fitNote("Crew assigned for " + lowerCopy(pb.title) + " at " + place(c) + ". Inspection and first action under way; we will update you within " +
                                   std::to_string(updateIntervalHours(c, &pb)) + " hours.");
    } else if (c.status() == Status::IN_PROGRESS) {
        in.noteLabel = "Note for the citizen - when you mark it Resolved";
        in.suggestedNote = fitNote(pb.resolvedNote + " Please check on your side and confirm.");
    }
}

// ------------------------------------------------------------------ citizen view
void citizenView(Insight& in, const Complaint& c, const Playbook& pb) {
    const std::string ref = c.referenceCode();
    in.plan.title = "What happens next";
    Lines& p = in.plan.lines;
    switch (c.status()) {
        case Status::PENDING:
            add(p, LineKind::TEXT, "Your complaint is in the queue. It is routed automatically, first in first out, to the department that handles " +
                                       std::string(c.typeName()) + " complaints.");
            add(p, LineKind::TEXT, "You will get a notification as soon as it is assigned.");
            add(p, LineKind::TEXT, "The target for this kind of complaint is " + std::to_string(c.slaHours()) + " hours from the time you reported it.");
            break;
        case Status::ASSIGNED:
            add(p, LineKind::TEXT, (c.departmentName().empty() ? std::string("The department") : c.departmentName()) +
                                       " has your complaint. A first crew visit is normally planned within about " +
                                       std::to_string(firstResponseHours(c, &pb)) + " hours.");
            add(p, LineKind::BULLET, "Next step: it moves to In Progress when work starts.");
            add(p, LineKind::BULLET, "You will be notified at every status change.");
            break;
        case Status::IN_PROGRESS:
            add(p, LineKind::TEXT, "Work is under way. The team will usually:");
            for (std::size_t i = 0; i < pb.fieldWork.size() && i < 2; ++i) add(p, LineKind::BULLET, pb.fieldWork[i]);
            break;
        case Status::RESOLVED:
            add(p, LineKind::TEXT, "The department says it is fixed. Please look at the spot and confirm.");
            add(p, LineKind::HEADING, "Check that");
            add(p, LineKind::BULLET, "The problem you described is gone.");
            add(p, LineKind::BULLET, "The work area has been left clean and safe.");
            add(p, LineKind::TIP, "Fixed? Confirm and close it. Not fixed? Do not close it - report it again and quote " + ref + ".");
            break;
        case Status::CLOSED:
            add(p, LineKind::TEXT, "This complaint is closed. Thank you for confirming.");
            break;
    }

    in.support.title = "While you wait";
    Lines& s = in.support.lines;
    if (isOpen(c.status())) {
        if (!pb.citizenSafety.empty()) {
            add(s, LineKind::HEADING, "Stay safe");
            for (const auto& t : pb.citizenSafety) add(s, pb.emergency ? LineKind::WARNING : LineKind::TIP, t);
        }
        if (!pb.citizenTips.empty()) {
            add(s, LineKind::HEADING, "Help the team");
            for (const auto& t : pb.citizenTips) add(s, LineKind::BULLET, t);
        }
    } else {
        add(s, LineKind::TEXT, "If the problem comes back, report a new complaint and mention " + ref + " so the team can see the history.");
    }

    in.outlook.title = "If it is not fixed in time";
    Lines& o = in.outlook.lines;
    if (in.hotspot)
        add(o, LineKind::TEXT, std::to_string(in.similarOpen) + " other open " + c.typeName() + " complaints exist in " + c.location().city +
                                   ", so an area-wide fix is more likely than a one-off visit.");
    switch (in.sla.state) {
        case SlaState::BREACHED:
            add(o, LineKind::WARNING, "The " + std::to_string(c.slaHours()) + "-hour target passed " + AiAgent::formatHours(-in.sla.hoursLeft) + " ago. You can:");
            add(o, LineKind::BULLET, "Call the department's helpline or visit the ward office and quote " + ref + ".");
            add(o, LineKind::BULLET, "Ask your ward representative or the municipal control room to follow it up.");
            add(o, LineKind::BULLET, "Use your state's public grievance portal, or CPGRAMS, and quote the same reference.");
            add(o, LineKind::TIP, "Keep the reference number, the dates and any notes from this screen ready.");
            break;
        case SlaState::AT_RISK:
            add(o, LineKind::TEXT, "The target is close (" + in.sla.text + "). Nothing to do yet - you will be notified when the status changes.");
            break;
        case SlaState::ON_TRACK:
            add(o, LineKind::TEXT, "Nothing to do yet: " + in.sla.text + ". You will be notified at every step.");
            break;
        default: break;
    }

    if (c.status() == Status::RESOLVED) {
        in.noteLabel = "Note when you confirm";
        in.suggestedNote = "Checked on site - the issue is fixed. Thank you.";
    }
}

} // namespace

Insight AiAgent::buildInsight(const User& viewer, const Complaint& c, int similarOpen, std::time_t now) {
    Insight in;
    TextReading rd = readText(c.description(), c.categoryCode(), c.location().locality);
    const Playbook* pb = rd.playbook ? rd.playbook : generalPlaybook(c.categoryCode());
    in.playbook = pb;
    in.confidence = rd.confidence;
    in.sla = slaClock(c, now);
    in.similarOpen = isOpen(c.status()) ? std::max(0, similarOpen) : 0;
    in.hotspot = in.similarOpen >= 3;

    // evidence behind the priority, in the order the UI lists it
    int signalPoints = 0;
    for (const auto& s : rd.signals) { in.signals.push_back(s); signalPoints += s.points; }
    if (isOpen(c.status()) && in.sla.state != SlaState::ON_TRACK) {
        int pts = static_cast<int>(std::lround(slaBoost(in.sla)));
        if (pts > 0) in.signals.push_back({in.sla.state == SlaState::BREACHED ? "the SLA target has been missed" : "most of the SLA time is used", pts});
    }
    if (in.similarOpen >= 3) in.signals.push_back({std::to_string(in.similarOpen) + " other open complaints of this kind in " + c.location().city, in.similarOpen >= 8 ? 6 : 3});
    if (c.priority() == Priority::URGENT) in.signals.push_back({"the citizen marked it Urgent", 5});
    else if (c.priority() == Priority::HIGH) in.signals.push_back({"the citizen marked it High priority", 3});

    in.priority = priorityScore(c, in.sla, signalPoints, in.similarOpen);
    in.priorityLabel = priorityLabel(in.priority, c.status());

    const bool deptView = viewer.role() != Role::CITIZEN;
    std::string kind = pb ? pb->title : std::string(c.typeName());
    if (deptView) {
        in.headline = std::string(c.typeName()) + " - " + kind + ". " + in.priorityLabel + " priority (" + std::to_string(in.priority) + "/100). " + in.sla.text + ".";
        if (pb) departmentView(in, c, *pb, rd);
    } else {
        std::string state;
        switch (c.status()) {
            case Status::PENDING: state = "It is waiting to be assigned."; break;
            case Status::ASSIGNED: state = "It has been passed to the department."; break;
            case Status::IN_PROGRESS: state = "Work is under way."; break;
            case Status::RESOLVED: state = "The department says it is fixed - please confirm."; break;
            case Status::CLOSED: state = "It is closed."; break;
        }
        in.headline = (in.confidence > 0 ? "This looks like: " + kind + ". " : std::string(c.typeName()) + " complaint. ") + state;
        if (pb) citizenView(in, c, *pb);
    }
    return in;
}

// ---------------------------------------------------------------- database-aware
std::unique_ptr<Complaint> AiAgent::loadVisible(const User& viewer, long long id) {
    try {
        std::unique_ptr<Complaint> c = db_.getComplaintById(id);
        if (c && viewer.canView(*c)) return c;
    } catch (const std::exception&) {
    }
    return nullptr;
}

// other still-open complaints of the same category in the same city; aggregate numbers only (no personal data)
int AiAgent::similarOpenCount(const Complaint& c) {
    int total = 0;
    try {
        static const char* open[] = {"PENDING", "ASSIGNED", "IN_PROGRESS"};
        for (const char* st : open) {
            ComplaintFilter f;
            f.cityId = c.location().cityId;
            f.categoryCode = c.categoryCode();
            f.status = st;
            total += db_.countComplaints(f);
        }
    } catch (const std::exception&) {
        return 0;
    }
    if (isOpen(c.status())) --total;   // the complaint itself
    return std::max(0, total);
}

Insight AiAgent::analyse(const User& viewer, const Complaint& c) {
    return buildInsight(viewer, c, similarOpenCount(c), std::time(nullptr));
}

Triage AiAgent::triage(const User& viewer) {
    return triageScope(svc_.scopeFor(viewer), viewer.role() == Role::CITIZEN);
}

Triage AiAgent::triageScope(const Scope& scope, bool includeResolved) {
    Triage t;
    ComplaintFilter f;
    f.scope = scope;
    f.activeOnly = true;
    f.limit = 400;
    f.orderBy = "severity";
    std::vector<std::unique_ptr<Complaint>> list = db_.getComplaints(f);
    const std::time_t now = std::time(nullptr);

    // cheap hotspot proxy inside the loaded list: open complaints per (city, category)
    std::map<std::pair<int, std::string>, int> groups;
    for (const auto& c : list)
        if (isOpen(c->status())) ++groups[{c->location().cityId, c->categoryCode()}];

    for (const auto& cp : list) {
        const Complaint& c = *cp;
        if (c.status() == Status::RESOLVED && !includeResolved) continue;   // a resolved complaint waits for the citizen, not the department
        int similar = isOpen(c.status()) ? groups[{c.location().cityId, c.categoryCode()}] - 1 : 0;
        TextReading rd = readText(c.description(), c.categoryCode(), c.location().locality);
        SlaClock sla = slaClock(c, now);
        int points = 0;
        for (const auto& s : rd.signals) points += s.points;

        TriageItem it;
        it.id = c.id();
        it.reference = c.referenceCode();
        it.category = c.typeName();
        it.city = c.location().city;
        it.locality = c.location().locality;
        it.status = c.status();
        it.severity = c.severityLevel();
        it.severityScore = c.severityScore();
        it.priority = priorityScore(c, sla, points, similar);
        it.label = priorityLabel(it.priority, c.status());
        it.sla = sla.state;
        it.hoursLeft = sla.hoursLeft;
        it.ageHours = sla.ageHours;

        std::vector<std::string> why;
        if (c.status() == Status::RESOLVED) why.push_back("fixed - please confirm or report again");
        else {
            // what makes THIS complaint stand out comes first (danger cues), then the clock, then repeats nearby
            for (const auto& s : rd.signals)
                if (why.size() < 2 && s.points >= 4) why.push_back(s.text);
            if (why.size() < 2 && sla.state == SlaState::BREACHED) why.push_back("SLA missed by " + formatHours(-sla.hoursLeft));
            else if (why.size() < 2 && sla.state == SlaState::AT_RISK) why.push_back(formatHours(sla.hoursLeft) + " left on the SLA");
            if (similar >= 3 && why.size() < 2) why.push_back(std::to_string(similar) + " similar open complaints in " + c.location().city);
            if (why.empty()) why.push_back(rd.playbook ? rd.playbook->title : std::string(c.typeName()));
        }
        it.reason = why[0] + (why.size() > 1 ? "; " + why[1] : "");

        if (c.status() != Status::RESOLVED) {
            ++t.open;
            if (sla.state == SlaState::BREACHED) ++t.overdue;
            else if (sla.state == SlaState::AT_RISK) ++t.atRisk;
            if (it.priority >= 80) ++t.critical;
        }
        t.items.push_back(it);
    }

    // highest priority first; equal priority -> the one with less SLA time left (Merge Sort is stable)
    t.sort = mergeSort(t.items, [](const TriageItem& a, const TriageItem& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.hoursLeft < b.hoursLeft;
    });
    return t;
}

} // namespace ai
} // namespace scms
