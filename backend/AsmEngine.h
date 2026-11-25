// =====================================================================
// AsmEngine.h - the C++ <-> NASM boundary
//
// The extern "C" block below is the C++ declaration of every Assembly routine.
// extern "C" disables C++ name mangling so the linker resolves the plain NASM labels
// (e.g. "calculateSeverityAsm") exported with `global` in assembly/*.asm.
// Under the Win64 ABI: arg1 -> RCX, arg2 -> RDX, arg3 -> R8, return <- RAX.
// =====================================================================
#pragma once
#include <cstdint>
#include <string>

extern "C" {
    // assembly/severity.asm
    unsigned long long asm_multiply_shift_add(unsigned long long a, unsigned long long b);
    unsigned long long asm_multiply_successive(unsigned long long a, unsigned long long b);
    long long calculateSeverityAsm(long long categoryCode, long long impact, long long urgency);
    long long classifySeverityAsm(long long score);
    // assembly/strops.asm
    unsigned long long asm_strlen(const char* s);
    unsigned long long asm_to_lower(char* s);
    long long asm_contains_ci(const char* haystack, const char* needle);
    unsigned long long asm_hash_djb2(const char* key, unsigned long long buckets);
}

namespace scms {

// Static facade: counts every genuine Assembly call so the UI can prove execution.
// [CO-EVIDENCE] OOPS-CO2: static data members shared by all callers (class-level state).
class AsmEngine {
public:
    struct Stats {
        unsigned long long severityCalls, classifyCalls, multiplyCalls,
                           containsCalls, hashCalls, stringCalls;
    };

    static int severityScore(int categoryCode, int impact, int urgency);
    static int classify(int score);                              // 1..4
    static unsigned long long multiplyShiftAdd(unsigned long long a, unsigned long long b);
    static unsigned long long multiplySuccessive(unsigned long long a, unsigned long long b);
    static bool containsIgnoreCase(const std::string& haystack, const std::string& needle);
    static unsigned long long hash(const std::string& key, unsigned long long buckets);
    static std::size_t length(const std::string& s);
    static std::string toLower(std::string s);

    static Stats stats();
    static unsigned long long totalCalls();
    static bool selfTest();      // verifies the routines at start-up -> "ONLINE"/"FAULT"

private:
    AsmEngine() = delete;        // pure static utility class
    static unsigned long long severityCalls_, classifyCalls_, multiplyCalls_,
                              containsCalls_, hashCalls_, stringCalls_;
};

} // namespace scms
