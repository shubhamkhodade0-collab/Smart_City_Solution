#include "Sorting.h"
#include <cstdio>

namespace scms {

std::string describe(const SortMetrics& m) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "%s  n=%zu  comparisons=%llu  swaps=%llu  time=%.1f us  %s",
                  m.algorithm.c_str(), m.n, m.comparisons, m.swaps, m.microseconds, m.complexity.c_str());
    return buf;
}

} // namespace scms
