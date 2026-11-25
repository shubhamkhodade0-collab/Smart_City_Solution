#include "CityArray.h"
#include "../backend/Exceptions.h"

namespace scms {

CityArray::CityArray() : items_(new CityStat[MAX_CITIES]) {}
CityArray::~CityArray() { delete[] items_; }

void CityArray::insert(const CityStat& s) {
    if (size_ >= MAX_CITIES) throw CapacityException("CityArray is full (" + std::to_string(MAX_CITIES) + ")");
    items_[size_++] = s;
}

int CityArray::indexOf(int cityId) const {
    for (std::size_t i = 0; i < size_; ++i)
        if (items_[i].cityId == cityId) return static_cast<int>(i);
    return -1;
}

bool CityArray::update(int cityId, const CityStat& s) {
    int i = indexOf(cityId);
    if (i < 0) return false;
    items_[i] = s;
    return true;
}

bool CityArray::remove(int cityId) {
    int i = indexOf(cityId);
    if (i < 0) return false;
    for (std::size_t k = static_cast<std::size_t>(i) + 1; k < size_; ++k) items_[k - 1] = items_[k];
    --size_;
    return true;
}

CityStat& CityArray::at(std::size_t i) {
    if (i >= size_) throw NotFoundException("CityArray index out of range");
    return items_[i];
}

const CityStat& CityArray::at(std::size_t i) const {
    if (i >= size_) throw NotFoundException("CityArray index out of range");
    return items_[i];
}

long long CityArray::totalComplaints() const {
    long long t = 0;
    for (std::size_t i = 0; i < size_; ++i) t += items_[i].complaints;
    return t;
}

void StatGrid::clear() {
    for (auto& row : cells_)
        for (int& c : row) c = 0;
}

void StatGrid::set(int s, int c, int v) {
    if (s < 0 || s >= MAX_STATES || c < 0 || c >= MAX_CATEGORIES) return;
    cells_[s][c] = v;
}

int StatGrid::get(int s, int c) const {
    if (s < 0 || s >= MAX_STATES || c < 0 || c >= MAX_CATEGORIES) return 0;
    return cells_[s][c];
}

int StatGrid::rowTotal(int s) const {
    int t = 0;
    for (int c = 0; c < MAX_CATEGORIES; ++c) t += get(s, c);
    return t;
}

int StatGrid::columnTotal(int c) const {
    int t = 0;
    for (int s = 0; s < MAX_STATES; ++s) t += get(s, c);
    return t;
}

int StatGrid::maxCell() const {
    int m = 0;
    for (const auto& row : cells_)
        for (int v : row) m = v > m ? v : m;
    return m;
}

} // namespace scms
