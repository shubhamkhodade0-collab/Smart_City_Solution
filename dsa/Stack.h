// =====================================================================
// Stack.h - array-based LIFO stack with fixed capacity
//
// Used for:
//   1. STATUS HISTORY STACK of the open complaint (PENDING at bottom, latest on TOP)
//   2. RECENT ADMIN ACTIONS stack (latest action on top)
// Boundary cases: pop/peek on empty -> EmptyStructureException,
//                 push on full      -> CapacityException (overflow).
//
// [CO-EVIDENCE] PROG-CO3 / DSA-CO2: push / pop / peek / isEmpty / isFull / size / capacity
// =====================================================================
#pragma once
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace scms {

struct StackEntry {
    std::string label;      // e.g. "IN_PROGRESS" or "Assigned SCMS-2026-001042"
    std::string detail;     // who / note
    std::time_t at = 0;
};

class Stack {
public:
    explicit Stack(std::size_t capacity);
    ~Stack();
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    void push(const StackEntry& e);          // throws CapacityException when full
    void pushBounded(const StackEntry& e);   // for an audit trail: discards the bottom when full
    StackEntry pop();                        // throws EmptyStructureException
    const StackEntry& peek() const;          // throws EmptyStructureException
    bool isEmpty() const { return top_ == 0; }
    bool isFull() const { return top_ == capacity_; }
    std::size_t size() const { return top_; }
    std::size_t capacity() const { return capacity_; }
    std::vector<StackEntry> topToBottom() const;
    void clear() { top_ = 0; }

private:
    StackEntry* data_;      // raw array, managed by constructor/destructor
    std::size_t capacity_;
    std::size_t top_ = 0;   // index of the next free slot
};

} // namespace scms
