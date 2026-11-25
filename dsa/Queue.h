// =====================================================================
// Queue.h - linked FIFO queue of complaints waiting for department assignment
//
// Scenario solved (Programming Lab Task 3 / DSA-CO2):
//   complaints must be routed strictly in arrival order (fairness: first come,
//   first served). New complaint -> enqueue at rear; the processor dequeues
//   from the front, determines the category, finds the Department object and
//   assigns it. Boundary cases: dequeue/front on an empty queue throw
//   EmptyStructureException.
//
// [CO-EVIDENCE] PROG-CO3 / DSA-CO2: FIFO enqueue / dequeue / front / isEmpty / size
// =====================================================================
#pragma once
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace scms {

struct QueueItem {
    long long complaintId = 0;
    std::string reference;
    std::string categoryCode;
    int severity = 0;
    std::time_t enqueuedAt = 0;
};

class ComplaintQueue {
public:
    ComplaintQueue() = default;
    ~ComplaintQueue();
    ComplaintQueue(const ComplaintQueue&) = delete;
    ComplaintQueue& operator=(const ComplaintQueue&) = delete;

    void enqueue(const QueueItem& item);     // O(1) at rear
    QueueItem dequeue();                     // O(1) from front, throws when empty
    const QueueItem& front() const;          // throws when empty
    bool isEmpty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    bool contains(long long complaintId) const;
    std::vector<QueueItem> snapshot(std::size_t max = 50) const;   // front -> rear, for the UI
    void clear();

    unsigned long long totalEnqueued() const { return enqueued_; }
    unsigned long long totalDequeued() const { return dequeued_; }

private:
    struct Node {
        QueueItem item;
        Node* next;
    };
    Node* front_ = nullptr;
    Node* rear_ = nullptr;
    std::size_t size_ = 0;
    unsigned long long enqueued_ = 0, dequeued_ = 0;
};

} // namespace scms
