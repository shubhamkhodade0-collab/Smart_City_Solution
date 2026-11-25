#include "AsmEngine.h"

namespace scms {

unsigned long long AsmEngine::severityCalls_ = 0;
unsigned long long AsmEngine::classifyCalls_ = 0;
unsigned long long AsmEngine::multiplyCalls_ = 0;
unsigned long long AsmEngine::containsCalls_ = 0;
unsigned long long AsmEngine::hashCalls_ = 0;
unsigned long long AsmEngine::stringCalls_ = 0;

// [CO-EVIDENCE] COA-CO2: C++ calls the NASM severity routine (RCX=category, RDX=impact, R8=urgency, RAX=score)
int AsmEngine::severityScore(int categoryCode, int impact, int urgency) {
    ++severityCalls_;
    return static_cast<int>(calculateSeverityAsm(categoryCode, impact, urgency));
}

int AsmEngine::classify(int score) {
    ++classifyCalls_;
    return static_cast<int>(classifySeverityAsm(score));
}

unsigned long long AsmEngine::multiplyShiftAdd(unsigned long long a, unsigned long long b) {
    ++multiplyCalls_;
    return asm_multiply_shift_add(a, b);
}

unsigned long long AsmEngine::multiplySuccessive(unsigned long long a, unsigned long long b) {
    ++multiplyCalls_;
    return asm_multiply_successive(a, b);
}

bool AsmEngine::containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    ++containsCalls_;
    return asm_contains_ci(haystack.c_str(), needle.c_str()) != 0;
}

unsigned long long AsmEngine::hash(const std::string& key, unsigned long long buckets) {
    ++hashCalls_;
    return asm_hash_djb2(key.c_str(), buckets);
}

std::size_t AsmEngine::length(const std::string& s) {
    ++stringCalls_;
    return static_cast<std::size_t>(asm_strlen(s.c_str()));
}

std::string AsmEngine::toLower(std::string s) {
    ++stringCalls_;
    asm_to_lower(s.data());   // in place; std::string storage is contiguous and NUL-terminated
    return s;
}

AsmEngine::Stats AsmEngine::stats() {
    return {severityCalls_, classifyCalls_, multiplyCalls_, containsCalls_, hashCalls_, stringCalls_};
}

unsigned long long AsmEngine::totalCalls() {
    return severityCalls_ + classifyCalls_ + multiplyCalls_ + containsCalls_ + hashCalls_ + stringCalls_;
}

bool AsmEngine::selfTest() {
    // known answers computed by hand from the documented formulas
    return asm_multiply_shift_add(13, 11) == 143 &&
           asm_multiply_successive(13, 11) == 143 &&
           calculateSeverityAsm(2, 5, 5) == 97 &&      // 22 + 3*25
           calculateSeverityAsm(1, 1, 1) == 13 &&      // 10 + 3*1
           classifySeverityAsm(97) == 4 && classifySeverityAsm(13) == 1 &&
           asm_contains_ci("Pothole near Raigad fort", "RAIGAD") == 1 &&
           asm_contains_ci("abc", "abd") == 0 &&
           asm_strlen("Raigad") == 6;
}

} // namespace scms
