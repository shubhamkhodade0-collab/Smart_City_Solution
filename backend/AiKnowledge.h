// =====================================================================
// AiKnowledge.h - curated municipal playbooks used by the AI assistant
//
// A playbook describes ONE kind of civic problem (for example "No or low water
// supply"): the words people use to describe it, the likely causes, what a
// department should do in the first hours, how to fix it, how to verify the fix,
// and what a citizen can safely do while waiting.
//
// The assistant is a local, rule-based expert system: this file is its
// knowledge base. Nothing here talks to the network and nothing is generated
// at random - the same complaint always receives the same advice.
// =====================================================================
#pragma once
#include <string>
#include <vector>

namespace scms {
namespace ai {

struct Playbook {
    std::string id;                          // e.g. "WATER_NO_SUPPLY"
    std::string category;                    // categories.code, e.g. "WATER"
    std::string title;                       // "No or low water supply"
    std::vector<std::string> keywords;       // lower-case words / phrases; trailing '*' = prefix match, leading '!' = distinctive (double weight)
    std::string explanation;                 // one sentence: what this usually is
    std::vector<std::string> causes;         // probable causes to check first
    std::vector<std::string> immediate;      // department: first hours
    std::vector<std::string> fieldWork;      // department: the actual fix
    std::vector<std::string> verify;         // department: checks before marking RESOLVED
    std::vector<std::string> prevention;     // department: stop it coming back
    std::vector<std::string> resources;      // crew / equipment to line up
    std::vector<std::string> citizenSafety;  // citizen: staying safe until it is fixed
    std::vector<std::string> citizenTips;    // citizen: how to help the fix along
    std::string resolvedNote;                // wording for the "resolved" note shown to the citizen
    int defaultImpact = 3;                   // 1..5 suggested when nothing else is known
    int defaultUrgency = 3;                  // 1..5
    bool emergency = false;                  // true -> advise 112 when someone is in danger
    bool general = false;                    // catch-all playbook of a category
};

// every playbook (6 categories x several sub-issues + one general playbook per category)
const std::vector<Playbook>& playbooks();

const Playbook* playbookById(const std::string& id);

// the catch-all playbook of a category (never null for the six known categories)
const Playbook* generalPlaybook(const std::string& categoryCode);

} // namespace ai
} // namespace scms
