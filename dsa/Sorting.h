// =====================================================================
// Sorting.h - hand-written sorting algorithms with instrumentation
// (std::sort is never used for any ranking shown in the UI)
//
//   Bubble Sort   O(n^2)      - department workload ranking (6 departments)
//   Quick Sort    O(n log n)  - city ranking (all cities from PostgreSQL)
//   Merge Sort    O(n log n)  - stable complaint table ordering (severity / date)
//   Insertion Sort O(n^2)     - district list of the selected state (nearly sorted, small)
//
// [CO-EVIDENCE] DSA-CO3 / PROG-CO4: algorithms + comparisons/swaps/time metrics
// =====================================================================
#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace scms {

struct SortMetrics {
    std::string algorithm;
    std::size_t n = 0;
    unsigned long long comparisons = 0;
    unsigned long long swaps = 0;     // element moves for merge sort
    double microseconds = 0.0;
    std::string complexity;
};

namespace detail {
struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double us() const {
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    }
};
template <class T> void swapItems(T& a, T& b, SortMetrics& m) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
    ++m.swaps;
}
} // namespace detail

// less(a, b) == true  ->  a must come before b
template <class T, class Less>
SortMetrics bubbleSort(T* a, std::size_t n, Less less) {
    SortMetrics m{"Bubble Sort", n, 0, 0, 0.0, "O(n^2)"};
    detail::Timer t;
    for (std::size_t pass = 0; pass + 1 < n; ++pass) {
        bool swapped = false;
        for (std::size_t j = 0; j + 1 < n - pass; ++j) {
            ++m.comparisons;
            if (less(a[j + 1], a[j])) {
                detail::swapItems(a[j], a[j + 1], m);
                swapped = true;
            }
        }
        if (!swapped) break;   // already sorted -> early exit (best case O(n))
    }
    m.microseconds = t.us();
    return m;
}

namespace detail {
template <class T, class Less>
std::size_t partition(T* a, std::size_t lo, std::size_t hi, Less& less, SortMetrics& m) {
    // median-of-three pivot moved to a[hi] (avoids O(n^2) on already-ranked data)
    std::size_t mid = lo + (hi - lo) / 2;
    ++m.comparisons; if (less(a[mid], a[lo])) swapItems(a[mid], a[lo], m);
    ++m.comparisons; if (less(a[hi], a[lo])) swapItems(a[hi], a[lo], m);
    ++m.comparisons; if (less(a[mid], a[hi])) swapItems(a[mid], a[hi], m);
    T& pivot = a[hi];
    std::size_t i = lo;
    for (std::size_t j = lo; j < hi; ++j) {
        ++m.comparisons;
        if (less(a[j], pivot)) {
            if (i != j) swapItems(a[i], a[j], m);
            ++i;
        }
    }
    if (i != hi) swapItems(a[i], a[hi], m);
    return i;
}
template <class T, class Less>
void quickSortRec(T* a, std::size_t lo, std::size_t hi, Less& less, SortMetrics& m) {
    while (lo < hi) {
        std::size_t p = partition(a, lo, hi, less, m);
        // recurse into the smaller side, loop on the larger (bounded stack depth)
        if (p - lo < hi - p) {
            if (p > lo) quickSortRec(a, lo, p - 1, less, m);
            lo = p + 1;
        } else {
            quickSortRec(a, p + 1, hi, less, m);
            if (p == 0) break;
            hi = p - 1;
        }
    }
}
} // namespace detail

template <class T, class Less>
SortMetrics quickSort(T* a, std::size_t n, Less less) {
    SortMetrics m{"Quick Sort", n, 0, 0, 0.0, "O(n log n) avg"};
    detail::Timer t;
    if (n > 1) detail::quickSortRec(a, 0, n - 1, less, m);
    m.microseconds = t.us();
    return m;
}

template <class T, class Less>
SortMetrics mergeSort(std::vector<T>& v, Less less) {
    SortMetrics m{"Merge Sort", v.size(), 0, 0, 0.0, "O(n log n), stable"};
    detail::Timer t;
    std::vector<T> buf(v.size());
    for (std::size_t width = 1; width < v.size(); width *= 2) {          // bottom-up
        for (std::size_t lo = 0; lo < v.size(); lo += 2 * width) {
            std::size_t mid = std::min(lo + width, v.size()), hi = std::min(lo + 2 * width, v.size());
            std::size_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                ++m.comparisons;
                if (less(v[j], v[i])) buf[k++] = v[j++];   // strict: equal keys keep order -> stable
                else buf[k++] = v[i++];
                ++m.swaps;
            }
            while (i < mid) { buf[k++] = v[i++]; ++m.swaps; }
            while (j < hi) { buf[k++] = v[j++]; ++m.swaps; }
        }
        v.swap(buf);
    }
    m.microseconds = t.us();
    return m;
}

template <class T, class Less>
SortMetrics insertionSort(T* a, std::size_t n, Less less) {
    SortMetrics m{"Insertion Sort", n, 0, 0, 0.0, "O(n^2), O(n) nearly sorted"};
    detail::Timer t;
    for (std::size_t i = 1; i < n; ++i) {
        T key = std::move(a[i]);
        std::size_t j = i;
        while (j > 0) {
            ++m.comparisons;
            if (!less(key, a[j - 1])) break;
            a[j] = std::move(a[j - 1]);
            ++m.swaps;
            --j;
        }
        a[j] = std::move(key);
    }
    m.microseconds = t.us();
    return m;
}

std::string describe(const SortMetrics& m);   // one-line summary for logs/UI

} // namespace scms
