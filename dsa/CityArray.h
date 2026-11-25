// =====================================================================
// CityArray.h - contiguous array of per-city statistics + 2-D state x category grid
//
// Filled ONLY from PostgreSQL (Database::loadCityStats / loadStateCategoryGrid).
// The array is what Quick Sort ranks and what the map/dashboard read.
//
// [CO-EVIDENCE] PROG-CO1 / DSA-CO2: array insert / update / search / display;
//               2-D array (grid) indexed [state][category]
// =====================================================================
#pragma once
#include <cstddef>
#include <string>

namespace scms {

struct CityStat {
    int cityId = 0, districtId = 0, stateId = 0;
    std::string city, district, state;
    int complaints = 0, pending = 0, resolved = 0, highSeverity = 0;
};

class CityArray {
public:
    static constexpr std::size_t MAX_CITIES = 2048;

    CityArray();
    ~CityArray();
    CityArray(const CityArray&) = delete;
    CityArray& operator=(const CityArray&) = delete;

    void clear() { size_ = 0; }
    void insert(const CityStat& s);                    // append, throws CapacityException
    bool update(int cityId, const CityStat& s);
    bool remove(int cityId);                           // shifts left, O(n)
    int indexOf(int cityId) const;                     // linear search
    CityStat& at(std::size_t i);
    const CityStat& at(std::size_t i) const;
    CityStat* data() { return items_; }
    const CityStat* data() const { return items_; }
    std::size_t size() const { return size_; }
    long long totalComplaints() const;

private:
    CityStat* items_;          // fixed-size heap array
    std::size_t size_ = 0;
};

// state x category counts used by the 3-D chart and the category report
class StatGrid {
public:
    static constexpr int MAX_STATES = 40;
    static constexpr int MAX_CATEGORIES = 8;
    void clear();
    void set(int stateIndex, int categoryIndex, int value);
    int get(int stateIndex, int categoryIndex) const;
    int rowTotal(int stateIndex) const;
    int columnTotal(int categoryIndex) const;
    int maxCell() const;

private:
    int cells_[MAX_STATES][MAX_CATEGORIES] = {};
};

} // namespace scms
