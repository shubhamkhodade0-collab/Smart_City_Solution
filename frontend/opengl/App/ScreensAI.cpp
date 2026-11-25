// =====================================================================
// ScreensAI.cpp - the AI assistant in the native OpenGL UI (no web technology involved)
//
//   * AI Assistant screen   chat + live triage list (citizens and departments)
//   * AI card               recommendations on the complaint-details screen
//   * report-form helper    suggests category / impact / urgency while a citizen types
//
// All analysis comes from backend/AiAgent (local expert system). This file only draws it.
// =====================================================================
#include "App.h"
#include "../../../backend/Exceptions.h"
#include "../../../backend/Validator.h"
#include <algorithm>
#include <cctype>

using namespace ui;
using namespace scms;

static const Palette& P() { return Theme::p(); }
static std::string num(long long v) { return std::to_string(v); }

static std::string upper(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

static void labelColors(const std::string& label, Color& fg, Color& bg) {
    if (label == "Critical") { fg = P().danger; bg = P().dangerSoft; }
    else if (label == "Urgent") { fg = P().warning; bg = P().warningSoft; }
    else if (label == "Elevated") { fg = P().info; bg = P().infoSoft; }
    else if (label == "Routine") { fg = P().success; bg = P().successSoft; }
    else if (label == "Verify") { fg = P().primary; bg = P().primarySoft; }
    else { fg = P().textMuted; bg = P().neutralSoft; }
}

static Color slaColor(ai::SlaState s) {
    switch (s) {
        case ai::SlaState::ON_TRACK: return P().success;
        case ai::SlaState::AT_RISK: return P().warning;
        case ai::SlaState::BREACHED: return P().danger;
        default: return P().textMuted;
    }
}

// ============================================================ typed lines (shared by every AI panel)
// draw == false only measures, so layouts can size their cards before drawing.
float App::aiLines(const ai::Lines& lines, float x, float y, float w, bool draw) {
    float cy = y;
    int step = 0;
    bool first = true;
    for (const auto& l : lines) {
        if (l.kind != ai::LineKind::STEP) step = 0;
        switch (l.kind) {
            case ai::LineKind::HEADING:
                if (!first) cy += 8;
                if (draw) Renderer::text(x, cy + 13, upper(l.text), 10.5f, P().textFaint, Weight::SEMIBOLD);
                cy += 21;
                break;
            case ai::LineKind::TEXT: {
                for (const auto& s : Renderer::wrap(l.text, w, 13)) {
                    if (draw) Renderer::text(x, cy + 14, s, 13, P().text);
                    cy += 19;
                }
                cy += 3;
                break;
            }
            case ai::LineKind::BULLET: {
                if (draw) Renderer::circle(x + 4, cy + 10, 2.2f, P().textMuted);
                for (const auto& s : Renderer::wrap(l.text, w - 16, 13)) {
                    if (draw) Renderer::text(x + 16, cy + 14, s, 13, P().text);
                    cy += 19;
                }
                cy += 3;
                break;
            }
            case ai::LineKind::STEP: {
                ++step;
                if (draw) {
                    Renderer::circle(x + 9, cy + 10, 9, P().primarySoft);
                    Renderer::textIn({x, cy + 1, 18, 18}, num(step), 10.5f, P().primary, Weight::BOLD, Align::CENTER);
                }
                for (const auto& s : Renderer::wrap(l.text, w - 28, 13)) {
                    if (draw) Renderer::text(x + 28, cy + 14, s, 13, P().text);
                    cy += 19;
                }
                cy += 5;
                break;
            }
            case ai::LineKind::NOTE: {
                for (const auto& s : Renderer::wrap(l.text, w, 11.5f)) {
                    if (draw) Renderer::text(x, cy + 13, s, 11.5f, P().textFaint);
                    cy += 16;
                }
                cy += 4;
                break;
            }
            case ai::LineKind::WARNING:
            case ai::LineKind::TIP: {
                const bool warn = l.kind == ai::LineKind::WARNING;
                auto ws = Renderer::wrap(l.text, w - 34, 12.5f);
                float h = ws.size() * 18.0f + 14;
                if (draw) {
                    Rect b{x, cy, w, h};
                    Renderer::roundRect(b, 8, warn ? P().dangerSoft : P().infoSoft);
                    icon(warn ? "alert" : "info", b.x + 16, b.y + 17, 13, warn ? P().danger : P().info);
                    float ty = b.y + 18;
                    for (const auto& s : ws) { Renderer::text(b.x + 32, ty, s, 12.5f, P().text); ty += 18; }
                }
                cy += h + 6;
                break;
            }
        }
        first = false;
    }
    return cy - y;
}

float App::aiColumn(const std::string& title, const ai::Lines& lines, float x, float y, float w, bool draw) {
    if (draw) Renderer::text(x, y + 12, upper(title), 11, P().primary, Weight::BOLD);
    return 26 + aiLines(lines, x, y + 26, w, draw);
}

// ============================================================ assistant state
void App::aiReset() {
    if (aiPending_.job) aiPending_.job->cancel();   // the worker thread finishes on its own; its answer is dropped
    aiPending_ = PendingLlm();
    aiChat_.clear();
    aiInput_.field.clear();
    aiFocusId_ = 0;
    aiFocusRef_.clear();
    aiFocusTitle_.clear();
    aiTriage_ = ai::Triage();
    aiTriageLoaded_ = false;
    aiInsight_.reset();
    aiDraftSeen_.clear();
    aiDraft_ = ai::TextReading();
    aiScroll_.offset = 0;
    aiSideScroll_.offset = 0;
    aiStick_ = true;
}

void App::aiLoadInsight() {
    aiInsight_.reset();
    if (!detail_ || !user_ || user_->role() == Role::ADMIN) return;   // the assistant serves citizens and departments
    guarded([&] { aiInsight_.reset(new ai::Insight(agent_.analyse(*user_, *detail_))); });
}

void App::aiSend(const std::string& raw) {
    std::string text = Validator::trim(raw);
    if (text.empty() || !user_) return;
    if (aiPending_.job) { UI::get().toast("Please wait for the answer first", ToastKind::INFO); return; }

    // the conversation so far, for Groq (without the greeting and without bubbles still waiting)
    std::vector<ai::LlmMessage> history;
    for (std::size_t i = 1; i < aiChat_.size(); ++i) {
        const AiMessage& m = aiChat_[i];
        if (m.pending) continue;
        std::string t;
        for (const auto& l : m.lines)
            if (l.kind != ai::LineKind::NOTE) t += (l.kind == ai::LineKind::BULLET || l.kind == ai::LineKind::STEP ? "- " : "") + l.text + "\n";
        if (!t.empty()) history.push_back({m.fromUser ? "user" : "assistant", t});
    }

    AiMessage q;
    q.fromUser = true;
    q.lines.push_back({ai::LineKind::TEXT, text});
    aiChat_.push_back(q);

    AiMessage a;
    ai::Reply reply;
    if (!guarded([&] { reply = agent_.chat(*user_, text, aiFocusId_); })) {
        a.lines.push_back({ai::LineKind::WARNING, "I could not read the complaint data (the database is not reachable). Please try again in a moment."});
        aiChat_.push_back(a);
        aiStick_ = true;
        return;
    }
    if (reply.focusId == 0) { aiFocusId_ = 0; aiFocusRef_.clear(); aiFocusTitle_.clear(); }
    else if (reply.focusId > 0 && reply.focusId != aiFocusId_) {
        aiFocusId_ = reply.focusId;
        guarded([&] {
            std::unique_ptr<Complaint> c = db_.getComplaintById(aiFocusId_);
            if (c) { aiFocusRef_ = c->referenceCode(); aiFocusTitle_ = std::string(c->typeName()) + ", " + c->location().city + "  (" + displayName(c->status()) + ")"; }
        });
    }

    // optional Groq: the built-in assistant did not understand (or the user chose "every question")
    if (ai::AiAgent::wantsLlm(reply, groq_)) {
        ai::LlmRequest rq;
        if (guarded([&] { rq = agent_.llmRequest(*user_, text, aiFocusId_, history, groq_, &reply); })) {
            a.pending = true;
            a.lines.push_back({ai::LineKind::TEXT, "Asking GPT-4"});
            aiChat_.push_back(a);
            aiPending_.job = ai::Async<ai::LlmResult>::run([rq]() { return ai::groqChat(rq); });
            aiPending_.msgIndex = aiChat_.size() - 1;
            aiPending_.builtIn = reply;
            aiStick_ = true;
            return;
        }
    }
    a.lines = reply.lines;
    a.chips = reply.followUps;
    if (reply.intent == "unknown" && !groq_.ready())
        a.lines.push_back({ai::LineKind::TIP, "Tip: add GROQ_API_KEY to the project-root .env file and restart the app to enable direct AI."});
    aiChat_.push_back(a);
    aiStick_ = true;
}

// called every frame: a finished AI answer replaces the "Asking GPT-4" bubble
void App::aiPoll() {
    if (aiPending_.job && aiPending_.job->done()) {
        ai::LlmResult res = aiPending_.job->get();
        if (aiPending_.msgIndex < aiChat_.size() && aiChat_[aiPending_.msgIndex].pending) {
            ai::Reply r = ai::AiAgent::withLlmAnswer(aiPending_.builtIn, res, groq_);
            AiMessage& m = aiChat_[aiPending_.msgIndex];
            m.lines = r.lines;
            m.chips = r.followUps;
            m.pending = false;
            m.h = -1;
            aiStick_ = true;
        }
        aiPending_ = PendingLlm();
    }
}

void App::aiAsk(long long complaintId, const std::string& question) {
    aiFocusId_ = complaintId;
    aiFocusRef_.clear();
    aiFocusTitle_.clear();
    guarded([&] {
        std::unique_ptr<Complaint> c = db_.getComplaintById(complaintId);
        if (c) { aiFocusRef_ = c->referenceCode(); aiFocusTitle_ = std::string(c->typeName()) + ", " + c->location().city + "  (" + displayName(c->status()) + ")"; }
    });
    go(Screen::ASSISTANT);
    aiSend(question);
}

// ============================================================ AI card on the complaint screen
// column 1: the numbers (priority, SLA, evidence)
static float drawAssessment(const ai::Insight& in, float x, float y, float w, bool draw) {
    Color fg, bg;
    labelColors(in.priorityLabel, fg, bg);
    float cy = y;
    if (draw) {
        Renderer::text(x, cy + 12, "AI PRIORITY", 11, P().primary, Weight::BOLD);
        std::string score = num(in.priority);
        float scoreEnd = Renderer::text(x, cy + 50, score, 34, fg, Weight::BOLD);   // text() returns the END x
        float slashEnd = Renderer::text(scoreEnd + 6, cy + 50, "/ 100", 13, P().textMuted);
        badge(slashEnd + 12, cy + 40, in.priorityLabel, fg, bg, 12);
        progressBar({x, cy + 62, w, 8}, in.priority / 100.0f, fg);
    }
    cy += 86;
    if (draw) {
        Renderer::text(x, cy + 12, "SLA CLOCK", 11, P().primary, Weight::BOLD);
    }
    cy += 20;
    for (const auto& s : Renderer::wrap(in.sla.text, w, 13, Weight::SEMIBOLD)) {
        if (draw) Renderer::text(x, cy + 14, s, 13, slaColor(in.sla.state), Weight::SEMIBOLD);
        cy += 19;
    }
    if (in.sla.state != ai::SlaState::DONE) {
        if (draw) progressBar({x, cy + 4, w, 6}, std::min(1.0f, static_cast<float>(in.sla.usedPct / 100.0)), slaColor(in.sla.state));
        cy += 16;
    }
    cy += 10;
    if (draw) Renderer::text(x, cy + 12, "WHAT RAISED THE PRIORITY", 11, P().primary, Weight::BOLD);
    cy += 20;
    if (in.signals.empty()) {
        for (const auto& s : Renderer::wrap("Nothing in the text or the clock lifts it above its severity score.", w, 12.5f)) {
            if (draw) Renderer::text(x, cy + 14, s, 12.5f, P().textMuted);
            cy += 18;
        }
    }
    int shown = 0;
    for (const auto& sg : in.signals) {
        if (++shown > 5) break;
        std::string t = "+" + num(sg.points) + "  " + sg.text;
        auto ws = Renderer::wrap(t, w, 12.5f);
        for (const auto& s : ws) {
            if (draw) Renderer::text(x, cy + 14, s, 12.5f, P().text);
            cy += 18;
        }
        cy += 3;
    }
    return cy - y;
}

float App::aiDetailHeight(float width, bool hasNote) {
    if (!aiInsight_) return 0;
    if (!aiOpen_) return 64;
    const ai::Insight& in = *aiInsight_;
    const float pad = 24, gap = 28;
    float c1 = 290, c23 = (width - 2 * pad - c1 - 2 * gap) / 2;
    float rowA = std::max({drawAssessment(in, 0, 0, c1, false), aiColumn(in.plan.title, in.plan.lines, 0, 0, c23, false),
                           aiColumn(in.support.title, in.support.lines, 0, 0, c23, false)});
    float rowB = 0;
    if (!in.outlook.lines.empty()) rowB = aiColumn(in.outlook.title, in.outlook.lines, 0, 0, c1 + gap + c23, false);
    if (!in.suggestedNote.empty()) {
        float nh = 26 + Renderer::wrap("\"" + in.suggestedNote + "\"", c23 - 28, 12.5f).size() * 18.0f + 22 + (hasNote ? 46.0f : 0.0f);
        rowB = std::max(rowB, nh);
    }
    return 64 + 20 + rowA + (rowB > 0 ? 30 + rowB : 0) + 22;
}

void App::aiDetailCard(const Rect& r, bool canUseNote) {
    if (!aiInsight_ || !detail_) return;
    const ai::Insight& in = *aiInsight_;
    UI& u = UI::get();
    card(r);

    // header
    Renderer::circle(r.x + 40, r.y + 32, 18, P().primarySoft);
    icon("ai", r.x + 40, r.y + 32, 18, P().primary);
    float titleEnd = Renderer::text(r.x + 68, r.y + 28, "AI assistant", 16, P().text, Weight::SEMIBOLD);   // returns the END x
    badge(titleEnd + 12, r.y + 22, in.confidence > 0 ? "Detected: " + in.playbook->title + "  (" + num(in.confidence) + "%)" : "General guidance for this category",
          P().primaryText, P().primarySoft, 11.5f);
    Renderer::textIn({r.x + 68, r.y + 38, r.w - 68 - 300, 20}, in.headline, 12.5f, P().textMuted);
    if (button({r.right() - 112, r.y + 15, 90, 34}, aiOpen_ ? "Hide" : "Show", ButtonStyle::GHOST)) aiOpen_ = !aiOpen_;
    if (button({r.right() - 262, r.y + 15, 138, 34}, "Ask AI", ButtonStyle::PRIMARY, true, "ai")) {
        aiAsk(detail_->id(), user_->role() == Role::CITIZEN ? "What happens next?" : "What should I do next?");
        return;
    }
    if (!aiOpen_) return;
    Renderer::line(r.x + 1, r.y + 64, r.right() - 1, r.y + 64, P().border);

    const float pad = 24, gap = 28;
    float c1 = 290, c23 = (r.w - 2 * pad - c1 - 2 * gap) / 2;
    float x1 = r.x + pad, x2 = x1 + c1 + gap, x3 = x2 + c23 + gap;
    float y = r.y + 64 + 20;
    float h1 = drawAssessment(in, x1, y, c1, true);
    float h2 = aiColumn(in.plan.title, in.plan.lines, x2, y, c23, true);
    float h3 = aiColumn(in.support.title, in.support.lines, x3, y, c23, true);
    float rowA = std::max({h1, h2, h3});
    Renderer::line(x2 - gap / 2, y, x2 - gap / 2, y + rowA, P().border);
    Renderer::line(x3 - gap / 2, y, x3 - gap / 2, y + rowA, P().border);

    y += rowA + 14;
    bool haveB = !in.outlook.lines.empty() || !in.suggestedNote.empty();
    if (!haveB) return;
    Renderer::line(r.x + 1, y, r.right() - 1, y, P().border);
    y += 16;
    if (!in.outlook.lines.empty()) aiColumn(in.outlook.title, in.outlook.lines, x1, y, c1 + gap + c23, true);
    if (!in.suggestedNote.empty()) {
        Renderer::text(x3, y + 12, upper(in.noteLabel), 11, P().primary, Weight::BOLD);
        auto ws = Renderer::wrap("\"" + in.suggestedNote + "\"", c23 - 28, 12.5f);
        Rect box{x3, y + 24, c23, ws.size() * 18.0f + 22};
        Renderer::roundRect(box, 8, P().surfaceAlt);
        Renderer::roundRectOutline(box, 8, P().border);
        float ty = box.y + 24;
        for (const auto& s : ws) { Renderer::text(box.x + 14, ty - 5, s, 12.5f, P().text); ty += 18; }
        if (canUseNote) {
            if (button({box.x, box.bottom() + 10, 150, 34}, "Use this note", ButtonStyle::SECONDARY, true, "check")) {
                note_.field.setText(in.suggestedNote);
                detailScroll_.offset = 0;   // bring the note box into view
                u.toast("Note added to the box above - edit it if you like", ToastKind::SUCCESS);
            }
        }
    }
}

// ============================================================ helper next to the report form
void App::aiReportPanel(const Rect& r) {
    card(r);
    Renderer::circle(r.x + 34, r.y + 32, 16, P().primarySoft);
    icon("ai", r.x + 34, r.y + 32, 16, P().primary);
    Renderer::text(r.x + 60, r.y + 30, "AI writing helper", 15, P().text, Weight::SEMIBOLD);
    Renderer::text(r.x + 60, r.y + 47, "Runs on this computer", 11, P().textFaint);

    // re-read the text only when it (or the category) changed
    std::string catCode = rCategory_.selected >= 0 ? svc_.categories()[rCategory_.selected].code : "";
    std::string key = rDesc_.text() + "\n" + rLocality_.text() + "\n" + catCode;
    if (key != aiDraftSeen_) {
        aiDraftSeen_ = key;
        aiDraft_ = agent_.readText(rDesc_.text(), "", rLocality_.text());
        if (!aiDraft_.playbook && !catCode.empty()) aiDraft_ = agent_.readText(rDesc_.text(), catCode, rLocality_.text());
    }
    const ai::TextReading& d = aiDraft_;
    Renderer::pushClip(r);
    float x = r.x + 20, w = r.w - 40, y = r.y + 70;
    if (Validator::trim(rDesc_.text()).size() < 12) {
        for (const auto& s : Renderer::wrap("Describe the problem in the Description box. I will suggest the category, impact and urgency, and tell you what details to add so the right team can act faster.", w, 13)) {
            Renderer::text(x, y + 14, s, 13, P().textMuted);
            y += 19;
        }
        y += 12;
        ai::Lines tips;
        tips.push_back({ai::LineKind::HEADING, "A good complaint says"});
        tips.push_back({ai::LineKind::BULLET, "what is wrong and exactly where"});
        tips.push_back({ai::LineKind::BULLET, "since when it has been happening"});
        tips.push_back({ai::LineKind::BULLET, "how many people are affected"});
        tips.push_back({ai::LineKind::TIP, "If someone is in danger, call 112 first."});
        aiLines(tips, x, y, w, true);
        Renderer::popClip();
        return;
    }
    if (!d.playbook || d.confidence == 0) {
        for (const auto& s : Renderer::wrap("I cannot tell the kind of problem yet. Mention what is wrong in plain words, for example \"no water\", \"pothole\", \"power cut\" or \"garbage not collected\".", w, 13)) {
            Renderer::text(x, y + 14, s, 13, P().textMuted);
            y += 19;
        }
        Renderer::popClip();
        return;
    }
    Renderer::text(x, y + 4, "I READ THIS AS", 10.5f, P().textFaint, Weight::SEMIBOLD);
    y += 8;
    for (const auto& s : Renderer::wrap(d.playbook->title, w, 15, Weight::SEMIBOLD)) {
        Renderer::text(x, y + 18, s, 15, P().text, Weight::SEMIBOLD);
        y += 21;
    }
    Renderer::text(x, y + 16, num(d.confidence) + "% sure", 11.5f, P().textMuted);
    y += 28;

    // category suggestion
    int suggested = -1;
    for (std::size_t i = 0; i < svc_.categories().size(); ++i)
        if (svc_.categories()[i].code == d.playbook->category) suggested = static_cast<int>(i);
    if (suggested >= 0) {
        if (rCategory_.selected == suggested) {
            icon("check", x + 8, y + 12, 12, P().success);
            Renderer::text(x + 24, y + 16, "Category matches: " + svc_.categories()[suggested].name, 12.5f, P().text);
            y += 30;
        } else if (button({x, y, w, 34}, "Use category: " + svc_.categories()[suggested].name, ButtonStyle::SECONDARY)) {
            rCategory_.selected = suggested;
            rCategory_.error.clear();
        } else {
            y += 42;
        }
    }
    // impact / urgency suggestion
    if (d.impact > 0 && d.urgency > 0) {
        bool same = rImpact_ == d.impact && rUrgency_ == d.urgency;
        Renderer::text(x, y + 14, "Suggested impact " + num(d.impact) + ", urgency " + num(d.urgency), 12.5f, P().text, Weight::SEMIBOLD);
        y += 22;
        ai::Lines why;
        for (std::size_t i = 1; i < d.impactWhy.size() && i < 3; ++i) why.push_back({ai::LineKind::BULLET, "Impact: " + d.impactWhy[i]});
        for (std::size_t i = 1; i < d.urgencyWhy.size() && i < 3; ++i) why.push_back({ai::LineKind::BULLET, "Urgency: " + d.urgencyWhy[i]});
        if (!why.empty()) y += aiLines(why, x, y, w, true);
        if (same) {
            icon("check", x + 8, y + 12, 12, P().success);
            Renderer::text(x + 24, y + 16, "Already applied", 12.5f, P().text);
            y += 30;
        } else if (button({x, y, w, 34}, "Apply impact " + num(d.impact) + " and urgency " + num(d.urgency), ButtonStyle::SECONDARY)) {
            rImpact_ = d.impact;
            rUrgency_ = d.urgency;
            rImpactErr_.clear();
            rUrgencyErr_.clear();
        } else {
            y += 42;
        }
    }
    // what to add / safety
    ai::Lines more;
    if (d.hazard || d.playbook->emergency) more.push_back({ai::LineKind::WARNING, "If anyone is in danger, call 112 now. Then report it here."});
    if (!d.missing.empty()) {
        more.push_back({ai::LineKind::HEADING, "Add for a faster fix"});
        for (const auto& m : d.missing) more.push_back({ai::LineKind::BULLET, m});
    }
    if (!d.playbook->general && !d.playbook->citizenSafety.empty() && !(d.hazard || d.playbook->emergency)) {
        more.push_back({ai::LineKind::HEADING, "While you wait"});
        more.push_back({ai::LineKind::TIP, d.playbook->citizenSafety[0]});
    }
    aiLines(more, x, y + 4, w, true);
    Renderer::popClip();
}


// ============================================================ AI ASSISTANT SCREEN
void App::screenAssistant() {
    if (!user_ || user_->role() == Role::ADMIN) { go(Screen::DASHBOARD); return; }
    UI& u = UI::get();
    const bool citizen = user_->role() == Role::CITIZEN;
    if (aiChat_.empty()) {
        AiMessage g;
        ai::Reply r = agent_.greeting(*user_, groq_.ready() ? groq_.mode : -1);
        g.lines = r.lines;
        g.chips = r.followUps;
        aiChat_.push_back(g);
    }
    if (!aiTriageLoaded_ && dbOnline_) {
        aiTriageLoaded_ = true;
        guarded([&] { aiTriage_ = agent_.triage(*user_); });
    }

    Rect c = content();
    Rect r{c.x + 24, c.y + 20, c.w - 48, c.h - 40};
    float sideW = 390;
    Rect chat{r.x, r.y, r.w - sideW - 16, r.h};
    Rect side{chat.right() + 16, r.y, sideW, r.h};

    // ------------------------------------------------------------ chat card
    card(chat);
    Renderer::circle(chat.x + 38, chat.y + 34, 18, P().primarySoft);
    icon("ai", chat.x + 38, chat.y + 34, 18, P().primary);
    float titleEnd = Renderer::text(chat.x + 66, chat.y + 30, "SmartCity Assistant", 16, P().text, Weight::SEMIBOLD);   // returns the END x
    badge(titleEnd + 12, chat.y + 24, groq_.ready() ? (groq_.mode == 1 ? "Direct AI (GPT-4)" : "Local engine + GPT-4") : "Local expert engine", P().success, P().successSoft, 11);
    Renderer::text(chat.x + 66, chat.y + 49, groq_.ready() && groq_.mode == 1 ? "Ask anything - complaints, general questions, study help" : citizen ? "Ask about your complaints, safety and what happens next" : "Ask for priorities, fix plans and status notes", 12, P().textMuted);
    if (button({chat.right() - 112, chat.y + 17, 92, 34}, "New chat", ButtonStyle::GHOST)) { if (aiPending_.job) aiPending_.job->cancel(); aiPending_ = PendingLlm(); aiChat_.clear(); aiFocusId_ = 0; aiFocusRef_.clear(); aiFocusTitle_.clear(); return; }
    Renderer::line(chat.x + 1, chat.y + 66, chat.right() - 1, chat.y + 66, P().border);

    float top = chat.y + 67;
    if (aiFocusId_ > 0 && !aiFocusRef_.empty()) {   // which complaint the conversation is about
        Rect f{chat.x + 16, top + 10, chat.w - 32, 36};
        Renderer::roundRect(f, 8, P().primarySoft);
        icon("list", f.x + 20, f.y + 18, 14, P().primary);
        Renderer::textIn({f.x + 38, f.y, f.w - 190, f.h}, "Talking about  " + aiFocusRef_ + "   " + aiFocusTitle_, 12.5f, P().primaryText, Weight::SEMIBOLD);
        if (linkText(f.right() - 150, f.y + 23, "Open complaint", 12.5f)) { backFromDetail_ = Screen::ASSISTANT; openDetail(aiFocusId_); return; }
        if (linkText(f.right() - 40, f.y + 23, "Clear", 12.5f)) { aiFocusId_ = 0; aiFocusRef_.clear(); aiFocusTitle_.clear(); }
        top += 52;
    }

    // chips (follow-up questions) and the input box live at the bottom
    std::vector<std::string> chips;
    if (!aiChat_.empty() && !aiChat_.back().fromUser) chips = aiChat_.back().chips;
    std::vector<Rect> chipRects;
    float chipX = chat.x + 20, chipY = 0, chipRowH = 34;
    int rows = 0;
    {
        float cx = chipX;
        for (const auto& s : chips) {
            float cw = Renderer::measure(s, 12.5f) + 28;
            if (cx + cw > chat.right() - 20) { cx = chipX; ++rows; }
            chipRects.push_back({cx, static_cast<float>(rows), cw, 30});
            cx += cw + 8;
        }
        if (!chips.empty()) ++rows;
    }
    float chipsH = rows ? rows * chipRowH + 6 : 0;
    Rect inputRow{chat.x + 16, chat.bottom() - 62, chat.w - 32, 46};
    chipY = inputRow.y - 8 - chipsH;
    Rect msgs{chat.x + 1, top, chat.w - 2, chipY - top};

    // messages
    float bubbleMax = std::min(msgs.w - 100.0f, 720.0f);
    float contentH = 16;
    for (auto& m : aiChat_) {
        if (m.h < 0 || m.hw != msgs.w) {
            if (m.fromUser) {
                float lw = 0;
                auto ws = Renderer::wrap(m.lines[0].text, std::min(bubbleMax * 0.8f, 520.0f) - 32, 13.5f);
                m.h = ws.size() * 20.0f + 22;
                (void)lw;
            } else {
                m.h = aiLines(m.lines, 0, 0, bubbleMax - 32, false) + 24;
            }
            m.hw = msgs.w;
        }
        contentH += m.h + 14;
    }
    if (aiStick_) { aiScroll_.offset = 1e9f; aiStick_ = false; }
    float y0 = aiScroll_.begin(msgs, contentH + 8);
    float my = y0 + 8;
    for (auto& m : aiChat_) {
        if (m.fromUser) {
            float maxW = std::min(bubbleMax * 0.8f, 520.0f) - 32;
            auto ws = Renderer::wrap(m.lines[0].text, maxW, 13.5f);
            float bw = 0;
            for (const auto& s : ws) bw = std::max(bw, Renderer::measure(s, 13.5f));
            Rect b{msgs.right() - 28 - (bw + 32), my, bw + 32, m.h};
            if (b.bottom() > msgs.y && b.y < msgs.bottom()) {
                Renderer::roundRect(b, 14, P().primary);
                float ty = b.y + 25;
                for (const auto& s : ws) { Renderer::text(b.x + 16, ty - 4, s, 13.5f, hex(0xFFFFFF)); ty += 20; }
            }
        } else if (my + m.h > msgs.y && my < msgs.bottom()) {
            Renderer::circle(msgs.x + 38, my + 16, 15, P().primarySoft);
            icon("ai", msgs.x + 38, my + 16, 14, P().primary);
            Rect b{msgs.x + 62, my, bubbleMax, m.h};
            Renderer::roundRect(b, 14, P().surfaceAlt);
            Renderer::roundRectOutline(b, 14, P().border);
            aiLines(m.lines, b.x + 16, b.y + 12, b.w - 32, true);
            if (m.pending) spinnerDots(b.x + 16 + Renderer::measure("Asking GPT-4", 13) + 28, b.y + 22, P().primary);   // waiting for the answer
        }
        my += m.h + 14;
    }
    aiScroll_.end();

    // chips
    for (std::size_t i = 0; i < chips.size(); ++i) {
        Rect cr{chipRects[i].x, chipY + chipRects[i].y * chipRowH, chipRects[i].w, chipRects[i].h};
        bool hov = u.hover(cr);
        Renderer::roundRect(cr, 15, hov ? P().primarySoft : P().surface);
        Renderer::roundRectOutline(cr, 15, hov ? P().primary : P().border);
        Renderer::textIn(cr, chips[i], 12.5f, hov ? P().primaryText : P().text, Weight::REGULAR, Align::CENTER);
        if (hov) u.wantHand();
        if (u.clicked(cr)) { std::string q = chips[i]; aiSend(q); break; }
    }

    // input
    aiInput_.draw({inputRow.x, inputRow.y, inputRow.w - 110, inputRow.h}, groq_.ready() && groq_.mode == 1 ? "Ask me anything..." : citizen ? "Ask about a complaint, safety, or type a reference like SCMS-2026-000123" : "Ask for priorities, a fix plan, or type a reference like SCMS-2026-000123", "ai");
    if (button({inputRow.right() - 98, inputRow.y, 98, inputRow.h}, "Send", ButtonStyle::PRIMARY, !aiPending_.job, "arrow-right")) {
        std::string q = aiInput_.text();
        aiInput_.field.clear();
        aiSend(q);
    }

    // ------------------------------------------------------------ side card: AI triage
    card(side);
    Renderer::text(side.x + 22, side.y + 34, citizen ? "Needs your attention" : "AI triage", 16, P().text, Weight::SEMIBOLD);
    Renderer::text(side.x + 22, side.y + 53, citizen ? "Your complaints, most urgent first" : "Open complaints ranked by AI priority", 12, P().textMuted);
    if (button({side.right() - 108, side.y + 17, 88, 32}, "Refresh", ButtonStyle::GHOST, true, "refresh")) aiTriageLoaded_ = false;
    Renderer::line(side.x + 1, side.y + 66, side.right() - 1, side.y + 66, P().border);
    float sy = side.y + 78;
    {
        struct Chip { const char* label; int value; Color fg; };
        Chip chipsInfo[] = {{"open", aiTriage_.open, P().text}, {"past SLA", aiTriage_.overdue, P().danger}, {"near SLA", aiTriage_.atRisk, P().warning}, {"critical", aiTriage_.critical, P().danger}};
        float cw = (side.w - 44 - 3 * 8) / 4;
        for (int i = 0; i < 4; ++i) {
            Rect t{side.x + 22 + i * (cw + 8), sy, cw, 50};
            Renderer::roundRect(t, 8, P().surfaceAlt);
            Renderer::textIn({t.x, t.y + 6, t.w, 26}, num(chipsInfo[i].value), 18, chipsInfo[i].value ? chipsInfo[i].fg : P().textMuted, Weight::BOLD, Align::CENTER);
            Renderer::textIn({t.x, t.y + 30, t.w, 16}, chipsInfo[i].label, 10.5f, P().textMuted, Weight::REGULAR, Align::CENTER);
        }
    }
    sy += 62;
    if (aiTriage_.items.empty()) {
        emptyState({side.x, sy, side.w, side.bottom() - sy - 40}, citizen ? "No open complaints" : "Nothing needs attention", citizen ? "Report a problem and I will keep track of it" : "Your queue is empty");
    } else {
        Rect list{side.x + 1, sy, side.w - 2, side.bottom() - sy - 40};
        const float rowH = 70;
        float ly = aiSideScroll_.begin(list, aiTriage_.items.size() * rowH);
        for (std::size_t i = 0; i < aiTriage_.items.size(); ++i) {
            const ai::TriageItem& it = aiTriage_.items[i];
            Rect row{list.x + 10, ly + i * rowH, list.w - 24, rowH - 4};
            if (row.bottom() < list.y || row.y > list.bottom()) continue;
            bool inList = list.contains(u.mouse.x, u.mouse.y);
            Rect ask{row.right() - 58, row.y + 30, 46, 22};
            bool hov = u.hover(row) && inList;
            bool hovAsk = u.hover(ask) && inList;
            if (hov) { Renderer::roundRect(row, 8, P().hover); u.wantHand(); }
            Color fg, bg;
            labelColors(it.label, fg, bg);
            Renderer::text(row.x + 12, row.y + 22, it.reference, 13, P().text, Weight::SEMIBOLD);
            std::string lab = it.label + " " + num(it.priority);
            float bw = Renderer::measure(lab, 11, Weight::SEMIBOLD) + 16;
            badge(row.right() - bw - 6, row.y + 16, lab, fg, bg, 11);
            Renderer::textIn({row.x + 12, row.y + 28, row.w - 80, 20}, it.category + "  \xC2\xB7  " + it.city + "  \xC2\xB7  " + displayName(it.status), 12, P().textMuted);
            Renderer::textIn({row.x + 12, row.y + 46, row.w - 20, 18}, it.reason, 11.5f, P().textFaint);
            Renderer::roundRect(ask, 11, hovAsk ? P().primary : P().primarySoft);
            Renderer::textIn(ask, "Ask", 11.5f, hovAsk ? hex(0xFFFFFF) : P().primaryText, Weight::SEMIBOLD, Align::CENTER);
            if (hovAsk) u.wantHand();
            if (inList && u.clicked(ask)) {
                aiAsk(it.id, citizen ? "What happens next?" : "What should I do next?");
                aiSideScroll_.end();
                return;
            }
            if (hov && !hovAsk && u.clicked(row)) {
                backFromDetail_ = Screen::ASSISTANT;
                long long id = it.id;
                aiSideScroll_.end();
                openDetail(id);
                return;
            }
        }
        aiSideScroll_.end();
    }
    Renderer::textIn({side.x + 22, side.bottom() - 34, side.w - 44, 20},
                     aiTriage_.sort.n ? "Ranked with " + aiTriage_.sort.algorithm + "  \xC2\xB7  " + num(aiTriage_.sort.comparisons) + " comparisons" : "Ranked with Merge Sort",
                     11, P().textFaint);

}
