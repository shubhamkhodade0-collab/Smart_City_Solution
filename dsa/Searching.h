// =====================================================================
// Searching.h - Linear Search and Binary Search with comparison counters
//
// Linear search : unsorted complaint list (match predicate, Assembly substring scan)
// Binary search : city array AFTER it is sorted by name (Merge Sort) - "Search City"
//
// [CO-EVIDENCE] DSA-CO3 / PROG-CO4: searching algorithms with metrics
// =====================================================================
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace scms {

struct SearchMetrics {
    std::string algorithm;
    std::string target;
    long long resultIndex = -1;          // -1 = not found
    std::size_t matches = 0;             // linear search may collect several
    unsigned long long comparisons = 0;
    std::size_t n = 0;
};

// collects every index whose element satisfies pred
template <class T, class Pred>
SearchMetrics linearSearch(const std::vector<T>& v, const std::string& target, Pred pred,
                           std::vector<std::size_t>& found) {
    SearchMetrics m{"Linear Search", target, -1, 0, 0, v.size()};
    for (std::size_t i = 0; i < v.size(); ++i) {
        ++m.comparisons;
        if (pred(v[i])) {
            if (m.resultIndex < 0) m.resultIndex = static_cast<long long>(i);
            found.push_back(i);
        }
    }
    m.matches = found.size();
    return m;
}

// cmp(element) < 0  : element is before the target,  0 : match,  > 0 : after
template <class T, class Cmp>
SearchMetrics binarySearch(const T* a, std::size_t n, const std::string& target, Cmp cmp) {
    SearchMetrics m{"Binary Search", target, -1, 0, 0, n};
    std::size_t lo = 0, hi = n;     // half-open [lo, hi)
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        ++m.comparisons;
        int c = cmp(a[mid]);
        if (c == 0) {
            m.resultIndex = static_cast<long long>(mid);
            m.matches = 1;
            break;
        }
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return m;
}

int compareIgnoreCase(const std::string& a, const std::string& b);
bool startsWithIgnoreCase(const std::string& s, const std::string& prefix);

} // namespace scms
