// =====================================================================
// HashTable.h - separate-chaining hash table: key (reference code / id) -> Complaint*
//
// * bucket index = asm_hash_djb2(key) mod bucketCount   (NASM, case-insensitive)
// * collisions resolved by chaining (singly linked list per bucket)
// * automatic rehash to the next prime > 2n when load factor exceeds 0.75
//
// [CO-EVIDENCE] DSA-CO2 / PROG-CO5: hashing with collision handling; insert/search/update/delete
// =====================================================================
#pragma once
#include "../backend/Complaint.h"
#include <cstddef>
#include <string>
#include <vector>

namespace scms {

class HashTable {
public:
    struct Stats {
        std::size_t records, buckets, usedBuckets, collisions, longestChain;
        double loadFactor;
        unsigned long long successfulSearches, failedSearches, lastProbes, rehashes;
    };

    explicit HashTable(std::size_t buckets = 101);
    ~HashTable();
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    bool insert(const std::string& key, Complaint* value);   // false if key already exists
    Complaint* search(const std::string& key) const;
    bool update(const std::string& key, Complaint* value);
    bool remove(const std::string& key);
    void clear();

    std::size_t size() const { return count_; }
    std::size_t bucketOf(const std::string& key) const;
    Stats stats() const;
    std::vector<std::size_t> chainLengths() const;            // for the UI histogram

private:
    struct Entry {
        std::string key;
        Complaint* value;
        Entry* next;
    };
    void rehash(std::size_t newBuckets);
    static std::size_t nextPrime(std::size_t n);

    std::vector<Entry*> buckets_;
    std::size_t count_ = 0;
    std::size_t collisions_ = 0;
    unsigned long long rehashes_ = 0;
    mutable unsigned long long hits_ = 0, misses_ = 0, lastProbes_ = 0;
};

} // namespace scms
