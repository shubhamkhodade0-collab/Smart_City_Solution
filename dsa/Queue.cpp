#include "Queue.h"
#include "../backend/Exceptions.h"

namespace scms {

ComplaintQueue::~ComplaintQueue() { clear(); }

void ComplaintQueue::clear() {
    while (front_) {
        Node* n = front_->next;
        delete front_;
        front_ = n;
    }
    rear_ = nullptr;
    size_ = 0;
}

void ComplaintQueue::enqueue(const QueueItem& item) {
    Node* n = new Node{item, nullptr};
    if (rear_) rear_->next = n; else front_ = n;
    rear_ = n;
    ++size_;
    ++enqueued_;
}

QueueItem ComplaintQueue::dequeue() {
    if (!front_) throw EmptyStructureException("The processing queue is empty - there is no pending complaint to assign.");
    Node* n = front_;
    QueueItem out = n->item;
    front_ = n->next;
    if (!front_) rear_ = nullptr;
    delete n;
    --size_;
    ++dequeued_;
    return out;
}

const QueueItem& ComplaintQueue::front() const {
    if (!front_) throw EmptyStructureException("The processing queue is empty.");
    return front_->item;
}

bool ComplaintQueue::contains(long long complaintId) const {
    for (Node* n = front_; n; n = n->next)
        if (n->item.complaintId == complaintId) return true;
    return false;
}

std::vector<QueueItem> ComplaintQueue::snapshot(std::size_t max) const {
    std::vector<QueueItem> v;
    for (Node* n = front_; n && v.size() < max; n = n->next) v.push_back(n->item);
    return v;
}

} // namespace scms
