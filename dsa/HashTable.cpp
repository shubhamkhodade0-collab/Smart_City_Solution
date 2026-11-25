#include "HashTable.h"
#include "../backend/AsmEngine.h"
#include <cctype>

namespace scms {

static bool keyEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

HashTable::HashTable(std::size_t buckets) : buckets_(nextPrime(buckets < 3 ? 3 : buckets), nullptr) {}

HashTable::~HashTable() { clear(); }

void HashTable::clear() {
    for (auto& head : buckets_) {
        while (head) {
            Entry* n = head->next;
            delete head;
            head = n;
        }
    }
    count_ = collisions_ = 0;
}

std::size_t HashTable::bucketOf(const std::string& key) const {
    return static_cast<std::size_t>(AsmEngine::hash(key, buckets_.size()));   // NASM djb2
}

bool HashTable::insert(const std::string& key, Complaint* value) {
    if (search(key)) return false;
    if (static_cast<double>(count_ + 1) / buckets_.size() > 0.75) rehash(nextPrime(buckets_.size() * 2 + 1));
    std::size_t b = bucketOf(key);
    if (buckets_[b]) ++collisions_;                  // bucket already occupied -> collision, chained
    buckets_[b] = new Entry{key, value, buckets_[b]};  // push at chain head
    ++count_;
    return true;
}

Complaint* HashTable::search(const std::string& key) const {
    lastProbes_ = 0;
    for (Entry* e = buckets_[bucketOf(key)]; e; e = e->next) {
        ++lastProbes_;
        if (keyEquals(e->key, key)) {
            ++hits_;
            return e->value;
        }
    }
    ++misses_;
    return nullptr;
}

bool HashTable::update(const std::string& key, Complaint* value) {
    for (Entry* e = buckets_[bucketOf(key)]; e; e = e->next)
        if (keyEquals(e->key, key)) {
            e->value = value;
            return true;
        }
    return false;
}

bool HashTable::remove(const std::string& key) {
    Entry** link = &buckets_[bucketOf(key)];
    while (*link) {
        if (keyEquals((*link)->key, key)) {
            Entry* dead = *link;
            *link = dead->next;
            delete dead;
            --count_;
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

void HashTable::rehash(std::size_t newBuckets) {
    std::vector<Entry*> old;
    old.swap(buckets_);
    buckets_.assign(newBuckets, nullptr);
    collisions_ = 0;
    ++rehashes_;
    for (Entry* head : old) {
        while (head) {
            Entry* next = head->next;
            std::size_t b = bucketOf(head->key);
            if (buckets_[b]) ++collisions_;
            head->next = buckets_[b];
            buckets_[b] = head;
            head = next;
        }
    }
}

std::size_t HashTable::nextPrime(std::size_t n) {
    auto isPrime = [](std::size_t x) {
        if (x < 2) return false;
        for (std::size_t d = 2; d * d <= x; ++d)
            if (x % d == 0) return false;
        return true;
    };
    while (!isPrime(n)) ++n;
    return n;
}

std::vector<std::size_t> HashTable::chainLengths() const {
    std::vector<std::size_t> v(buckets_.size(), 0);
    for (std::size_t i = 0; i < buckets_.size(); ++i)
        for (Entry* e = buckets_[i]; e; e = e->next) ++v[i];
    return v;
}

HashTable::Stats HashTable::stats() const {
    Stats s{};
    s.records = count_;
    s.buckets = buckets_.size();
    s.collisions = collisions_;
    for (std::size_t len : chainLengths()) {
        if (len) ++s.usedBuckets;
        if (len > s.longestChain) s.longestChain = len;
    }
    s.loadFactor = buckets_.empty() ? 0.0 : static_cast<double>(count_) / buckets_.size();
    s.successfulSearches = hits_;
    s.failedSearches = misses_;
    s.lastProbes = lastProbes_;
    s.rehashes = rehashes_;
    return s;
}

} // namespace scms
